#include "TelemetryStore.h"
#include "ZenithProtocol.h"

#include <QDateTime>
#include <QtMath>
#include <QtEndian>
#include <cmath>
#include <limits>

namespace {
bool mapUnsigned(const QVariantMap &payload, const char *key, quint32 limit, quint32 &out,
                 bool allowJsonNumber = false)
{
    const QVariant value = payload.value(QLatin1String(key));
    if (allowJsonNumber && (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Float)) {
        const double number = value.toDouble();
        if (!std::isfinite(number) || number < 0 || number > limit || std::floor(number) != number)
            return false;
        out = quint32(number);
        return true;
    }
    if (value.typeId() != QMetaType::UInt && value.typeId() != QMetaType::Int
        && value.typeId() != QMetaType::ULongLong && value.typeId() != QMetaType::LongLong)
        return false;
    bool ok = false;
    const qulonglong number = value.toULongLong(&ok);
    if (!ok || number > limit) return false;
    out = quint32(number);
    return true;
}

bool mapFloat(const QVariantMap &payload, const char *key, float &out)
{
    const QVariant value = payload.value(QLatin1String(key));
    if (value.typeId() != QMetaType::Float && value.typeId() != QMetaType::Double
        && value.typeId() != QMetaType::Int && value.typeId() != QMetaType::UInt
        && value.typeId() != QMetaType::LongLong && value.typeId() != QMetaType::ULongLong)
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max())
        return false;
    out = float(number);
    return true;
}

bool mapExtentFinite(float origin, float resolution, int size)
{
    const double end = double(origin) + double(resolution) * size;
    return std::isfinite(end) && std::abs(end) <= std::numeric_limits<float>::max();
}

bool sameVoxelMetadata(const TelemetryStore::VoxelMapSnapshot &a,
                       const TelemetryStore::VoxelMapSnapshot &b)
{
    return a.vehicleId == b.vehicleId && a.frameId == b.frameId
        && a.originX == b.originX && a.originY == b.originY && a.resolution == b.resolution
        && a.width == b.width && a.height == b.height && a.zMin == b.zMin
        && a.zResolution == b.zResolution && a.layers == b.layers;
}
}

TelemetryStore::TelemetryStore(QObject *parent)
    : QObject(parent),
      m_vehicleName("UAV1"),
      m_flightStatus("Waiting For Zenith Link"),
      m_flightMode("UNKNOWN"),
      m_controllerMode("PX4_ORIGIN"),
      m_controlState("INIT"),
      m_execState("DISARMED"),
      m_missionMode("MANUAL"),
      m_activeCommandSource("NONE"),
      m_pendingRequest("NONE"),
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
    m_mapClock.start();
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
QString TelemetryStore::execState() const { return m_execState; }
QString TelemetryStore::missionMode() const { return m_missionMode; }
quint32 TelemetryStore::lastReachedWaypointId() const { return m_lastReachedWaypointId; }
QString TelemetryStore::activeCommandSource() const { return m_activeCommandSource; }
QString TelemetryStore::pendingRequest() const { return m_pendingRequest; }
bool TelemetryStore::requestActive() const { return m_requestActive; }
QString TelemetryStore::locationSource() const { return m_locationSource; }
QString TelemetryStore::gpsStatus() const { return m_gpsStatus; }
int TelemetryStore::gpsFixType() const { return m_gpsFixType; }
int TelemetryStore::locationSourceId() const { return m_locationSourceId; }
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
double TelemetryStore::vinsPositionX() const { return m_vinsPositionX; }
double TelemetryStore::vinsPositionY() const { return m_vinsPositionY; }
double TelemetryStore::vinsPositionZ() const { return m_vinsPositionZ; }
bool TelemetryStore::odomValid() const { return m_odomValid; }
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
QString TelemetryStore::managedTaskName() const { return m_managedTaskName; }
QString TelemetryStore::managedTaskPath() const { return m_managedTaskPath; }
QString TelemetryStore::managedTaskState() const { return m_managedTaskState; }
QString TelemetryStore::managedTaskReason() const { return m_managedTaskReason; }
QString TelemetryStore::managedTaskAck() const { return m_managedTaskAck; }
QString TelemetryStore::managedTaskRequestId() const { return m_managedTaskRequestId; }
bool TelemetryStore::managedTaskActive() const { return m_managedTaskActive; }
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

