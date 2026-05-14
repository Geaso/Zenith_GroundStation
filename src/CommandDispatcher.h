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
    Q_INVOKABLE void armVehicle(bool arm);
    Q_INVOKABLE void setPx4Mode(const QString &mode);
    Q_INVOKABLE void switchLocationSource(int sourceIndex);

private:
    void sendUavCommand(const QVariantMap &payload, const QString &humanReadableName);
    void sendUavSetup(const QVariantMap &payload, const QString &humanReadableName);
    TelemetryStore *m_telemetryStore = nullptr;
    ZenithProtocolClient *m_protocolClient = nullptr;
    quint32 m_commandId = 1;
};
