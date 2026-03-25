#pragma once

#include <QObject>
#include <QPointF>
#include <QVariantList>

class TelemetryStore;
class ZenithProtocolClient;
class CommandDispatcher;

class AppState : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString vehicleName READ vehicleName NOTIFY telemetryChanged)
    Q_PROPERTY(QString flightStatus READ flightStatus NOTIFY telemetryChanged)
    Q_PROPERTY(QString flightMode READ flightMode NOTIFY telemetryChanged)
    Q_PROPERTY(QString controllerMode READ controllerMode NOTIFY telemetryChanged)
    Q_PROPERTY(QString controlState READ controlState NOTIFY telemetryChanged)
    Q_PROPERTY(QString locationSource READ locationSource NOTIFY telemetryChanged)
    Q_PROPERTY(QString gpsStatus READ gpsStatus NOTIFY telemetryChanged)
    Q_PROPERTY(QString heartbeatLink READ heartbeatLink NOTIFY telemetryChanged)
    Q_PROPERTY(QString videoLink READ videoLink NOTIFY telemetryChanged)
    Q_PROPERTY(QString rcLink READ rcLink NOTIFY telemetryChanged)
    Q_PROPERTY(bool armed READ armed NOTIFY telemetryChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY telemetryChanged)
    Q_PROPERTY(bool failsafe READ failsafe NOTIFY telemetryChanged)
    Q_PROPERTY(double batteryVoltage READ batteryVoltage NOTIFY telemetryChanged)
    Q_PROPERTY(double batteryPercent READ batteryPercent NOTIFY telemetryChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY telemetryChanged)
    Q_PROPERTY(double speed READ speed NOTIFY telemetryChanged)
    Q_PROPERTY(double positionX READ positionX NOTIFY telemetryChanged)
    Q_PROPERTY(double positionY READ positionY NOTIFY telemetryChanged)
    Q_PROPERTY(double positionZ READ positionZ NOTIFY telemetryChanged)
    Q_PROPERTY(double velocityX READ velocityX NOTIFY telemetryChanged)
    Q_PROPERTY(double velocityY READ velocityY NOTIFY telemetryChanged)
    Q_PROPERTY(double velocityZ READ velocityZ NOTIFY telemetryChanged)
    Q_PROPERTY(double heading READ heading NOTIFY telemetryChanged)
    Q_PROPERTY(double roll READ roll NOTIFY telemetryChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY telemetryChanged)
    Q_PROPERTY(double yaw READ yaw NOTIFY telemetryChanged)
    Q_PROPERTY(double desiredPositionX READ desiredPositionX NOTIFY telemetryChanged)
    Q_PROPERTY(double desiredPositionY READ desiredPositionY NOTIFY telemetryChanged)
    Q_PROPERTY(double desiredPositionZ READ desiredPositionZ NOTIFY telemetryChanged)
    Q_PROPERTY(double desiredVelocityX READ desiredVelocityX NOTIFY telemetryChanged)
    Q_PROPERTY(double desiredVelocityY READ desiredVelocityY NOTIFY telemetryChanged)
    Q_PROPERTY(double desiredVelocityZ READ desiredVelocityZ NOTIFY telemetryChanged)
    Q_PROPERTY(double homeDistance READ homeDistance NOTIFY telemetryChanged)
    Q_PROPERTY(QString missionStage READ missionStage NOTIFY telemetryChanged)
    Q_PROPERTY(QString videoStatus READ videoStatus NOTIFY telemetryChanged)
    Q_PROPERTY(QString alertLevel READ alertLevel NOTIFY telemetryChanged)
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY telemetryChanged)
    Q_PROPERTY(QString lastCommand READ lastCommand NOTIFY telemetryChanged)
    Q_PROPERTY(QString commandAck READ commandAck NOTIFY telemetryChanged)
    Q_PROPERTY(QString remoteHostIp READ remoteHostIp NOTIFY linkSettingsChanged)
    Q_PROPERTY(int udpPort READ udpPort NOTIFY linkSettingsChanged)
    Q_PROPERTY(int tcpPort READ tcpPort NOTIFY linkSettingsChanged)
    Q_PROPERTY(int heartbeatPort READ heartbeatPort NOTIFY linkSettingsChanged)
    Q_PROPERTY(QString udpLinkState READ udpLinkState NOTIFY linkStateChanged)
    Q_PROPERTY(QString tcpLinkState READ tcpLinkState NOTIFY linkStateChanged)
    Q_PROPERTY(QString heartbeatLinkState READ heartbeatLinkState NOTIFY linkStateChanged)
    Q_PROPERTY(QString connectionSummary READ connectionSummary NOTIFY linkStateChanged)
    Q_PROPERTY(QString protocolLogText READ protocolLogText NOTIFY linkStateChanged)
    Q_PROPERTY(bool protocolConnected READ protocolConnected NOTIFY linkStateChanged)
    Q_PROPERTY(QVariantList pathPoints READ pathPoints NOTIFY pathChanged)
    Q_PROPERTY(QVariantList waypointPoints READ waypointPoints NOTIFY pathChanged)