int TelemetryStore::lastTelemetrySenderId() const
{
    return m_lastTelemetrySenderId;
}

QStringList TelemetryStore::runningNodes() const
{
    return m_runningNodes;
}

QString TelemetryStore::moduleExecFeedback() const
{
    return m_moduleExecFeedback;
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
    // 收到真正的飞行遥测才计数；心跳帧不走这里，所以不会把"链路通"误判成"数据可用"。
    if (m_uavStateFrames < StableFrames) {
        ++m_uavStateFrames;
    }
    if (m_firstUavStateMs == 0) {
        m_firstUavStateMs = QDateTime::currentMSecsSinceEpoch();
    }
    bool wasArmed = m_armed;
    m_armed = payload.value("armed", false).toBool();
    if (m_armed && !wasArmed) {
        m_trail3D.clear();
        m_pathPoints.clear();
        emit pathChanged();
    }
    m_flightMode = payload.value("mode", "UNKNOWN").toString();
    m_locationSourceId = payload.value("location_source").toInt();
    m_gpsFixType = payload.value("gps_status").toInt();
    m_locationSource = locationSourceName(m_locationSourceId);
    m_gpsStatus = gpsStatusName(m_gpsFixType);
    m_batteryVoltage = payload.value("battery_state").toDouble();
    m_batteryPercent = payload.value("battery_percetage").toDouble();
    m_altitude = payload.value("altitude").toDouble();
    m_relativeAltitude = payload.value("rel_alt").toDouble();
    m_range = payload.value("range").toDouble();

    const QVariantList position = payload.value("position").toList();
    const QVariantList vinsPosition = payload.value("vins_position").toList();
    const QVariantList velocity = payload.value("velocity").toList();
    const QVariantList attitude = payload.value("attitude").toList();

    const double posX = position.value(0).toDouble();
    const double posY = position.value(1).toDouble();
    const double posZ = position.value(2).toDouble();
    m_positionX = posX;
    m_positionY = posY;
    m_positionZ = posZ;
    m_homeDistance = qSqrt(posX * posX + posY * posY);

    if (!vinsPosition.isEmpty()) {
        m_vinsPositionX = vinsPosition.value(0).toDouble();
        m_vinsPositionY = vinsPosition.value(1).toDouble();
        m_vinsPositionZ = vinsPosition.value(2).toDouble();
    }
    m_odomValid = payload.value("odom_valid", false).toBool();

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
    Q_UNUSED(senderId)

    QPointF sample(0.10 + qMin(0.82, qAbs(posX) / 50.0), 0.12 + qMin(0.72, qAbs(posY) / 50.0));
    if (m_pathPoints.isEmpty() || m_pathPoints.last() != sample) {
        m_pathPoints.append(sample);
        if (m_pathPoints.size() > 120) {
            m_pathPoints.removeFirst();
        }
        emit pathChanged();
    }

    TrailPt tp{static_cast<float>(posX), static_cast<float>(posY), static_cast<float>(posZ)};
    if (m_trail3D.isEmpty() ||
        qAbs(tp.x - m_trail3D.last().x) > 0.02f ||
        qAbs(tp.y - m_trail3D.last().y) > 0.02f ||
        qAbs(tp.z - m_trail3D.last().z) > 0.02f) {
        m_trail3D.append(tp);
        if (m_trail3D.size() > kMaxTrail)
            m_trail3D.removeFirst();
    }

    emit telemetryChanged();
}

