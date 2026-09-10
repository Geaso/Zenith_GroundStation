#pragma once

#include <QObject>

class TelemetryStore;
class ZenithProtocolClient;

class CommandDispatcher : public QObject
{
    Q_OBJECT
public:
    explicit CommandDispatcher(TelemetryStore *telemetryStore, ZenithProtocolClient *protocolClient, QObject *parent = nullptr);

    Q_INVOKABLE void issueQuickAction(const QString &name);
    Q_INVOKABLE void sendManualMove(const QString &mode, double x, double y, double z, double yawDeg);
    Q_INVOKABLE void runScript(const QString &name, const QString &command, const QString &target);
    Q_INVOKABLE void sendManagedTaskRequest(const QString &taskName, const QString &action,
                                            bool yawEnable = false,
                                            const QString &taskPath = QString());
    Q_INVOKABLE void armVehicle(bool arm);
    Q_INVOKABLE void setPx4Mode(const QString &mode);
    Q_INVOKABLE void switchLocationSource(int sourceIndex);
    Q_INVOKABLE void executeRemoteCommand(const QString &moduleName, const QString &command);
    Q_INVOKABLE void stopRemoteModule(const QString &moduleName, const QString &nodePattern);

    // 航线任务：发送单个航点（XYZ_POS），显式 waypoint_mission 标记还有后续点。
    // Command_ID 是完整 uint32 序号，不再借用高位编码状态。
    quint32 sendWaypoint(double x, double y, double z, double yawRad, bool isContinuation);

private:
    void sendUavCommand(const QVariantMap &payload, const QString &humanReadableName);
    void sendUavSetup(const QVariantMap &payload, const QString &humanReadableName);
    TelemetryStore *m_telemetryStore = nullptr;
    ZenithProtocolClient *m_protocolClient = nullptr;
    quint32 m_commandId = 1;
    quint32 m_taskRequestSequence = 1;
};
