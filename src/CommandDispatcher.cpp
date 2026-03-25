#include "CommandDispatcher.h"

#include "TelemetryStore.h"
#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"

#include <QMap>

CommandDispatcher::CommandDispatcher(TelemetryStore *telemetryStore, ZenithProtocolClient *protocolClient, QObject *parent)
    : QObject(parent),
      m_telemetryStore(telemetryStore),
      m_protocolClient(protocolClient)
{
}

void CommandDispatcher::issueQuickAction(const QString &name)
{
    QVariantMap payload;

    if (name == "Hover Here" || name == "Hover Home") {
        payload.insert("Agent_CMD", name == "Hover Here" ? 2 : 1);
    } else if (name == "Land" || name == "Emergency Land") {
        payload.insert("Agent_CMD", 3);
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
    payload.insert("yaw_ref", yawDeg);
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
    QVariantMap payload;
    payload.insert("cmd", command);
    payload.insert("mode", 1);
    payload.insert("node_name", "");
    payload.insert("detection_cmd", "");
    payload.insert("flag", 1);
    payload.insert("cmd_level", 2);
    payload.insert("close_cmd", "");
    payload.insert("target", target);

    m_protocolClient->sendTcpMessage(ZenithProtocol::CUSTOMDATASEGMENT_1, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(QString("StartScript: %1").arg(name), "StartScript payload queued");
}

void CommandDispatcher::sendUavCommand(const QVariantMap &payload, const QString &humanReadableName)
{
    m_protocolClient->sendTcpMessage(ZenithProtocol::UAVCOMMAND, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(humanReadableName, "UAVCommand queued");
}