void TelemetryStore::applyTextInfo(const QVariantMap &payload)
{
    const int messageType = payload.value("MessageType").toInt();
    const QString message = payload.value("Message").toString();

    // 拦截命令ACK: "CMD_ACK:ID=N:STATUS"
    if (message.startsWith(QLatin1String("CMD_ACK:"))) {
        m_commandAck = message;
        emit telemetryChanged();
        return;
    }

    // 拦截模块执行反馈: "MODULE_EXEC:..."
    if (message.startsWith(QLatin1String("MODULE_EXEC:"))) {
        m_moduleExecFeedback = message;
        m_commandAck = message;
        emit telemetryChanged();
        return;
    }

    m_flightStatus = message;
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

    // 追加到任务日志滚动缓冲（机载 mission_log_forwarder 把 /rosout 转成 TextInfo 送上来）
    if (!message.isEmpty()) {
        const QString line = QStringLiteral("[%1] %2 %3")
                                 .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                      m_alertLevel.leftJustified(5, QLatin1Char(' ')),
                                      message);
        m_missionLog.prepend(line);
        while (m_missionLog.size() > MissionLogMax)
            m_missionLog.removeLast();
    }

    emit telemetryChanged();
}

void TelemetryStore::noteFrameReceived(int senderId)
{
    bool changed = false;
    if (senderId >= 0 && m_lastTelemetrySenderId != senderId) {
        m_lastTelemetrySenderId = senderId;
        changed = true;
        emit telemetrySenderChanged();
    }
    if (!m_linkEstablished) {
        m_linkEstablished = true;
        changed = true;
    }
    if (changed) {
        emit telemetryChanged();
    }
}

bool TelemetryStore::linkEstablished() const { return m_linkEstablished; }
bool TelemetryStore::telemetryStable() const
{
    if (!m_connected || m_firstUavStateMs == 0) {
        return false;
    }
    if (m_uavStateFrames < StableFrames) {
        return false;
    }
    return (QDateTime::currentMSecsSinceEpoch() - m_firstUavStateMs) >= StableDwellMs;
}

bool TelemetryStore::fcuReady()  const { return m_readyMask & 0x1; }
bool TelemetryStore::batteryValid() const { return m_readyMask & 0x2; }
bool TelemetryStore::locReady()  const { return m_readyMask & 0x4; }
bool TelemetryStore::ctrlReady() const { return m_readyMask & 0x8; }
int  TelemetryStore::aircraftUptime() const { return m_aircraftUptime; }

QVariantList TelemetryStore::readinessSteps() const
{
    // 与机载 preflight_reporter 的 rd_mask 位序、rd_times 顺序一一对应
    static const char *names[4] = {"飞控连接", "电池数据", "定位就绪", "控制状态机"};
    const QStringList times = m_readyTimes.split(QLatin1Char(','));
    QVariantList list;
    for (int i = 0; i < 4; ++i) {
        QVariantMap m;
        m.insert("name", QString::fromUtf8(names[i]));
        m.insert("ready", bool(m_readyMask & (1 << i)));
        m.insert("atSec", i < times.size() ? times.at(i).toDouble() : 0.0);
        list.append(m);
    }
    return list;
}

QString TelemetryStore::missionLogText() const
{
    return m_missionLog.join(QLatin1Char('\n'));
}

void TelemetryStore::clearMissionLog()
{
    m_missionLog.clear();
    emit telemetryChanged();
}

void TelemetryStore::applyUavControlState(const QVariantMap &payload)
{
    // Must match zenith_msgs/UAVControlState.msg exactly.  The old five-item
    // table was inherited from a different wire enum and shifted COMMAND/LAND.
    static const QStringList controlStates = {"INIT", "RC_POS", "COMMAND", "LAND"};
    static const QStringList controllers = {"PX4_ORIGIN", "PID", "UDE", "NE"};
    static const QStringList execStates = {
        "DISARMED", "STANDBY", "RC_CONTROL", "AUTO_HOLD",
        "AUTO_TRACK", "AUTO_LAND", "FAILSAFE"
    };
    static const QStringList missionModes = {
        "MANUAL", "HOVER", "MOVE", "TRACK_TRAJ",
        "AUTO_EXPLORE", "RETURN_HOME", "LAND_PENDING", "EMERGENCY"
    };
    static const QStringList commandSources = {
        "NONE", "RC", "MISSION", "PLANNER", "FAILSAFE", "GROUND_STATION"
    };
    static const QStringList pendingRequests = {
        "NONE", "ENTER_INIT", "ENTER_RC", "ENTER_CMD",
        "HOVER", "LAND", "MANUAL_OVERRIDE"
    };

    const int controlState = payload.value("control_state").toInt();
    const int controller = payload.value("pos_controller").toInt();
    m_controlState = controlStates.value(controlState, "UNKNOWN");
    m_controllerMode = controllers.value(controller, "UNKNOWN");
    m_failsafe = payload.value("failsafe").toBool();

    if (payload.contains("exec_state"))
        m_execState = execStates.value(payload.value("exec_state").toInt(), "UNKNOWN");
    if (payload.contains("mission_mode"))
        m_missionMode = missionModes.value(payload.value("mission_mode").toInt(), "UNKNOWN");
    if (payload.contains("active_command_source"))
        m_activeCommandSource = commandSources.value(payload.value("active_command_source").toInt(), "UNKNOWN");
    if (payload.contains("pending_request"))
        m_pendingRequest = pendingRequests.value(payload.value("pending_request").toInt(), "UNKNOWN");
    if (payload.contains("request_active"))
        m_requestActive = payload.value("request_active").toBool();
    if (payload.contains("last_reached_waypoint_id"))
        m_lastReachedWaypointId = payload.value("last_reached_waypoint_id").toUInt();

    emit telemetryChanged();
}

