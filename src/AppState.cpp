#include "AppState.h"

#include "CommandDispatcher.h"
#include "TelemetryStore.h"
#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"

#include <QDateTime>

AppState::AppState(QObject *parent)
    : QObject(parent)
{
    m_telemetryStore = new TelemetryStore(this);
    m_protocolClient = new ZenithProtocolClient(this);
    m_commandDispatcher = new CommandDispatcher(m_telemetryStore, m_protocolClient, this);

    connect(m_telemetryStore, &TelemetryStore::telemetryChanged, this, [this]() {
        syncFromStore();
        emit telemetryChanged();
    });
    connect(m_telemetryStore, &TelemetryStore::pathChanged, this, [this]() {
        syncFromStore();
        emit pathChanged();
    });
    connect(m_protocolClient, &ZenithProtocolClient::linkStatesChanged, this, [this]() {
        m_remoteHostIp = m_protocolClient->remoteHostIp();
        m_udpPort = m_protocolClient->udpPort();
        m_tcpPort = m_protocolClient->tcpPort();
        m_heartbeatPort = m_protocolClient->heartbeatPort();
        m_udpLinkState = m_protocolClient->udpState();
        m_tcpLinkState = m_protocolClient->tcpState();
        m_heartbeatLinkState = m_protocolClient->heartbeatState();
        m_connectionSummary = m_protocolClient->connectionSummary();
        m_protocolLogText = m_protocolClient->protocolLogText();
        m_protocolConnected = m_protocolClient->isConnected();
        emit linkStateChanged();
        emit linkSettingsChanged();
    });
    connect(m_protocolClient, &ZenithProtocolClient::decodedMessage, this, [this](int msgId, int robotId, const QVariantMap &payload) {
        switch (msgId) {
        case ZenithProtocol::UAVSTATE:
            m_telemetryStore->applyUavState(payload, robotId);
            break;
        case ZenithProtocol::TEXTINFO:
            m_telemetryStore->applyTextInfo(payload);
            break;
        case ZenithProtocol::HEARTBEAT:
            m_telemetryStore->applyHeartbeat(payload);
            break;
        case ZenithProtocol::UAVCONTROLSTATE:
            m_telemetryStore->applyUavControlState(payload);
            break;
        default:
            break;
        }
    });
    bootstrapDemoTelemetry();
    syncFromStore();
    m_remoteHostIp = m_protocolClient->remoteHostIp();
    m_udpPort = m_protocolClient->udpPort();
    m_tcpPort = m_protocolClient->tcpPort();
    m_heartbeatPort = m_protocolClient->heartbeatPort();
    m_udpLinkState = m_protocolClient->udpState();
    m_tcpLinkState = m_protocolClient->tcpState();
    m_heartbeatLinkState = m_protocolClient->heartbeatState();
    m_connectionSummary = m_protocolClient->connectionSummary();
    m_protocolLogText = m_protocolClient->protocolLogText();
    m_protocolConnected = m_protocolClient->isConnected();
}

AppState::~AppState() = default;