public:
    explicit AppState(QObject *parent = nullptr);
    ~AppState() override;

    QString vehicleName() const;
    QString flightStatus() const;
    QString flightMode() const;
    QString controllerMode() const;
    QString controlState() const;
    QString locationSource() const;
    QString gpsStatus() const;
    QString heartbeatLink() const;
    QString videoLink() const;
    QString rcLink() const;
    bool armed() const;
    bool connected() const;
    bool failsafe() const;
    double batteryVoltage() const;
    double batteryPercent() const;
    double altitude() const;
    double speed() const;
    double positionX() const;
    double positionY() const;
    double positionZ() const;
    double velocityX() const;
    double velocityY() const;
    double velocityZ() const;
    double heading() const;
    double roll() const;
    double pitch() const;
    double yaw() const;
    double desiredPositionX() const;
    double desiredPositionY() const;
    double desiredPositionZ() const;
    double desiredVelocityX() const;
    double desiredVelocityY() const;
    double desiredVelocityZ() const;
    double homeDistance() const;
    QString missionStage() const;
    QString videoStatus() const;
    QString alertLevel() const;
    QString currentTime() const;
    QString lastCommand() const;
    QString commandAck() const;
    QString remoteHostIp() const;
    int udpPort() const;
    int tcpPort() const;
    int heartbeatPort() const;
    QString udpLinkState() const;
    QString tcpLinkState() const;
    QString heartbeatLinkState() const;
    QString connectionSummary() const;
    QString protocolLogText() const;
    bool protocolConnected() const;
    QVariantList pathPoints() const;
    QVariantList waypointPoints() const;

    Q_INVOKABLE void selectVehicle(const QString &name);
    Q_INVOKABLE void issueCommand(const QString &commandName);
    Q_INVOKABLE void sendManualMove(const QString &mode, double x, double y, double z, double yawDeg);
    Q_INVOKABLE void runScriptAction(const QString &name, const QString &command, const QString &target);
    Q_INVOKABLE void applyConnectionSettings(const QString &hostIp, int udpPort, int tcpPort, int heartbeatPort);
    Q_INVOKABLE void connectProtocol();
    Q_INVOKABLE void disconnectProtocol();
    Q_INVOKABLE bool testProtocol();

signals:
    void telemetryChanged();
    void pathChanged();
    void commandTriggered(const QString &commandName);
    void linkStateChanged();
    void linkSettingsChanged();

private:
    QVariantList toVariantList(const QList<QPointF> &points) const;
    void syncFromStore();
    void bootstrapDemoTelemetry();

    QString m_vehicleName;
    QString m_flightStatus;
    QString m_flightMode;
    QString m_controllerMode;
    QString m_controlState;
    QString m_locationSource;
    QString m_gpsStatus;
    QString m_heartbeatLink;
    QString m_videoLink;
    QString m_rcLink;
    QString m_missionStage;
    QString m_videoStatus;
    QString m_alertLevel;
    QString m_currentTime;
    QString m_lastCommand;
    QString m_commandAck;
    QString m_remoteHostIp;
    int m_udpPort;
    int m_tcpPort;
    int m_heartbeatPort;
    QString m_udpLinkState;
    QString m_tcpLinkState;
    QString m_heartbeatLinkState;
    QString m_connectionSummary;
    QString m_protocolLogText;
    bool m_protocolConnected;
    bool m_armed;
    bool m_connected;
    bool m_failsafe;
    double m_batteryVoltage;
    double m_batteryPercent;
    double m_altitude;
    double m_speed;
    double m_positionX;
    double m_positionY;
    double m_positionZ;
    double m_velocityX;
    double m_velocityY;
    double m_velocityZ;
    double m_heading;
    double m_roll;
    double m_pitch;
    double m_yaw;
    double m_desiredPositionX;
    double m_desiredPositionY;
    double m_desiredPositionZ;
    double m_desiredVelocityX;
    double m_desiredVelocityY;
    double m_desiredVelocityZ;
    double m_homeDistance;
    QList<QPointF> m_pathPoints;
    QList<QPointF> m_waypointPoints;
    TelemetryStore *m_telemetryStore = nullptr;
    ZenithProtocolClient *m_protocolClient = nullptr;
    CommandDispatcher *m_commandDispatcher = nullptr;
};
