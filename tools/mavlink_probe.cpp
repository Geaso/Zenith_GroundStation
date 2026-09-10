#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"
#include "TelemetryStore.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>
#include <cmath>
#include <optional>

// Manual integration probe. Its only outbound traffic is the production GCS
// heartbeat/handshake and one read-only PARAMSETTINGS query (module 2 or 5).
// Run against an explicitly isolated test bridge and simulated FCU publisher.
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ZenithMavlinkProbe");
    QCommandLineParser parser;
    parser.setApplicationDescription("Production MAVLink2 TCP/telemetry probe for an isolated test bridge. Never sends arming, movement, task start or parameter writes.");
    // Qt's automatic help/error exit path can use a Windows message box when
    // the launcher redirects handles instead of attaching an interactive
    // console. Keep this tool's output entirely on stdout/stderr.
    parser.addOption(QCommandLineOption(QStringList{"h", "help", "help-all", "?"},
                                        "Display this help and exit."));
    parser.addPositionalArgument("ip", "Explicit bridge IP address.");
    parser.addPositionalArgument("port", "Explicit isolated bridge TCP port.");
    parser.addPositionalArgument("uav_id", "Logical Zenith vehicle ID (1..254; 255 is the GCS).");
    parser.addOptions({
        {"timeout-ms", "Stop and print diagnostics after this duration.", "milliseconds", "15000"},
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
    if (args.size() != 3) return badArgument("Expected: ZenithMavlinkProbe [options] <ip> <port> <uav_id>");
    bool portOk = false, idOk = false, timeoutOk = false, fcuOk = true;
    const int port = args[1].toInt(&portOk), id = args[2].toInt(&idOk);
    const int timeout = parser.value("timeout-ms").toInt(&timeoutOk);
    const int expectedFcu = parser.isSet("fcu-id") ? parser.value("fcu-id").toInt(&fcuOk) : 0;
    if (QHostAddress(args[0]).isNull() || !portOk || port < 1 || port > 65535
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
    const double expectedBattery = parser.isSet("expect-battery") ? parser.value("expect-battery").toDouble(&batteryOk) : 0;
    if (!expectedOk || !batteryOk || !std::isfinite(expectedBattery)) return badArgument("Invalid expected position, velocity or voltage.");
    if (parser.isSet("param-name") && !parser.value("param-name").startsWith('/')) return badArgument("--param-name must be an absolute ROS parameter name.");

    ZenithProtocolClient client;
    TelemetryStore store;
    store.setVehicleName(QString("UAV%1").arg(id));
    client.setRemoteHostIp(args[0]); client.setTcpPort(quint16(port)); client.setRobotId(id);
    client.setUdpPort(0); // Never share the production UDP telemetry port.
    bool handshakeAck = false, querySent = false, queryAck = false, queryResponse = false;
    bool gotPosition = false, gotVelocity = false, gotAttitude = false, gotTaskState = false, finished = false;
    int fcuId = 0, messages = 0;
    const int paramModule = parser.isSet("param-name") ? 5 : 2;
    QVariantMap lastParamAck, lastParams;
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
        case ZenithProtocol::CUSTOMDATASEGMENT_1:
            for (int i = 0; i < payload.value("datas_num").toInt(); ++i)
                gotTaskState |= payload.value(QString("name[%1]").arg(i)).toString() == "task_state";
            store.applyCustomDataSegment(payload); break;
        case ZenithProtocol::VOXELMAP: store.applyVoxelMap(payload, robotId); break;
        case ZenithProtocol::GRIDMAP: store.applyGridMap(payload, robotId); break;
        case ZenithProtocol::PLANNEDPATH: store.applyPlannedPath(payload); break;
        case ZenithProtocol::PROTOCOL_ACK: {
            const bool received = payload.value("status") == "RECEIVED" || payload.value("status") == "DUPLICATE";
            if (payload.value("request_kind").toInt() == ZenithProtocol::MODESELECTION) handshakeAck |= received;
            if (querySent && payload.value("request_kind").toInt() == ZenithProtocol::PARAMSETTINGS) {
                lastParamAck = payload; queryAck |= received;
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
            {"parameter_ack", queryAck}, {"parameter_response", queryResponse},
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
            {"remote_ip", args[0]}, {"tcp_port", port}, {"uav_id", id}, {"fcu_system_id", fcuId},
            {"decoded_messages", messages}, {"checks", checks()}, {"connection", client.connectionSummary()},
            {"position_enu", QJsonArray{store.positionX(), store.positionY(), store.positionZ()}},
            {"velocity_enu", QJsonArray{store.velocityX(), store.velocityY(), store.velocityZ()}},
            {"attitude_flu_enu_degrees", QJsonArray{store.roll(), store.pitch(), store.yaw()}},
            {"battery_voltage", store.batteryVoltage()}, {"battery_fraction", store.batteryPercent()},
            {"armed", store.armed()}, {"flight_mode", store.flightMode()},
            {"managed_task_state", store.managedTaskState()}, {"preflight_valid", store.preflightValid()},
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
        if (!querySent && handshakeAck && !client.awaitingModeSelectionAck() && client.isConnected()) {
            querySent = true;
            QVariantList params;
            if (parser.isSet("param-name")) params.append(QVariantMap{{"type", 5}, {"param_name", parser.value("param-name")}, {"param_value", ""}});
            client.sendTcpMessage(ZenithProtocol::PARAMSETTINGS, {{"param_module", paramModule}, {"params", params}}, id);
        }
        const auto status = checks();
        bool success = true;
        for (auto it = status.begin(); it != status.end(); ++it) success &= it.value().toBool();
        if (success) finish(true, "all required production client/store checks passed");
        else if (elapsed.elapsed() >= timeout) finish(false, "timeout waiting for required checks");
    });
    poll.start(50);
    client.start();
    return app.exec();
}
