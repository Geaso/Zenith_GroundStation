#include "CommandDispatcher.h"

#include "TelemetryStore.h"
#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"

#include <QMap>
#include <QDateTime>
#include <cmath>

CommandDispatcher::CommandDispatcher(TelemetryStore *telemetryStore, ZenithProtocolClient *protocolClient, QObject *parent)
    : QObject(parent),
      m_telemetryStore(telemetryStore),
      m_protocolClient(protocolClient)
{
}

namespace {
bool ensureControlLinkReady(TelemetryStore *telemetryStore, ZenithProtocolClient *protocolClient, const QString &commandName)
{
    if (protocolClient->canSendControlCommands()) {
        return true;
    }
    telemetryStore->setCommandFeedback(commandName, QStringLiteral("Blocked: link not CONNECTED"));
    return false;
}
}

void CommandDispatcher::issueQuickAction(const QString &name)
{
    // 安全命令（降落、当前点悬停）：只要串口/连接在就发，不受link状态阻塞
    const bool isSafetyCommand = (name == "Land" || name == "降落" || name == "Emergency Land"
                                  || name == "Hover Here" || name == "当前点悬停");
    if (!isSafetyCommand && !ensureControlLinkReady(m_telemetryStore, m_protocolClient, name)) {
        return;
    }

    QVariantMap payload;

    if (name == "Hover Here" || name == "当前点悬停") {
        payload.insert("Agent_CMD", 2);  // Current_Pos_Hover
    } else if (name == "Hover Home" || name == "初始点悬停") {
        payload.insert("Agent_CMD", 1);  // Init_Pos_Hover
    } else if (name == "Land" || name == "降落" || name == "Emergency Land") {
        payload.insert("Agent_CMD", 3);  // Land
    } else if (name == "Return Home") {
        payload.insert("Agent_CMD", 2);
    } else {
        payload.insert("Agent_CMD", 2);
    }

    payload.insert("Control_Level", 0);
    payload.insert("Move_mode", 0);
    payload.insert("position_ref", QVariantList{0.0, 0.0, 0.0});
    payload.insert("velocity_ref", QVariantList{0.0, 0.0, 0.0});
    payload.insert("acceleration_ref", QVariantList{0.0, 0.0, 0.0});
    payload.insert("yaw_ref", 0.0);
    payload.insert("Yaw_Rate_Mode", false);
    payload.insert("yaw_rate_ref", 0.0);
    payload.insert("att_ref", QVariantList{0.0, 0.0, 0.0, 0.0});
    payload.insert("latitude", 0.0);
    payload.insert("longitude", 0.0);
    payload.insert("altitude", 0.0);
    payload.insert("Command_ID", static_cast<int>(m_commandId++));

    sendUavCommand(payload, name);
}

void CommandDispatcher::sendManualMove(const QString &mode, double x, double y, double z, double yawDeg)
{
    if (!ensureControlLinkReady(m_telemetryStore, m_protocolClient, QString("Manual Move %1").arg(mode))) {
        return;
    }

    static const QMap<QString, int> moveModes = {
        {"XYZ_POS", 0},
        {"XY_VEL_Z_POS", 1},
        {"XYZ_VEL", 2},
        {"XYZ_POS_BODY", 3},
        {"XYZ_VEL_BODY", 4},
        {"XY_VEL_Z_POS_BODY", 5},
        {"TRAJECTORY", 6},
        {"XYZ_ATT", 7},
        {"LAT_LON_ALT", 8}
    };

    QVariantMap payload;
    payload.insert("Agent_CMD", 4);
    payload.insert("Control_Level", 0);
    payload.insert("Move_mode", moveModes.value(mode, 0));
    payload.insert("position_ref", QVariantList{x, y, z});
    payload.insert("velocity_ref", QVariantList{0.0, 0.0, 0.0});
    payload.insert("acceleration_ref", QVariantList{0.0, 0.0, 0.0});
    payload.insert("yaw_ref", yawDeg * M_PI / 180.0);
    payload.insert("Yaw_Rate_Mode", false);
    payload.insert("yaw_rate_ref", 0.0);
    payload.insert("att_ref", QVariantList{0.0, 0.0, 0.0, 0.0});
    payload.insert("latitude", 0.0);
    payload.insert("longitude", 0.0);
    payload.insert("altitude", z);
    payload.insert("Command_ID", static_cast<int>(m_commandId++));

    double desiredPosX = m_telemetryStore->positionX();
    double desiredPosY = m_telemetryStore->positionY();
    double desiredPosZ = m_telemetryStore->positionZ();
    double desiredVelX = 0.0;
    double desiredVelY = 0.0;
    double desiredVelZ = 0.0;

    if (mode == "XYZ_POS" || mode == "XYZ_POS_BODY" || mode == "LAT_LON_ALT") {
        desiredPosX = x;
        desiredPosY = y;
        desiredPosZ = z;
    } else if (mode == "XYZ_VEL" || mode == "XYZ_VEL_BODY") {
        desiredVelX = x;
        desiredVelY = y;
        desiredVelZ = z;
    } else if (mode == "XY_VEL_Z_POS" || mode == "XY_VEL_Z_POS_BODY") {
        desiredPosZ = z;
        desiredVelX = x;
        desiredVelY = y;
    }

    m_telemetryStore->setDesiredReference(desiredPosX, desiredPosY, desiredPosZ,
                                          desiredVelX, desiredVelY, desiredVelZ);

    sendUavCommand(payload, QString("Manual Move %1").arg(mode));
}