void TelemetryStore::applyHeartbeat(const QVariantMap &payload)
{
    m_heartbeatLink = QString("Heartbeat #%1").arg(payload.value("count").toInt());
    if (!payload.value("message").toString().isEmpty()) {
        m_commandAck = payload.value("message").toString();
    }

    // 提取 ROS 节点列表 (heartbeat 中以 "rosnode" 或 "nodes" 字段传递)
    if (payload.contains("nodes")) {
        const QVariantList nodeList = payload.value("nodes").toList();
        m_runningNodes.clear();
        for (const auto &n : nodeList)
            m_runningNodes.append(n.toString());
    } else if (payload.contains("rosnode")) {
        const QString nodesStr = payload.value("rosnode").toString();
        m_runningNodes = nodesStr.split(',', Qt::SkipEmptyParts);
    }

    m_connected = true;
    emit telemetryChanged();
}

void TelemetryStore::setTransportHealth(bool telemetryFresh, bool heartbeatFresh, const QString &summary)
{
    QString nextFlightStatus = m_flightStatus;
    if (!telemetryFresh) {
        nextFlightStatus = summary.contains("DEGRADED") || summary.contains("RECONNECTING")
            ? QStringLiteral("Telemetry Stale")
            : QStringLiteral("No Telemetry");
    } else if (nextFlightStatus == QLatin1String("No Telemetry") || nextFlightStatus == QLatin1String("Telemetry Stale")) {
        nextFlightStatus = QStringLiteral("Telemetry Online");
    }

    const QString nextHeartbeat = heartbeatFresh ? m_heartbeatLink : QStringLiteral("Heartbeat Stale");
    const bool nextConnected = telemetryFresh;
    const bool telemetryLost = m_connected && !nextConnected;
    const bool senderLost = !telemetryFresh && m_lastTelemetrySenderId != -1;

    if (nextFlightStatus == m_flightStatus && nextHeartbeat == m_heartbeatLink
        && nextConnected == m_connected && !senderLost) {
        return;
    }

    m_flightStatus = nextFlightStatus;
    m_heartbeatLink = nextHeartbeat;
    m_connected = nextConnected;
    if (telemetryLost || senderLost) {
        invalidateVehicleData();
    }
    emit telemetryChanged();
}