QString AppState::vehicleName() const { return m_vehicleName; }
QString AppState::flightStatus() const { return m_flightStatus; }
QString AppState::flightMode() const { return m_flightMode; }
QString AppState::controllerMode() const { return m_controllerMode; }
QString AppState::controlState() const { return m_controlState; }
QString AppState::locationSource() const { return m_locationSource; }
QString AppState::gpsStatus() const { return m_gpsStatus; }
QString AppState::heartbeatLink() const { return m_heartbeatLink; }
QString AppState::videoLink() const { return m_videoLink; }
QString AppState::rcLink() const { return m_rcLink; }
bool AppState::armed() const { return m_armed; }
bool AppState::connected() const { return m_connected; }
bool AppState::failsafe() const { return m_failsafe; }
double AppState::batteryVoltage() const { return m_batteryVoltage; }
double AppState::batteryPercent() const { return m_batteryPercent; }
double AppState::altitude() const { return m_altitude; }
double AppState::speed() const { return m_speed; }
double AppState::positionX() const { return m_positionX; }
double AppState::positionY() const { return m_positionY; }
double AppState::positionZ() const { return m_positionZ; }
double AppState::velocityX() const { return m_velocityX; }
double AppState::velocityY() const { return m_velocityY; }
double AppState::velocityZ() const { return m_velocityZ; }
double AppState::heading() const { return m_heading; }
double AppState::roll() const { return m_roll; }
double AppState::pitch() const { return m_pitch; }
double AppState::yaw() const { return m_yaw; }
double AppState::desiredPositionX() const { return m_desiredPositionX; }
double AppState::desiredPositionY() const { return m_desiredPositionY; }
double AppState::desiredPositionZ() const { return m_desiredPositionZ; }
double AppState::desiredVelocityX() const { return m_desiredVelocityX; }
double AppState::desiredVelocityY() const { return m_desiredVelocityY; }
double AppState::desiredVelocityZ() const { return m_desiredVelocityZ; }
double AppState::homeDistance() const { return m_homeDistance; }
QString AppState::missionStage() const { return m_missionStage; }
QString AppState::videoStatus() const { return m_videoStatus; }
QString AppState::alertLevel() const { return m_alertLevel; }
QString AppState::currentTime() const { return m_currentTime; }
QString AppState::lastCommand() const { return m_lastCommand; }
QString AppState::commandAck() const { return m_commandAck; }
QString AppState::remoteHostIp() const { return m_remoteHostIp; }
int AppState::udpPort() const { return m_udpPort; }
int AppState::tcpPort() const { return m_tcpPort; }
int AppState::heartbeatPort() const { return m_heartbeatPort; }
QString AppState::udpLinkState() const { return m_udpLinkState; }
QString AppState::tcpLinkState() const { return m_tcpLinkState; }
QString AppState::heartbeatLinkState() const { return m_heartbeatLinkState; }
QString AppState::connectionSummary() const { return m_connectionSummary; }
QString AppState::protocolLogText() const { return m_protocolLogText; }
bool AppState::protocolConnected() const { return m_protocolConnected; }
QVariantList AppState::pathPoints() const { return toVariantList(m_pathPoints); }
QVariantList AppState::waypointPoints() const { return toVariantList(m_waypointPoints); }

void AppState::selectVehicle(const QString &name)
{
    m_telemetryStore->setVehicleName(name);
    m_protocolClient->setRobotId(m_telemetryStore->currentVehicleId());
    emit linkSettingsChanged();
}

void AppState::issueCommand(const QString &commandName)
{
    m_commandDispatcher->issueQuickAction(commandName);
    emit commandTriggered(commandName);
}

void AppState::sendManualMove(const QString &mode, double x, double y, double z, double yawDeg)
{
    m_commandDispatcher->sendManualMove(mode, x, y, z, yawDeg);
    emit commandTriggered(QString("Manual Move %1").arg(mode));
}

void AppState::runScriptAction(const QString &name, const QString &command, const QString &target)
{
    m_commandDispatcher->runScript(name, command, target);
    emit commandTriggered(QString("Script Run: %1").arg(name));
}

void AppState::applyConnectionSettings(const QString &hostIp, int udpPort, int tcpPort, int heartbeatPort)
{
    m_protocolClient->setRemoteHostIp(hostIp);
    m_protocolClient->setUdpPort(static_cast<quint16>(udpPort));
    m_protocolClient->setTcpPort(static_cast<quint16>(tcpPort));
    m_protocolClient->setHeartbeatPort(static_cast<quint16>(heartbeatPort));

    m_remoteHostIp = m_protocolClient->remoteHostIp();
    m_udpPort = m_protocolClient->udpPort();
    m_tcpPort = m_protocolClient->tcpPort();
    m_heartbeatPort = m_protocolClient->heartbeatPort();
    emit linkSettingsChanged();
}

void AppState::connectProtocol()
{
    m_protocolClient->setRobotId(m_telemetryStore->currentVehicleId());
    m_protocolClient->start();
}

void AppState::disconnectProtocol()
{
    m_protocolClient->stop();
}

bool AppState::testProtocol()
{
    return m_protocolClient->testConnection();
}

QVariantList AppState::toVariantList(const QList<QPointF> &points) const
{
    QVariantList list;
    list.reserve(points.size());
    for (const QPointF &point : points) {
        QVariantMap item;
        item.insert("x", point.x());
        item.insert("y", point.y());
        list.append(item);
    }
    return list;
}

