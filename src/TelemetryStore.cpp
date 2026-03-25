#include "TelemetryStore.h"

#include <QDateTime>
#include <QtMath>

TelemetryStore::TelemetryStore(QObject *parent)
    : QObject(parent),
      m_vehicleName("UAV1"),
      m_flightStatus("Waiting For Zenith Link"),
      m_flightMode("UNKNOWN"),
      m_controllerMode("PX4_ORIGIN"),
      m_controlState("INIT"),
      m_locationSource("MOCAP"),
      m_gpsStatus("GPS_FIX_TYPE_NO_GPS"),
      m_heartbeatLink("No Heartbeat"),
      m_videoLink("Video Offline"),
      m_rcLink("RC Unknown"),
      m_missionStage("Idle"),
      m_videoStatus("RTSP Pending"),
      m_alertLevel("INFO"),
      m_currentTime(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")),
      m_lastCommand("None"),
      m_commandAck("Waiting")
{
    m_waypointPoints = {
        QPointF(0.14, 0.20),
        QPointF(0.28, 0.32),
        QPointF(0.44, 0.38),
        QPointF(0.56, 0.54),
        QPointF(0.74, 0.62)
    };
}

QString TelemetryStore::vehicleName() const { return m_vehicleName; }
QString TelemetryStore::flightStatus() const { return m_flightStatus; }
QString TelemetryStore::flightMode() const { return m_flightMode; }
QString TelemetryStore::controllerMode() const { return m_controllerMode; }
QString TelemetryStore::controlState() const { return m_controlState; }
QString TelemetryStore::locationSource() const { return m_locationSource; }
QString TelemetryStore::gpsStatus() const { return m_gpsStatus; }
QString TelemetryStore::heartbeatLink() const { return m_heartbeatLink; }
QString TelemetryStore::videoLink() const { return m_videoLink; }
QString TelemetryStore::rcLink() const { return m_rcLink; }
bool TelemetryStore::armed() const { return m_armed; }
bool TelemetryStore::connected() const { return m_connected; }
bool TelemetryStore::failsafe() const { return m_failsafe; }
double TelemetryStore::batteryVoltage() const { return m_batteryVoltage; }
double TelemetryStore::batteryPercent() const { return m_batteryPercent; }
double TelemetryStore::altitude() const { return m_altitude; }
double TelemetryStore::relativeAltitude() const { return m_relativeAltitude; }
double TelemetryStore::range() const { return m_range; }
double TelemetryStore::speed() const { return m_speed; }
double TelemetryStore::positionX() const { return m_positionX; }
double TelemetryStore::positionY() const { return m_positionY; }
double TelemetryStore::positionZ() const { return m_positionZ; }
double TelemetryStore::velocityX() const { return m_velocityX; }
double TelemetryStore::velocityY() const { return m_velocityY; }
double TelemetryStore::velocityZ() const { return m_velocityZ; }
double TelemetryStore::heading() const { return m_heading; }
double TelemetryStore::roll() const { return m_roll; }
double TelemetryStore::pitch() const { return m_pitch; }
double TelemetryStore::yaw() const { return m_yaw; }
double TelemetryStore::desiredPositionX() const { return m_desiredPositionX; }
double TelemetryStore::desiredPositionY() const { return m_desiredPositionY; }
double TelemetryStore::desiredPositionZ() const { return m_desiredPositionZ; }
double TelemetryStore::desiredVelocityX() const { return m_desiredVelocityX; }
double TelemetryStore::desiredVelocityY() const { return m_desiredVelocityY; }
double TelemetryStore::desiredVelocityZ() const { return m_desiredVelocityZ; }
double TelemetryStore::homeDistance() const { return m_homeDistance; }
QString TelemetryStore::missionStage() const { return m_missionStage; }
QString TelemetryStore::videoStatus() const { return m_videoStatus; }
QString TelemetryStore::alertLevel() const { return m_alertLevel; }
QString TelemetryStore::currentTime() const { return m_currentTime; }
QString TelemetryStore::lastCommand() const { return m_lastCommand; }
QString TelemetryStore::commandAck() const { return m_commandAck; }
QList<QPointF> TelemetryStore::pathPoints() const { return m_pathPoints; }
QList<QPointF> TelemetryStore::waypointPoints() const { return m_waypointPoints; }

QString TelemetryStore::currentVehicleTopicRoot() const
{
    return QString("/uav%1/zenith").arg(m_currentVehicleId);
}

int TelemetryStore::currentVehicleId() const
{
    return m_currentVehicleId;
}

void TelemetryStore::setVehicleName(const QString &name)
{
    bool ok = false;
    const int parsedId = name.mid(3).toInt(&ok);
    m_vehicleName = name;
    if (ok && parsedId > 0) {
        m_currentVehicleId = parsedId;
    }
    emit telemetryChanged();
}

void TelemetryStore::applyUavState(const QVariantMap &payload, int senderId)
{
    auto readVector = [&payload](const QStringList &keys) {
        for (const QString &key : keys) {
            const QVariantList values = payload.value(key).toList();
            if (!values.isEmpty()) {
                return values;
            }
        }
        return QVariantList{};
    };

    m_currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_connected = payload.value("connected", true).toBool();
    m_armed = payload.value("armed", false).toBool();
    m_flightMode = payload.value("mode", "UNKNOWN").toString();
    m_locationSource = locationSourceName(payload.value("location_source").toInt());
    m_gpsStatus = gpsStatusName(payload.value("gps_status").toInt());
    m_batteryVoltage = payload.value("battery_state").toDouble();
    m_batteryPercent = payload.value("battery_percetage").toDouble();
    m_altitude = payload.value("altitude").toDouble();
    m_relativeAltitude = payload.value("rel_alt").toDouble();
    m_range = payload.value("range").toDouble();

    const QVariantList position = payload.value("position").toList();
    const QVariantList velocity = payload.value("velocity").toList();
    const QVariantList attitude = payload.value("attitude").toList();

    const double posX = position.value(0).toDouble();
    const double posY = position.value(1).toDouble();
    const double posZ = position.value(2).toDouble();
    m_positionX = posX;
    m_positionY = posY;
    m_positionZ = posZ;
    m_homeDistance = qSqrt(posX * posX + posY * posY);

    const double velX = velocity.value(0).toDouble();
    const double velY = velocity.value(1).toDouble();
    const double velZ = velocity.value(2).toDouble();
    m_velocityX = velX;
    m_velocityY = velY;
    m_velocityZ = velZ;
    m_speed = qSqrt(velX * velX + velY * velY + velZ * velZ);

    m_roll = qRadiansToDegrees(attitude.value(0).toDouble());
    m_pitch = qRadiansToDegrees(attitude.value(1).toDouble());
    m_yaw = qRadiansToDegrees(attitude.value(2).toDouble());
    m_heading = m_yaw;

    const QVariantList desiredPosition = readVector({"desired_position", "desiredPosition", "position_ref", "positionRef"});
    const QVariantList desiredVelocity = readVector({"desired_velocity", "desiredVelocity", "velocity_ref", "velocityRef"});
    if (!desiredPosition.isEmpty()) {
        m_desiredPositionX = desiredPosition.value(0).toDouble();
        m_desiredPositionY = desiredPosition.value(1).toDouble();
        m_desiredPositionZ = desiredPosition.value(2).toDouble();
    }
    if (!desiredVelocity.isEmpty()) {
        m_desiredVelocityX = desiredVelocity.value(0).toDouble();
        m_desiredVelocityY = desiredVelocity.value(1).toDouble();
        m_desiredVelocityZ = desiredVelocity.value(2).toDouble();
    }

    m_flightStatus = m_connected ? "Telemetry Online" : "Vehicle Offline";
    m_heartbeatLink = "Heartbeat OK";
    m_videoLink = payload.value("video_status", m_videoStatus).toString();
    m_vehicleName = QString("UAV%1").arg(senderId > 0 ? senderId : m_currentVehicleId);
    if (senderId > 0) {
        m_currentVehicleId = senderId;
    }

    QPointF sample(0.10 + qMin(0.82, qAbs(posX) / 50.0), 0.12 + qMin(0.72, qAbs(posY) / 50.0));
    if (m_pathPoints.isEmpty() || m_pathPoints.last() != sample) {
        m_pathPoints.append(sample);
        if (m_pathPoints.size() > 120) {
            m_pathPoints.removeFirst();
        }
        emit pathChanged();
    }

    emit telemetryChanged();
}

void TelemetryStore::applyTextInfo(const QVariantMap &payload)
{
    const int messageType = payload.value("MessageType").toInt();
    m_flightStatus = payload.value("Message").toString();
    switch (messageType) {
    case 0:
        m_alertLevel = "INFO";
        break;
    case 1:
        m_alertLevel = "WARN";
        break;
    case 2:
        m_alertLevel = "ERROR";
        break;
    case 3:
        m_alertLevel = "FATAL";
        break;
    default:
        break;
    }
    emit telemetryChanged();
}

void TelemetryStore::applyUavControlState(const QVariantMap &payload)
{
    static const QStringList controlStates = {"INIT", "MANUAL", "HOVER", "COMMAND", "LAND"};
    static const QStringList controllers = {"PX4_ORIGIN", "PID", "UDE", "NE"};
    const int controlState = payload.value("control_state").toInt();
    const int controller = payload.value("pos_controller").toInt();
    m_controlState = controlStates.value(controlState, "UNKNOWN");
    m_controllerMode = controllers.value(controller, "UNKNOWN");
    m_failsafe = payload.value("failsafe").toBool();
    emit telemetryChanged();
}

void TelemetryStore::applyHeartbeat(const QVariantMap &payload)
{
    m_heartbeatLink = QString("Heartbeat #%1").arg(payload.value("count").toInt());
    if (!payload.value("message").toString().isEmpty()) {
        m_commandAck = payload.value("message").toString();
    }
    m_connected = true;
    emit telemetryChanged();
}

void TelemetryStore::setCommandFeedback(const QString &commandName, const QString &ackText)
{
    m_lastCommand = commandName;
    m_commandAck = ackText;
    m_currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    emit telemetryChanged();
}

void TelemetryStore::setMissionStage(const QString &stage)
{
    m_missionStage = stage;
    emit telemetryChanged();
}

void TelemetryStore::setWaypointPoints(const QList<QPointF> &points)
{
    m_waypointPoints = points;
    emit pathChanged();
}

void TelemetryStore::setDesiredReference(double posX, double posY, double posZ, double velX, double velY, double velZ)
{
    m_desiredPositionX = posX;
    m_desiredPositionY = posY;
    m_desiredPositionZ = posZ;
    m_desiredVelocityX = velX;
    m_desiredVelocityY = velY;
    m_desiredVelocityZ = velZ;
    emit telemetryChanged();
}

QString TelemetryStore::locationSourceName(int locationSource) const
{
    static const QStringList names = {
        "MOCAP", "T265", "GAZEBO", "FAKE_ODOM", "GPS", "RTK", "UWB",
        "VINS", "OPTICAL_FLOW", "VIOBOT", "MID360", "BSA_SLAM", "ODIN", "PROSIM"
    };
    return names.value(locationSource, "UNKNOWN");
}

QString TelemetryStore::gpsStatusName(int gpsStatus) const
{
    static const QStringList names = {
        "GPS_FIX_TYPE_NO_GPS", "GPS_FIX_TYPE_NO_FIX", "GPS_FIX_TYPE_2D_FIX",
        "GPS_FIX_TYPE_3D_FIX", "GPS_FIX_TYPE_DGPS", "GPS_FIX_TYPE_RTK_FLOATR",
        "GPS_FIX_TYPE_RTK_FIXEDR", "GPS_FIX_TYPE_STATIC", "GPS_FIX_TYPE_PPP"
    };
    return names.value(gpsStatus, "GPS_UNKNOWN");
}