void TelemetryStore::invalidateVehicleData()
{
    resetMapReception();
    // 链路一断，上一次收到的遥测就不再代表飞机现在的状态，必须作废。
    //
    // 换电池时最明显：拔电池 -> 飞机断电 -> 插新电池 -> 机载重启。bridge 大约 +5s
    // 就上线并开始发心跳，而产生 UAVSTATE 的控制状态机要到 +14s 才起来。这中间
    // 链路已经算"通"了却还没有任何飞行数据，若不清缓存，地面站就会把上一块电池
    // 拔出前的电压和电量显示成新电池的读数，过几秒才跳到真实值。
    //
    // rd_mask 是机载"首次就绪锁存"，飞机重启后会从 0 重新累积，所以这里一并清零，
    // batteryValid()/locReady() 等判据在新数据到达前保持 false，UI 显示 "--"。
    m_readyMask = 0;
    // 重新累积稳定判据：飞机重启后 UAVSTATE 会中断再恢复，过渡期必须重新压住显示。
    m_uavStateFrames = 0;
    m_firstUavStateMs = 0;
    m_batteryVoltage = 0.0;
    m_batteryPercent = 0.0;
    m_altitude = 0.0;
    m_relativeAltitude = 0.0;
    m_range = 0.0;
    m_speed = 0.0;
    if (m_lastTelemetrySenderId != -1) {
        m_lastTelemetrySenderId = -1;
        emit telemetrySenderChanged();
    }
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
    // 必须与机载 zenith_msgs/UAVState.msg 的枚举严格一致，索引即枚举值。
    // 旧表只有 7 项且起点错位，导致 ODIN(12) 落到范围外显示 UNKNOWN。
    static const QStringList names = {
        "MOCAP",        // 0
        "T265",         // 1
        "GAZEBO",       // 2
        "FAKE_ODOM",    // 3
        "GPS",          // 4
        "RTK",          // 5
        "UWB",          // 6
        "VINS",         // 7
        "OPTICAL_FLOW", // 8
        "VIOBOT",       // 9
        "MID360",       // 10
        "BSA_SLAM",     // 11
        "ODIN",         // 12
        "ProSim",       // 13
        "OPENVINS",     // 14
        "ORBSLAM3",     // 15
        "OAKVIO"        // 16
    };
    return names.value(locationSource, "UNKNOWN");
}

// ---------------- 解锁前检查（PX4 SYS_STATUS 传感器健康位）----------------

namespace {
// MAV_SYS_STATUS_SENSOR 位定义，与机载 preflight_reporter.py 保持一致
struct SensorBit { quint32 bit; const char *name; };
const SensorBit kSensorBits[] = {
    {0x00000001, "陀螺仪"},        {0x00000002, "加速度计"},
    {0x00000004, "磁力计"},        {0x00000008, "气压计"},
    {0x00000010, "空速计"},        {0x00000020, "GPS"},
    {0x00000040, "光流"},          {0x00000080, "视觉定位"},
    {0x00000100, "激光定位"},      {0x00000200, "外部真值"},
    {0x00000400, "角速率控制"},    {0x00000800, "姿态增稳"},
    {0x00001000, "偏航控制"},      {0x00002000, "高度控制"},
    {0x00004000, "水平位置控制"},  {0x00008000, "电机输出"},
    {0x00010000, "遥控接收机"},    {0x00020000, "陀螺仪2"},
    {0x00040000, "加速度计2"},     {0x00080000, "磁力计2"},
    {0x00100000, "地理围栏"},      {0x00200000, "AHRS 姿态参考"},
    {0x00400000, "地形"},          {0x00800000, "反转电机"},
    {0x01000000, "日志"},          {0x02000000, "电池"},
    {0x04000000, "近距感知"},      {0x08000000, "卫通"},
    {0x10000000, "解锁前检查"},    {0x20000000, "避障"},
    {0x40000000, "动力系统"},
};
} // namespace

bool TelemetryStore::preflightValid() const { return m_preflightValid; }
bool TelemetryStore::preflightArmOk() const { return m_preflightArmOk; }
QString TelemetryStore::preflightFail() const { return m_preflightFail; }
bool TelemetryStore::preflightPrearmBit() const { return m_preflightPrearmBit; }
int TelemetryStore::preflightArmAck() const { return m_preflightArmAck; }
QString TelemetryStore::preflightArmAckText() const { return m_preflightArmAckText; }

QVariantList TelemetryStore::preflightChecks() const
{
    QVariantList list;
    if (!m_preflightValid)
        return list;
    for (const SensorBit &s : kSensorBits) {
        if (!(m_preflightPresent & s.bit))
            continue; // 该机型没有这个子系统，不显示
        QVariantMap item;
        item.insert("name", QString::fromUtf8(s.name));
        item.insert("enabled", bool(m_preflightEnabled & s.bit));
        item.insert("healthy", bool(m_preflightHealth & s.bit));
        list.append(item);
    }
    return list;
}