void AppState::syncFromStore()
{
    m_vehicleName = m_telemetryStore->vehicleName();
    m_flightStatus = m_telemetryStore->flightStatus();
    m_flightMode = m_telemetryStore->flightMode();
    m_controllerMode = m_telemetryStore->controllerMode();
    m_controlState = m_telemetryStore->controlState();
    m_locationSource = m_telemetryStore->locationSource();
    m_gpsStatus = m_telemetryStore->gpsStatus();
    m_heartbeatLink = m_telemetryStore->heartbeatLink();
    m_videoLink = m_telemetryStore->videoLink();
    m_rcLink = m_telemetryStore->rcLink();
    m_armed = m_telemetryStore->armed();
    m_connected = m_telemetryStore->connected();
    m_failsafe = m_telemetryStore->failsafe();
    m_batteryVoltage = m_telemetryStore->batteryVoltage();
    m_batteryPercent = m_telemetryStore->batteryPercent();
    m_altitude = m_telemetryStore->altitude();
    m_speed = m_telemetryStore->speed();
    m_positionX = m_telemetryStore->positionX();
    m_positionY = m_telemetryStore->positionY();
    m_positionZ = m_telemetryStore->positionZ();
    m_velocityX = m_telemetryStore->velocityX();
    m_velocityY = m_telemetryStore->velocityY();
    m_velocityZ = m_telemetryStore->velocityZ();
    m_heading = m_telemetryStore->heading();
    m_roll = m_telemetryStore->roll();
    m_pitch = m_telemetryStore->pitch();
    m_yaw = m_telemetryStore->yaw();
    m_desiredPositionX = m_telemetryStore->desiredPositionX();
    m_desiredPositionY = m_telemetryStore->desiredPositionY();
    m_desiredPositionZ = m_telemetryStore->desiredPositionZ();
    m_desiredVelocityX = m_telemetryStore->desiredVelocityX();
    m_desiredVelocityY = m_telemetryStore->desiredVelocityY();
    m_desiredVelocityZ = m_telemetryStore->desiredVelocityZ();
    m_homeDistance = m_telemetryStore->homeDistance();
    m_missionStage = m_telemetryStore->missionStage();
    m_videoStatus = m_telemetryStore->videoStatus();
    m_alertLevel = m_telemetryStore->alertLevel();
    m_currentTime = m_telemetryStore->currentTime();
    m_lastCommand = m_telemetryStore->lastCommand();
    m_commandAck = m_telemetryStore->commandAck();
    m_pathPoints = m_telemetryStore->pathPoints();
    m_waypointPoints = m_telemetryStore->waypointPoints();
}

void AppState::bootstrapDemoTelemetry()
{
    QVariantMap uavState;
    uavState.insert("connected", true);
    uavState.insert("armed", true);
    uavState.insert("mode", "OFFBOARD");
    uavState.insert("location_source", 0);
    uavState.insert("gps_status", 3);
    uavState.insert("battery_state", 23.4);
    uavState.insert("battery_percetage", 0.78);
    uavState.insert("altitude", 18.6);
    uavState.insert("rel_alt", 18.6);
    uavState.insert("range", 17.9);
    uavState.insert("position", QVariantList{12.4, 6.8, 18.6});
    uavState.insert("velocity", QVariantList{2.5, 4.3, 0.1});
    uavState.insert("attitude", QVariantList{1.8, -3.4, 1.26});
    uavState.insert("position_ref", QVariantList{15.0, 8.0, 20.0});
    uavState.insert("velocity_ref", QVariantList{1.2, 0.0, 0.0});
    uavState.insert("video_status", "RTSP 1920x1080 / 30 FPS");
    m_telemetryStore->applyUavState(uavState, 1);

    QVariantMap controlState;
    controlState.insert("control_state", 2);
    controlState.insert("pos_controller", 0);
    controlState.insert("failsafe", false);
    m_telemetryStore->applyUavControlState(controlState);

    QVariantMap heartbeat;
    heartbeat.insert("count", 1);
    heartbeat.insert("message", "Zenith Link Ready");
    m_telemetryStore->applyHeartbeat(heartbeat);
}
