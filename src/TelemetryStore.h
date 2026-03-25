#pragma once

#include <QObject>
#include <QPointF>
#include <QVariantMap>

class TelemetryStore : public QObject
{
    Q_OBJECT
public:
    explicit TelemetryStore(QObject *parent = nullptr);

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
    double relativeAltitude() const;
    double range() const;
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
    QList<QPointF> pathPoints() const;
    QList<QPointF> waypointPoints() const;
    QString currentVehicleTopicRoot() const;
    int currentVehicleId() const;

    void setVehicleName(const QString &name);
    void applyUavState(const QVariantMap &payload, int senderId);
    void applyTextInfo(const QVariantMap &payload);
    void applyUavControlState(const QVariantMap &payload);
    void applyHeartbeat(const QVariantMap &payload);
    void setCommandFeedback(const QString &commandName, const QString &ackText);
    void setMissionStage(const QString &stage);
    void setWaypointPoints(const QList<QPointF> &points);
    void setDesiredReference(double posX, double posY, double posZ, double velX, double velY, double velZ);

signals:
    void telemetryChanged();
    void pathChanged();

private:
    QString locationSourceName(int locationSource) const;
    QString gpsStatusName(int gpsStatus) const;
    QList<QPointF> m_pathPoints;
    QList<QPointF> m_waypointPoints;
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
    bool m_armed = false;
    bool m_connected = false;
    bool m_failsafe = false;
    double m_batteryVoltage = 0.0;
    double m_batteryPercent = 0.0;
    double m_altitude = 0.0;
    double m_relativeAltitude = 0.0;
    double m_range = 0.0;
    double m_speed = 0.0;
    double m_positionX = 0.0;
    double m_positionY = 0.0;
    double m_positionZ = 0.0;
    double m_velocityX = 0.0;
    double m_velocityY = 0.0;
    double m_velocityZ = 0.0;
    double m_heading = 0.0;
    double m_roll = 0.0;
    double m_pitch = 0.0;
    double m_yaw = 0.0;
    double m_desiredPositionX = 0.0;
    double m_desiredPositionY = 0.0;
    double m_desiredPositionZ = 0.0;
    double m_desiredVelocityX = 0.0;
    double m_desiredVelocityY = 0.0;
    double m_desiredVelocityZ = 0.0;
    double m_homeDistance = 0.0;
    int m_currentVehicleId = 1;
};