void TelemetryStore::applyCustomDataSegment(const QVariantMap &payload)
{
    // 线格式是扁平化带索引的：datas_num / name[i] / type[i] / value[i]
    const int count = payload.value(QStringLiteral("datas_num")).toInt();
    if (count <= 0)
        return;

    bool touched = false;
    bool preflightTouched = false;
    for (int i = 0; i < count; ++i) {
        const QString key = payload.value(QStringLiteral("name[%1]").arg(i)).toString();
        const QString value = payload.value(QStringLiteral("value[%1]").arg(i)).toString();

        if (key == QLatin1String("pf_present")) {
            m_preflightPresent = static_cast<quint32>(value.toLongLong());
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("pf_enabled")) {
            m_preflightEnabled = static_cast<quint32>(value.toLongLong());
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("pf_health")) {
            m_preflightHealth = static_cast<quint32>(value.toLongLong());
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("pf_arm_ok")) {
            // 注意：机载 customDataSegmentCb 里 ROS 的 bool 字段是 uint8_t，
            // setValue() 重载会选中 int 版本，所以线上实际是 type=INTEGER、值 "1"/"0"，
            // 而不是 BOOLEAN 的 "true"/"false"。两种都要认。
            m_preflightArmOk = (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
                            || (value.toInt() != 0);
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("pf_fail")) {
            m_preflightFail = value;
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("rd_mask")) {
            m_readyMask = value.toInt();
            touched = true;
        } else if (key == QLatin1String("rd_times")) {
            m_readyTimes = value;
            touched = true;
        } else if (key == QLatin1String("rd_up")) {
            m_aircraftUptime = value.toInt();
            touched = true;
        } else if (key == QLatin1String("pf_prearm_bit")) {
            m_preflightPrearmBit = (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
                                || (value.toInt() != 0);
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("pf_arm_ack")) {
            m_preflightArmAck = value.toInt();
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("pf_arm_ack_txt")) {
            m_preflightArmAckText = value;
            touched = true;
            preflightTouched = true;
        } else if (key == QLatin1String("task_name")) {
            m_managedTaskName = value;
            touched = true;
        } else if (key == QLatin1String("task_path")) {
            m_managedTaskPath = value;
            touched = true;
        } else if (key == QLatin1String("task_state")) {
            m_managedTaskState = value;
            touched = true;
        } else if (key == QLatin1String("task_reason")) {
            m_managedTaskReason = value;
            touched = true;
        } else if (key == QLatin1String("task_ack")) {
            m_managedTaskAck = value;
            touched = true;
        } else if (key == QLatin1String("task_request_id")) {
            m_managedTaskRequestId = value;
            touched = true;
        } else if (key == QLatin1String("task_active")) {
            m_managedTaskActive =
                value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
                || value.toInt() != 0;
            touched = true;
        }
    }

    if (touched) {
        if (preflightTouched)
            m_preflightValid = true;
        emit telemetryChanged();
    }
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

qint64 TelemetryStore::mapArrivalTime(qint64 suppliedTime) const
{
    return suppliedTime >= 0 ? suppliedTime : m_mapClock.elapsed();
}

void TelemetryStore::resetMapReception()
{
    // Preserve the last complete picture across a link interruption, but no
    // partial snapshot or sequence from the previous connection may survive.
    m_voxelAssembly = {};
    m_voxelSequences.clear();
    m_lastVoxelProgressMs = -ZenithProtocol::VoxelMap::kAssemblyTimeoutMs - 1;
}

bool TelemetryStore::applyVoxelMap(const QVariantMap &payload, int senderId, qint64 receivedAtMs)
{
    using namespace ZenithProtocol::VoxelMap;
    if (senderId < 0 || senderId > 255) return false;
    VoxelMapSnapshot metadata;
    metadata.vehicleId = senderId;
    quint32 width = 0, height = 0, layers = 0, partIndex = 0, partCount = 0, encoding = 0;
    if (!mapFloat(payload, "vm_origin_x", metadata.originX)
        || !mapFloat(payload, "vm_origin_y", metadata.originY)
        || !mapFloat(payload, "vm_xy_resolution", metadata.resolution)
        || !mapFloat(payload, "vm_z_min", metadata.zMin)
        || !mapFloat(payload, "vm_z_resolution", metadata.zResolution)
        || !mapUnsigned(payload, "vm_width", 65535, width)
        || !mapUnsigned(payload, "vm_height", 65535, height)
        || !mapUnsigned(payload, "vm_layers", 32, layers)
        || !mapUnsigned(payload, "vm_frame_id", std::numeric_limits<quint32>::max(), metadata.frameId)
        || !mapUnsigned(payload, "vm_part_index", kMaxParts - 1, partIndex)
        || !mapUnsigned(payload, "vm_part_count", kMaxParts, partCount)
        || !mapUnsigned(payload, "vm_encoding", 1, encoding)) return false;
    const quint64 columnCount = quint64(width) * height;
    if (!width || !height || columnCount > kMaxColumns || !layers || encoding != 1
        || !partCount || partIndex >= partCount || partCount > columnCount
        || metadata.resolution <= 0 || metadata.zResolution <= 0
        || !mapExtentFinite(metadata.originX, metadata.resolution, int(width))
        || !mapExtentFinite(metadata.originY, metadata.resolution, int(height))
        || !mapExtentFinite(metadata.zMin, metadata.zResolution, int(layers))) return false;
    metadata.width = int(width);
    metadata.height = int(height);
    metadata.layers = int(layers);

    const QVariant dataValue = payload.value("vm_data");
    if (dataValue.typeId() != QMetaType::QByteArray) return false;
    const QByteArray data = dataValue.toByteArray();
    if (data.isEmpty() || data.size() > kMaxPartBytes || data.size() % 5 != 0) return false;
    const quint32 legalBits = layers == 32 ? std::numeric_limits<quint32>::max()
                                         : (quint32(1) << layers) - 1;
    int partColumns = 0;
    for (qsizetype i = 0; i < data.size(); i += 5) {
        const quint32 mask = qFromLittleEndian<quint32>(data.constData() + i);
        const quint8 run = static_cast<quint8>(data[i + 4]);
        if (!run || (mask & ~legalBits) || quint64(partColumns + run) > columnCount) return false;
        partColumns += run;
    }

    const qint64 now = mapArrivalTime(receivedAtMs);
    if (!m_voxelAssembly.parts.isEmpty()
        && now - m_voxelAssembly.startedAtMs > kAssemblyTimeoutMs)
        m_voxelAssembly = {};

    auto &sequence = m_voxelSequences[senderId];
    // No epoch exists on the wire: after a silent timeout (or link reset),
    // a sender may restart its uint32 counter. Old staging bytes are discarded.
    if (sequence.initialized && now - sequence.lastProgressMs > kAssemblyTimeoutMs) {
        sequence.initialized = false;
        if (m_voxelAssembly.metadata.vehicleId == senderId) m_voxelAssembly = {};
    }
    const quint32 delta = metadata.frameId - sequence.frameId;
    const bool newer = !sequence.initialized || (delta != 0 && delta < 0x80000000u);
    if (newer) {
        sequence.initialized = true;
        sequence.frameId = metadata.frameId;
        sequence.lastProgressMs = now;
        m_voxelAssembly = {};
        m_voxelAssembly.metadata = metadata;
        m_voxelAssembly.parts.resize(int(partCount));
        m_voxelAssembly.startedAtMs = now;
    } else if (metadata.frameId != sequence.frameId
               || m_voxelAssembly.parts.isEmpty()
               || m_voxelAssembly.metadata.vehicleId != senderId
               || m_voxelAssembly.metadata.frameId != metadata.frameId) {
        return false;
    }

    if (!sameVoxelMetadata(metadata, m_voxelAssembly.metadata)
        || m_voxelAssembly.parts.size() != int(partCount)) {
        m_voxelAssembly = {}; // false/conflicting headers poison this frame only
        return false;
    }
    QByteArray & slot = m_voxelAssembly.parts[int(partIndex)];
    if (!slot.isEmpty()) {
        if (slot == data) return true; // duplicate does not extend the deadline
        m_voxelAssembly = {};
        return false;
    }
    if (quint64(m_voxelAssembly.decodedColumns + partColumns) > columnCount) {
        m_voxelAssembly = {};
        return false;
    }
    slot = data;
    ++m_voxelAssembly.receivedParts;
    m_voxelAssembly.decodedColumns += partColumns;
    sequence.lastProgressMs = now;
    if (m_voxelMapValid && m_voxelMap.vehicleId == senderId) m_lastVoxelProgressMs = now;
    if (m_voxelAssembly.receivedParts != int(partCount)) return true;
    if (quint64(m_voxelAssembly.decodedColumns) != columnCount) {
        m_voxelAssembly = {};
        return false;
    }

    QVector<quint32> columns(int(columnCount), 0);
    int offset = 0;
    for (const QByteArray &part : m_voxelAssembly.parts) {
        for (qsizetype i = 0; i < part.size(); i += 5) {
            const quint32 mask = qFromLittleEndian<quint32>(part.constData() + i);
            const quint8 run = static_cast<quint8>(part[i + 4]);
            for (int j = 0; j < run; ++j) columns[offset++] = mask;
        }
    }
    metadata.columns = std::move(columns);
    m_voxelMap = std::move(metadata);
    m_voxelMapValid = true; // an all-zero complete snapshot is also valid
    m_lastVoxelProgressMs = now;
    m_voxelAssembly = {};
    emit gridMapChanged();
    return true;
}

void TelemetryStore::applyGridMap(const QVariantMap &payload, int senderId, qint64 receivedAtMs)
{
    using namespace ZenithProtocol::VoxelMap;
    // No comparable sequence exists in GRIDMAP/11. Prefer an active 3D stream;
    // permit deliberate legacy fallback only after that stream goes quiet.
    if (m_voxelMapValid && (senderId == m_voxelMap.vehicleId || senderId == 0)
        && mapArrivalTime(receivedAtMs) - m_lastVoxelProgressMs <= kAssemblyTimeoutMs) return;

    float ox = 0, oy = 0, resolution = 0;
    quint32 width = 0, height = 0;
    if (!mapFloat(payload, "gm_origin_x", ox) || !mapFloat(payload, "gm_origin_y", oy)
        || !mapFloat(payload, "gm_resolution", resolution) || resolution <= 0
        || !mapUnsigned(payload, "gm_width", 65535, width, true)
        || !mapUnsigned(payload, "gm_height", 65535, height, true)
        || quint64(width) * height > kMaxColumns || (!width != !height)
        || !mapExtentFinite(ox, resolution, int(width))
        || !mapExtentFinite(oy, resolution, int(height))) return;
    const int total = int(width * height);
    const QByteArray rle = payload.value("gm_data").toByteArray();
    if (rle.size() % 2 != 0 || rle.size() > total * 2) return;
    QVector<uint8_t> cells(total, 0);
    int offset = 0;
    for (qsizetype i = 0; i < rle.size(); i += 2) {
        const quint8 value = static_cast<quint8>(rle[i]);
        const quint8 run = static_cast<quint8>(rle[i + 1]);
        if (!run || offset + run > total) return;
        for (int j = 0; j < run; ++j) cells[offset++] = value;
    }
    if (offset != total) return;
    m_gmOriginX = ox;
    m_gmOriginY = oy;
    m_gmResolution = resolution;
    m_gmWidth = int(width);
    m_gmHeight = int(height);
    m_gmSliceZ = payload.value("gm_slice_z").toFloat();
    m_gmCells = std::move(cells);
    m_voxelMapValid = false;
    m_voxelMap = {};
    emit gridMapChanged();
}

void TelemetryStore::applyPlannedPath(const QVariantMap &payload)
{
    int npts = payload.value("pp_num_points").toInt();
    QByteArray raw = payload.value("pp_data").toByteArray();

    m_plannedPath.clear();
    if (npts <= 0 || raw.size() < npts * 12) {
        emit plannedPathChanged();
        return;
    }

    m_plannedPath.reserve(npts);
    const float *fp = reinterpret_cast<const float*>(raw.constData());
    for (int i = 0; i < npts; ++i) {
        m_plannedPath.append({fp[i*3], fp[i*3+1], fp[i*3+2]});
    }
    emit plannedPathChanged();
}
