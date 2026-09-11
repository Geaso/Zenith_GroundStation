#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"
#include "TelemetryStore.h"
#include "CommandDispatcher.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <QRegularExpression>
#include <cmath>
#include <optional>

// Default traffic is handshake/heartbeat and read-only PARAMSETTINGS.
// Task/goal requests require explicit CLI options and a fresh disarmed FCU.
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ZenithMavlinkProbe");
    QCommandLineParser parser;
    parser.setApplicationDescription("Production MAVLink2 TCP or already-paired serial telemetry probe. Default traffic: handshake, heartbeat and read-only parameters. Serial LR24 verification never writes pairing settings.");
    // Qt's automatic help/error exit path can use a Windows message box when
    // the launcher redirects handles instead of attaching an interactive
    // console. Keep this tool's output entirely on stdout/stderr.
    parser.addOption(QCommandLineOption(QStringList{"h", "help", "help-all", "?"},
                                        "Display this help and exit."));
    parser.addPositionalArgument("endpoint", "TCP: <ip> <port> <uav_id>. Serial: --serial COM5 --baud 921600 --uav-id <logical_id>.", "[ip port uav_id]");
    parser.addOptions({
        {"timeout-ms", "Stop and print diagnostics after this duration.", "milliseconds", "15000"},
        {"serial", "Use this already-paired serial port, with read-only LR24 address verification.", "port"},
        {"baud", "Serial baud rate (requires --serial).", "rate", "921600"},
        {"uav-id", "Explicit logical MAVLink target for serial mode; independent of the radio address.", "id"},
        {"skip-params", "Skip the default read-only parameter request."},
        {"task-query", "Explicitly send a managed-task STATUS request using the production dispatcher."},
        {"task-start", "Explicitly START this managed task (yaw enable remains false).", "name"},
        {"task-stop", "Explicitly STOP this managed task.", "name"},
        {"task-path", "Absolute custom task path, only with --task-start.", "path"},
        {"wait-task-state", "Require this lifecycle state after the requested task action.", "state"},
        {"super-goal", "Explicitly publish one typed SUPER goal in world using the production dispatcher.", "x,y,z"},
        {"observe-ms", "Keep receiving for at least this duration after the explicit action.", "milliseconds", "1000"},
        {"fcu-id", "Require this FCU system ID in the companion association.", "id"},
        {"param-name", "Read this ROS parameter using SEARCH (5); otherwise query communication parameters (2).", "name"},
        {"expect-position", "Require ENU position x,y,z within 0.001 m.", "x,y,z"},
        {"expect-velocity", "Require ENU velocity x,y,z within 0.001 m/s.", "x,y,z"},
        {"expect-battery", "Require battery voltage within 0.001 V.", "volts"},
        {"expect-mode", "Require this decoded FCU flight mode.", "mode"},
        {"require-custom-data", "Also require a decoded managed-task state and valid preflight report."},
        {"require-voxel-map", "Also require a completely assembled voxel map."},
        {"require-grid-map", "Also require a decoded grid map."},
        {"require-planned-path", "Also require a decoded nonempty planned path."}
    });
    auto badArgument = [](const QString &message) {
        QTextStream(stderr) << message << Qt::endl;
        return 2;
    };
    if (!parser.parse(QCoreApplication::arguments())) return badArgument(parser.errorText());
    if (parser.isSet("help")) {
        QTextStream(stdout) << parser.helpText();
        return 0;
    }
    const auto args = parser.positionalArguments();
    const bool serialMode = parser.isSet("serial");
    if ((!serialMode && (args.size() != 3 || parser.isSet("uav-id") || parser.isSet("baud")))
        || (serialMode && (!args.isEmpty() || !parser.isSet("uav-id") || parser.value("serial").trimmed().isEmpty())))
        return badArgument("Expected TCP: <ip> <port> <uav_id>; serial: --serial COM5 --baud 921600 --uav-id <logical_id>");
    bool portOk = serialMode, idOk = false, timeoutOk = false, fcuOk = true, baudOk = true;
    const QString remoteIp = serialMode ? QString() : args[0];
    const int port = serialMode ? 0 : args[1].toInt(&portOk);
    const int id = (serialMode ? parser.value("uav-id") : args[2]).toInt(&idOk);
    const int baud = parser.value("baud").toInt(&baudOk);
    const int timeout = parser.value("timeout-ms").toInt(&timeoutOk);
    const int expectedFcu = parser.isSet("fcu-id") ? parser.value("fcu-id").toInt(&fcuOk) : 0;
    if ((!serialMode && (QHostAddress(remoteIp).isNull() || !portOk || port < 1 || port > 65535))
        || (serialMode && (!baudOk || baud < 1200 || baud > 4000000))
        || !idOk || id < 1 || id > 254 || !timeoutOk || timeout < 1000 || timeout > 300000
        || !fcuOk || (parser.isSet("fcu-id") && (expectedFcu < 1 || expectedFcu > 255)))
        return badArgument("Invalid IP, port, UAV/FCU ID or timeout (1000..300000 ms).");

    auto expectedVector = [&](const QString &option, bool &ok) -> std::optional<QList<double>> {
        if (!parser.isSet(option)) return std::nullopt;
        const auto text = parser.value(option).split(',');
        if (text.size() != 3) { ok = false; return std::nullopt; }
        QList<double> result;
        for (const auto &part : text) {
            bool numeric = false; const double number = part.toDouble(&numeric);
            if (!numeric || !std::isfinite(number)) { ok = false; return std::nullopt; }
            result.append(number);
        }
        return result;
    };
    bool expectedOk = true, batteryOk = true;
    const auto expectedPosition = expectedVector("expect-position", expectedOk);
    const auto expectedVelocity = expectedVector("expect-velocity", expectedOk);
    const auto plannerGoal = expectedVector("super-goal", expectedOk);
    const double expectedBattery = parser.isSet("expect-battery") ? parser.value("expect-battery").toDouble(&batteryOk) : 0;
    if (!expectedOk || !batteryOk || !std::isfinite(expectedBattery)) return badArgument("Invalid expected position, velocity or voltage.");
    if (parser.isSet("param-name") && !parser.value("param-name").startsWith('/')) return badArgument("--param-name must be an absolute ROS parameter name.");
    const int explicitActions = int(parser.isSet("task-query")) + int(parser.isSet("task-start"))
        + int(parser.isSet("task-stop")) + int(parser.isSet("super-goal"));
    const bool taskRequested = parser.isSet("task-query") || parser.isSet("task-start") || parser.isSet("task-stop");
    const QString taskAction = parser.isSet("task-start") ? "START" : parser.isSet("task-stop") ? "STOP" : "STATUS";
    const QString taskName = parser.isSet("task-start") ? parser.value("task-start") : parser.value("task-stop");
    bool observeOk = false;
    const int observeMs = parser.value("observe-ms").toInt(&observeOk);
    if (explicitActions > 1 || !observeOk || observeMs < 0 || observeMs > 120000
        || (parser.isSet("wait-task-state") && !taskRequested)
        || (parser.isSet("task-path") && !parser.isSet("task-start"))
        || (parser.isSet("skip-params") && parser.isSet("param-name")))
        return badArgument("Choose at most one explicit task/goal action; validate its task-path, wait-task-state, parameter and observe options.");
    if ((parser.isSet("task-start") || parser.isSet("task-stop"))
        && !QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$").match(taskName).hasMatch())
        return badArgument("Invalid task name.");
    const QString taskPath = parser.value("task-path");
    if (parser.isSet("task-path") && (!taskPath.startsWith('/') || taskPath.size() > 512
        || taskPath.contains('\n') || taskPath.contains('\r')))
        return badArgument("Custom task path must be an absolute path of at most 512 characters.");
    if (plannerGoal && (std::abs(plannerGoal->at(0)) > 1000 || std::abs(plannerGoal->at(1)) > 1000
        || plannerGoal->at(2) < -100 || plannerGoal->at(2) > 100))
        return badArgument("SUPER goal exceeds the bridge world-coordinate bounds.");

    ZenithProtocolClient client;
    TelemetryStore store;
    CommandDispatcher dispatcher(&store, &client);
    store.setVehicleName(QString("UAV%1").arg(id));
    client.setRobotId(id);
    if (serialMode) {
        client.setTransportMode(int(TransportMode::Serial));
        client.setSerialPortName(parser.value("serial"));
        client.setSerialBaudRate(baud);
        client.setRadioPairingReadOnly(true);
    } else {
        client.setRemoteHostIp(remoteIp); client.setTcpPort(quint16(port));
    }
    client.setUdpPort(0); // Never share the production UDP telemetry port.
    bool handshakeAck = false, querySent = false, queryAck = false, queryResponse = false;
    bool gotPosition = false, gotVelocity = false, gotAttitude = false, gotTaskState = false, finished = false;
    int fcuId = 0, messages = 0;
    const int paramModule = parser.isSet("param-name") ? 5 : 2;
    QVariantMap lastParamAck, lastParams;
    QJsonArray protocolAcks, taskEvents;
    QString taskRequestId, actionError;
    bool actionSent = false, actionAck = false, taskResponse = false, taskStateSeen = false;
    qint64 actionSentAt = 0;
    QElapsedTimer elapsed; elapsed.start();
    QObject::connect(&client, &ZenithProtocolClient::decodedMessage, &app,
        [&](int kind, int robotId, const QVariantMap &payload) {
        ++messages;
        store.noteFrameReceived(robotId);
        switch (kind) {
        case ZenithProtocol::UAVSTATE:
            if (payload.value("_reset_fcu").toBool()) gotPosition = gotVelocity = gotAttitude = false;
            if (payload.contains("fcu_system_id")) fcuId = payload.value("fcu_system_id").toInt();
            gotPosition |= payload.contains("position");
            gotVelocity |= payload.contains("velocity");
            gotAttitude |= payload.contains("attitude");
            store.applyUavState(payload, robotId); break;
        case ZenithProtocol::HEARTBEAT: store.applyHeartbeat(payload); break;
        case ZenithProtocol::TEXTINFO: store.applyTextInfo(payload); break;
        case ZenithProtocol::UAVCONTROLSTATE: store.applyUavControlState(payload); break;
        case ZenithProtocol::CUSTOMDATASEGMENT_1: {
            QVariantMap fields;
            for (int i = 0; i < payload.value("datas_num").toInt(); ++i) {
                const QString key = payload.value(QString("name[%1]").arg(i)).toString();
                gotTaskState |= key == "task_state";
                fields[key] = payload.value(QString("value[%1]").arg(i));
            }
            store.applyCustomDataSegment(payload);
            if (fields.contains("task_state") || fields.contains("task_ack")) {
                fields["received_ms"] = elapsed.elapsed();
                taskEvents.append(QJsonObject::fromVariantMap(fields));
                if (taskEvents.size() > 100) taskEvents.removeFirst();
            }
            if (actionSent && taskRequested && fields.value("task_request_id").toString() == taskRequestId) {
                const auto ack = fields.value("task_ack").toString();
                taskResponse |= QStringList{"STATUS", "ACCEPTED", "STARTED", "STOPPED", "DUPLICATE"}.contains(ack);
                if (ack == "REJECTED" || ack == "FAILED") actionError = "Task " + ack + ": " + fields.value("task_reason").toString();
                taskStateSeen |= fields.value("task_state").toString() == parser.value("wait-task-state");
            }
            break;
        }
        case ZenithProtocol::VOXELMAP: store.applyVoxelMap(payload, robotId); break;
        case ZenithProtocol::GRIDMAP: store.applyGridMap(payload, robotId); break;
        case ZenithProtocol::PLANNEDPATH: store.applyPlannedPath(payload); break;
        case ZenithProtocol::PROTOCOL_ACK: {
            protocolAcks.append(QJsonObject::fromVariantMap(payload));
            if (protocolAcks.size() > 100) protocolAcks.removeFirst();
            const bool received = payload.value("status") == "RECEIVED" || payload.value("status") == "DUPLICATE";
            if (payload.value("request_kind").toInt() == ZenithProtocol::MODESELECTION) handshakeAck |= received;
            if (querySent && payload.value("request_kind").toInt() == ZenithProtocol::PARAMSETTINGS) {
                lastParamAck = payload; queryAck |= received;
            }
            if (actionSent && payload.value("request_kind").toInt()
                == (taskRequested ? ZenithProtocol::CUSTOMDATASEGMENT_1 : ZenithProtocol::GOAL)) {
                actionAck |= received;
                if (!received) actionError = "Protocol " + payload.value("status").toString();
            }
            break;
        }
        case ZenithProtocol::PARAMSETTINGS:
            if (querySent && payload.value("param_module").toInt() == paramModule) {
                lastParams = payload;
                for (const auto &entry : payload.value("params").toList())
                    if (!parser.isSet("param-name") || entry.toMap().value("param_name").toString() == parser.value("param-name")) queryResponse = true;
            }
            break;
        default: break;
        }
    });
    QObject::connect(&client, &ZenithProtocolClient::linkStatesChanged, &app, [&] {
        store.setTransportHealth(client.telemetryFresh(), client.heartbeatFresh(), client.connectionSummary());
    });
    auto near = [](double a, double b) { return std::abs(a - b) <= 0.001; };
    auto vectorMatches = [&](const std::optional<QList<double>> &expected, double x, double y, double z) {
        return !expected || (near(expected->at(0), x) && near(expected->at(1), y) && near(expected->at(2), z));
    };
    auto checks = [&] {
        return QJsonObject{
            {"handshake_ack", handshakeAck && !client.awaitingModeSelectionAck()},
            {"serial_pairing_read_only", !serialMode || (client.radioPairingReadOnly() && client.radioPairingReady())},
            {"parameter_ack", parser.isSet("skip-params") || queryAck},
            {"parameter_response", parser.isSet("skip-params") || queryResponse},
            {"explicit_action_sent", !explicitActions || actionSent},
            {"explicit_action_ack", !explicitActions || actionAck},
            {"task_response", !taskRequested || taskResponse},
            {"task_state", !parser.isSet("wait-task-state") || taskStateSeen},
            {"observation_complete", !explicitActions || (actionSent && elapsed.elapsed() - actionSentAt >= observeMs)},
            {"fcu_connected", store.connected() && client.telemetryFresh()},
            {"fcu_disarmed", !store.armed()}, {"telemetry_stable", store.telemetryStable()},
            {"fcu_association", fcuId > 0 && (!expectedFcu || fcuId == expectedFcu)},
            {"position_received", gotPosition}, {"velocity_received", gotVelocity}, {"attitude_received", gotAttitude},
            {"battery_valid", store.batteryValid()},
            {"position_expected", vectorMatches(expectedPosition, store.positionX(), store.positionY(), store.positionZ())},
            {"velocity_expected", vectorMatches(expectedVelocity, store.velocityX(), store.velocityY(), store.velocityZ())},
            {"battery_expected", !parser.isSet("expect-battery") || near(expectedBattery, store.batteryVoltage())},
            {"mode_expected", !parser.isSet("expect-mode") || store.flightMode() == parser.value("expect-mode")},
            {"custom_data", !parser.isSet("require-custom-data") || (store.preflightValid() && gotTaskState)},
            {"voxel_map", !parser.isSet("require-voxel-map") || store.voxelMapValid()},
            {"grid_map", !parser.isSet("require-grid-map") || store.gridMapValid()},
            {"planned_path", !parser.isSet("require-planned-path") || store.plannedPathSize() > 0}
        };
    };
    auto finish = [&](bool success, const QString &reason) {
        if (finished) return;
        finished = true;
        const QJsonObject result{
            {"ok", success}, {"reason", reason}, {"elapsed_ms", double(elapsed.elapsed())},
            {"transport", serialMode ? "serial" : "tcp"}, {"remote_ip", remoteIp}, {"tcp_port", port},
            {"serial_port", serialMode ? parser.value("serial") : QString()}, {"baud", serialMode ? baud : 0},
            {"radio_pairing_read_only", client.radioPairingReadOnly()}, {"radio_actual_address", client.radioActualAddress()},
            {"radio_pairing_state", client.radioPairingState()}, {"radio_pairing_error", client.radioPairingErrorText()},
            {"serial_rx_bytes", double(client.serialRxBytes())}, {"serial_tx_bytes", double(client.serialTxBytes())},
            {"uav_id", id}, {"fcu_system_id", fcuId},
            {"decoded_messages", messages}, {"checks", checks()}, {"connection", client.connectionSummary()},
            {"position_enu", QJsonArray{store.positionX(), store.positionY(), store.positionZ()}},
            {"velocity_enu", QJsonArray{store.velocityX(), store.velocityY(), store.velocityZ()}},
            {"attitude_flu_enu_degrees", QJsonArray{store.roll(), store.pitch(), store.yaw()}},
            {"battery_voltage", store.batteryVoltage()}, {"battery_fraction", store.batteryPercent()},
            {"armed", store.armed()}, {"flight_mode", store.flightMode()},
            {"managed_task_state", store.managedTaskState()}, {"preflight_valid", store.preflightValid()},
            {"managed_task_name", store.managedTaskName()}, {"managed_task_path", store.managedTaskPath()},
            {"managed_task_ack", store.managedTaskAck()}, {"managed_task_reason", store.managedTaskReason()},
            {"task_request_id", taskRequestId}, {"task_events", taskEvents}, {"protocol_acks", protocolAcks},
            {"explicit_action", taskRequested ? taskAction : plannerGoal ? "SUPER_GOAL" : "NONE"},
            {"action_error", actionError},
            {"voxel_columns", store.voxelMap().columns.size()}, {"planned_path_points", store.plannedPathSize()},
            {"parameter_ack", QJsonObject::fromVariantMap(lastParamAck)},
            {"parameter_response", QJsonObject::fromVariantMap(lastParams)},
            {"protocol_log", client.protocolLogText()}
        };
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << Qt::endl;
        client.stop();
        app.exit(success ? 0 : 1);
    };
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (!parser.isSet("skip-params") && !querySent && handshakeAck && !client.awaitingModeSelectionAck() && client.isConnected()) {
            querySent = true;
            QVariantList params;
            if (parser.isSet("param-name")) params.append(QVariantMap{{"type", 5}, {"param_name", parser.value("param-name")}, {"param_value", ""}});
            client.sendTcpMessage(ZenithProtocol::PARAMSETTINGS, {{"param_module", paramModule}, {"params", params}}, id);
        }
        if (explicitActions && !actionSent && client.canSendControlCommands() && store.connected() && !store.armed()) {
            if (taskRequested) {
                taskRequestId = dispatcher.sendManagedTaskRequest(taskName, taskAction, false, taskPath);
                actionSent = !taskRequestId.isEmpty();
            } else if (plannerGoal) {
                actionSent = dispatcher.sendPlannerGoal(plannerGoal->at(0), plannerGoal->at(1), plannerGoal->at(2), 0.0);
            }
            if (actionSent) actionSentAt = elapsed.elapsed();
        }
        const auto status = checks();
        bool success = true;
        for (auto it = status.begin(); it != status.end(); ++it) success &= it.value().toBool();
        if (!actionError.isEmpty()) finish(false, actionError);
        else if (success) finish(true, "all required production client/store checks passed");
        else if (elapsed.elapsed() >= timeout) finish(false, "timeout waiting for required checks");
    });
    poll.start(50);
    client.start();
    return app.exec();
}