void CommandDispatcher::runScript(const QString &name, const QString &command, const QString &target)
{
    // 历史 bug: 这里以前发 CUSTOMDATASEGMENT_1, 但飞机端没有节点订阅并执行 system().
    // 实际上需要走 MODESELECTION/CUSTOMMODE 路径, 那条路 Bridge 收到后会调用
    // UAVBasic::loadCmdPub() → system(cmd + " &") 真正执行, 同时回传 CMD_ACK 日志.
    Q_UNUSED(target);
    if (!ensureControlLinkReady(m_telemetryStore, m_protocolClient, QString("StartScript: %1").arg(name))) {
        return;
    }

    QVariantMap payload;
    payload.insert("mode", ZenithProtocol::CUSTOMMODE_MODE);
    payload.insert("selectId", QVariantList{m_telemetryStore->currentVehicleId()});
    payload.insert("use_mode", ZenithProtocol::UM_CREATE);
    payload.insert("is_simulation", false);
    payload.insert("swarm_num", 1);
    payload.insert("cmd", command);  // 直接给原始 shell cmd, Bridge 不加 prefix 就当 "其他命令"

    m_protocolClient->sendTcpMessage(ZenithProtocol::MODESELECTION, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(QString("StartScript: %1").arg(name), "Sent (awaiting CMD_ACK)");
}

void CommandDispatcher::sendManagedTaskRequest(const QString &taskName,
                                                const QString &action,
                                                bool yawEnable,
                                                const QString &taskPath)
{
    const QString normalizedAction = action.trimmed().toUpper();
    const QString commandName = QStringLiteral("ManagedTask %1 %2")
                                    .arg(normalizedAction, taskName);
    if (!ensureControlLinkReady(m_telemetryStore, m_protocolClient, commandName)) {
        return;
    }

    const QString requestId = QStringLiteral("gcs-%1-%2")
                                  .arg(QDateTime::currentMSecsSinceEpoch())
                                  .arg(m_taskRequestSequence++);
    struct Field {
        QString name;
        int type;
        QString value;
    };
    QList<Field> fields = {
        {QStringLiteral("task_schema"), 1, QStringLiteral("1")},
        {QStringLiteral("task_request_id"), 5, requestId},
        {QStringLiteral("task_name"), 5, taskName},
        {QStringLiteral("task_action"), 5, normalizedAction},
        {QStringLiteral("task_target_id"), 1, QStringLiteral("-1")},
        {QStringLiteral("task_yaw_enable"), 2,
         yawEnable ? QStringLiteral("true") : QStringLiteral("false")},
    };
    if (!taskPath.isEmpty()) {
        fields.append({QStringLiteral("task_path"), 5, taskPath});
    }

    // JsonConverter in the aircraft bridge uses this indexed flat shape for
    // CustomDataSegment_1 (message 113).
    QVariantMap payload;
    payload.insert(QStringLiteral("datas_num"), fields.size());
    for (int i = 0; i < fields.size(); ++i) {
        payload.insert(QStringLiteral("name[%1]").arg(i), fields.at(i).name);
        payload.insert(QStringLiteral("type[%1]").arg(i), fields.at(i).type);
        payload.insert(QStringLiteral("value[%1]").arg(i), fields.at(i).value);
    }

    m_protocolClient->sendTcpMessage(
        ZenithProtocol::CUSTOMDATASEGMENT_1, payload,
        m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(
        commandName, QStringLiteral("已发送，等待机载任务 ACK (%1)").arg(requestId));
}

void CommandDispatcher::armVehicle(bool arm)
{
    if (!ensureControlLinkReady(m_telemetryStore, m_protocolClient, arm ? QStringLiteral("Arm") : QStringLiteral("Disarm"))) {
        return;
    }

    QVariantMap payload;
    payload.insert("cmd", 0);  // ARMING
    payload.insert("arming", arm);
    payload.insert("px4_mode", QString());
    payload.insert("control_state", QString());
    sendUavSetup(payload, arm ? "Arm" : "Disarm");
}

void CommandDispatcher::setPx4Mode(const QString &mode)
{
    if (!ensureControlLinkReady(m_telemetryStore, m_protocolClient, QString("PX4 Mode: %1").arg(mode))) {
        return;
    }

    QVariantMap payload;
    payload.insert("cmd", 1);  // SET_PX4_MODE
    payload.insert("arming", false);
    payload.insert("px4_mode", mode);
    payload.insert("control_state", QString());
    sendUavSetup(payload, QString("PX4 Mode: %1").arg(mode));
}

void CommandDispatcher::switchLocationSource(int sourceIndex)
{
    if (!ensureControlLinkReady(m_telemetryStore, m_protocolClient, QString("Switch Location Source %1").arg(sourceIndex))) {
        return;
    }

    static const QStringList sourceNames = {
        "GPS", "RTK", "VINS", "MID360", "ODIN", "ORBSLAM3", "OAKVIO"
    };

    const int vehicleId = m_telemetryStore->currentVehicleId();
    const QString paramName = QString("/uav_control_main_%1/control/location_source").arg(vehicleId);

    QVariantMap param;
    param.insert("type", 1);  // INT
    param.insert("param_name", paramName);
    param.insert("param_value", QString::number(sourceIndex));

    QVariantMap payload;
    payload.insert("param_module", 6);  // SEARCHMODIFY
    payload.insert("params", QVariantList{param});

    const QString name = sourceNames.value(sourceIndex, "UNKNOWN");
    m_protocolClient->sendTcpMessage(ZenithProtocol::PARAMSETTINGS, payload, vehicleId);
    m_telemetryStore->setCommandFeedback(QString("LocationSource: %1").arg(name), "ParamSettings queued");
}

void CommandDispatcher::executeRemoteCommand(const QString &moduleName, const QString &command)
{
    if (!m_protocolClient->isConnected()) {
        m_telemetryStore->setCommandFeedback(QString("RemoteExec: %1").arg(moduleName), "Blocked: TCP not connected");
        return;
    }

    QVariantMap payload;
    payload.insert("mode", ZenithProtocol::CUSTOMMODE_MODE);
    payload.insert("selectId", QVariantList{m_telemetryStore->currentVehicleId()});
    payload.insert("use_mode", ZenithProtocol::UM_CREATE);
    payload.insert("is_simulation", false);
    payload.insert("swarm_num", 1);
    payload.insert("cmd", QStringLiteral("remote_exec:") + command);

    m_protocolClient->sendTcpMessage(ZenithProtocol::MODESELECTION, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(QString("RemoteExec: %1").arg(moduleName), "Command sent");
}

void CommandDispatcher::stopRemoteModule(const QString &moduleName, const QString &nodePattern)
{
    executeRemoteCommand(QString("Stop %1").arg(moduleName),
                         QStringLiteral("/home/jetson/Zenith_ws/scripts/modules/stop_module.sh ") + nodePattern);
}

quint32 CommandDispatcher::sendWaypoint(double x, double y, double z, double yawRad, bool isContinuation)
{
    if (!ensureControlLinkReady(m_telemetryStore, m_protocolClient, QStringLiteral("Mission Waypoint"))) {
        return 0;
    }

    const quint32 localId = m_commandId++;
    const quint32 wireId = isContinuation ? (localId | 0x80000000u) : localId;

    QVariantMap payload;
    payload.insert("Agent_CMD", 4);   // Move
    payload.insert("Control_Level", 0);
    payload.insert("Move_mode", 0);   // XYZ_POS
    payload.insert("position_ref", QVariantList{x, y, z});
    payload.insert("velocity_ref", QVariantList{0.0, 0.0, 0.0});
    payload.insert("acceleration_ref", QVariantList{0.0, 0.0, 0.0});
    payload.insert("yaw_ref", yawRad);  // radians, drone-side: yaw_des = wrap_to_nearest_pi(...)
    payload.insert("Yaw_Rate_Mode", false);
    payload.insert("yaw_rate_ref", 0.0);
    payload.insert("att_ref", QVariantList{0.0, 0.0, 0.0, 0.0});
    payload.insert("latitude", 0.0);
    payload.insert("longitude", 0.0);
    payload.insert("altitude", z);
    // qint64 保留无符号 32 位的高位 bit31（JSON 数字会被 .so 转回 uint32）
    payload.insert("Command_ID", static_cast<qint64>(wireId));

    sendUavCommand(payload, QString("Waypoint #%1 %2").arg(localId).arg(isContinuation ? "(cont)" : "(last)"));
    return localId;
}

void CommandDispatcher::sendUavCommand(const QVariantMap &payload, const QString &humanReadableName)
{
    m_protocolClient->sendTcpMessage(ZenithProtocol::UAVCOMMAND, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(humanReadableName, "UAVCommand queued");
}

void CommandDispatcher::sendUavSetup(const QVariantMap &payload, const QString &humanReadableName)
{
    m_protocolClient->sendTcpMessage(ZenithProtocol::UAVSETUP, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(humanReadableName, "UAVSetup queued");
}
