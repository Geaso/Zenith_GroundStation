#include "AppState.h"

#include "CommandDispatcher.h"
#include "FlightRecorder.h"
#include "ParamStore.h"
#include "TelemetryStore.h"
#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"

#include <QDateTime>
#include <QSettings>

AppState::AppState(QObject *parent)
    : QObject(parent)
{
    m_telemetryStore = new TelemetryStore(this);
    m_protocolClient = new ZenithProtocolClient(this);
    m_commandDispatcher = new CommandDispatcher(m_telemetryStore, m_protocolClient, this);
    m_flightRecorder = new FlightRecorder(m_telemetryStore, this);
    m_paramStore = new ParamStore(this);

    connect(m_telemetryStore, &TelemetryStore::telemetryChanged, this, [this]() {
        syncFromStore();
        emit telemetryChanged();
    });
    connect(m_telemetryStore, &TelemetryStore::pathChanged, this, [this]() {
        syncFromStore();
        emit pathChanged();
    });
    connect(m_protocolClient, &ZenithProtocolClient::linkStatesChanged, this, [this]() {
        m_telemetryStore->setTransportHealth(
            m_protocolClient->telemetryFresh(),
            m_protocolClient->heartbeatFresh(),
            m_protocolClient->connectionSummary());
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
        emit linkStateChanged();
        emit linkSettingsChanged();
    });
    connect(m_protocolClient, &ZenithProtocolClient::decodedMessage, this, [this](int msgId, int robotId, const QVariantMap &payload) {
        switch (msgId) {
        case ZenithProtocol::UAVSTATE:
            m_telemetryStore->applyUavState(payload, robotId);
            break;
        case ZenithProtocol::TEXTINFO:
        {
            const QString msg = payload.value("Message").toString();
            if (msg.startsWith(QLatin1String("MODESELECTION_ACK:"))) {
                m_protocolClient->noteModeSelectionAck();
            }
            m_telemetryStore->applyTextInfo(payload);
            break;
        }
        case ZenithProtocol::HEARTBEAT:
            m_telemetryStore->applyHeartbeat(payload);
            break;
        case ZenithProtocol::UAVCONTROLSTATE:
            m_telemetryStore->applyUavControlState(payload);
            break;
        case ZenithProtocol::PARAMSETTINGS:
            m_paramStore->applyParamSettings(payload);
            break;
        default:
            break;
        }
    });
    if (QSettings().value("demoMode", false).toBool()) {
        bootstrapDemoTelemetry();
    }
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

    // Load last-used connection profile
    QString lastProfile = lastUsedProfile();
    if (!lastProfile.isEmpty()) {
        QVariantMap p = loadConnectionProfile(lastProfile);
        if (!p.isEmpty()) {
            applyConnectionSettings(p.value("ip").toString(),
                                    p.value("udp").toInt(),
                                    p.value("tcp").toInt(),
                                    p.value("heartbeat").toInt());
        }
    }
}

AppState::~AppState() = default;

QString AppState::vehicleName() const { return m_vehicleName; }
QString AppState::flightStatus() const { return m_flightStatus; }
QString AppState::flightMode() const { return m_flightMode; }
QString AppState::controllerMode() const { return m_controllerMode; }
QString AppState::controlState() const { return m_controlState; }
QString AppState::execState() const { return m_execState; }
QString AppState::missionMode() const { return m_missionMode; }
QString AppState::activeCommandSource() const { return m_activeCommandSource; }
QString AppState::pendingRequest() const { return m_pendingRequest; }
bool AppState::requestActive() const { return m_requestActive; }
QString AppState::locationSource() const { return m_locationSource; }
QString AppState::gpsStatus() const { return m_gpsStatus; }
int AppState::gpsFix() const { return m_gpsFixType; }
int AppState::locationSourceId() const { return m_locationSourceId; }
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

void AppState::startModule(const QString &moduleName)
{
    static const QMap<QString, QString> moduleScripts = {
        {"ODIN",            "/home/jetson/Zenith_ws/scripts/modules/start_odin.sh"},
        {"OAK_VIO",         "/home/jetson/Zenith_ws/scripts/modules/start_oakvio.sh"},
        {"D435i",           "/home/jetson/Zenith_ws/scripts/modules/start_d435i.sh"},
        {"Zenith_ODIN",     "/home/jetson/Zenith_ws/scripts/modules/start_zenith_control.sh /home/jetson/Zenith_ws/src/zenith_control/launch/profiles/indoor_odin.yaml"},
        {"Zenith_OAKVIO",   "/home/jetson/Zenith_ws/scripts/modules/start_zenith_control.sh /home/jetson/Zenith_ws/src/zenith_control/launch/profiles/indoor_vins.yaml"},
        {"Zenith_OPENVINS", "/home/jetson/Zenith_ws/scripts/modules/start_zenith_control.sh /home/jetson/Zenith_ws/src/zenith_control/launch/profiles/indoor_openvins.yaml"},
        {"RESTART_FCU",     "/home/jetson/Zenith_ws/scripts/modules/restart_fcu.sh"},
    };

    const QString script = moduleScripts.value(moduleName);
    if (script.isEmpty()) return;

    m_commandDispatcher->executeRemoteCommand(moduleName, script);
    emit commandTriggered(QString("StartModule: %1").arg(moduleName));
}

void AppState::stopModule(const QString &moduleName)
{
    static const QMap<QString, QString> moduleNodePatterns = {
        {"ODIN",            "odin1"},
        {"OAK_VIO",         "oakchina_vio"},
        {"D435i",           "realsense2_camera"},
        {"Zenith_Control",  "uav_control_main"},
    };

    const QString pattern = moduleNodePatterns.value(moduleName);
    if (pattern.isEmpty()) return;

    m_commandDispatcher->stopRemoteModule(moduleName, pattern);
    emit commandTriggered(QString("StopModule: %1").arg(moduleName));
}

void AppState::executeRemoteCommand(const QString &name, const QString &command)
{
    m_commandDispatcher->executeRemoteCommand(name, command);
    emit commandTriggered(QString("RemoteExec: %1").arg(name));
}

bool AppState::isModuleRunning(const QString &nodePattern) const
{
    const QStringList nodes = m_telemetryStore->runningNodes();
    for (const auto &n : nodes) {
        if (n.contains(nodePattern))
            return true;
    }
    return false;
}

QStringList AppState::runningNodes() const
{
    return m_telemetryStore->runningNodes();
}

QString AppState::moduleExecFeedback() const
{
    return m_telemetryStore->moduleExecFeedback();
}

void AppState::armVehicle(bool arm)
{
    m_commandDispatcher->armVehicle(arm);
    emit commandTriggered(arm ? "Arm" : "Disarm");
}

void AppState::setPx4Mode(const QString &mode)
{
    m_commandDispatcher->setPx4Mode(mode);
    emit commandTriggered(QString("PX4 Mode: %1").arg(mode));
}

void AppState::switchLocationSource(int sourceIndex)
{
    m_commandDispatcher->switchLocationSource(sourceIndex);
    emit commandTriggered(QString("Switch Location Source %1").arg(sourceIndex));
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

void AppState::startRecording()
{
    m_flightRecorder->start(QStringLiteral("flight_logs"));
    emit recordingChanged();
}

void AppState::stopRecording()
{
    m_flightRecorder->stop();
    emit recordingChanged();
}

void AppState::requestParams(int module)
{
    QVariantMap payload;
    payload.insert("param_module", module);
    payload.insert("params", QVariantList{});
    m_protocolClient->sendTcpMessage(ZenithProtocol::PARAMSETTINGS, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(QString("Request Params module=%1").arg(module), "ParamSettings queued");
}

void AppState::uploadDirtyParams()
{
    const auto dirty = m_paramStore->dirtyEntries();
    if (dirty.isEmpty()) return;

    QVariantList paramsList;
    for (const auto &entry : dirty) {
        QVariantMap pm;
        pm.insert("type", entry.type);
        pm.insert("param_name", entry.name);
        pm.insert("param_value", entry.value);
        paramsList.append(pm);
    }
    QVariantMap payload;
    payload.insert("param_module", 6); // SEARCHMODIFY
    payload.insert("params", paramsList);
    m_protocolClient->sendTcpMessage(ZenithProtocol::PARAMSETTINGS, payload, m_telemetryStore->currentVehicleId());
    m_paramStore->clearDirty();
    m_telemetryStore->setCommandFeedback("Upload Params", QString("%1 params uploaded").arg(dirty.size()));
}

QObject *AppState::paramStore() const { return m_paramStore; }

bool AppState::recording() const { return m_flightRecorder->isRecording(); }
QString AppState::recordingFile() const { return m_flightRecorder->filePath(); }
int AppState::recordingSamples() const { return m_flightRecorder->sampleCount(); }
double AppState::recordingElapsed() const { return m_flightRecorder->elapsedSeconds(); }

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
    m_execState = m_telemetryStore->execState();
    m_missionMode = m_telemetryStore->missionMode();
    m_activeCommandSource = m_telemetryStore->activeCommandSource();
    m_pendingRequest = m_telemetryStore->pendingRequest();
    m_requestActive = m_telemetryStore->requestActive();
    m_locationSource = m_telemetryStore->locationSource();
    m_gpsStatus = m_telemetryStore->gpsStatus();
    m_gpsFixType = m_telemetryStore->gpsFixType();
    m_locationSourceId = m_telemetryStore->locationSourceId();
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

// ── Connection profile persistence ──

QVariantList AppState::connectionProfiles() const
{
    QSettings settings;
    QVariantList profiles;
    int size = settings.beginReadArray("ConnectionProfiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QVariantMap p;
        p.insert("name", settings.value("name").toString());
        p.insert("ip", settings.value("ip").toString());
        p.insert("udp", settings.value("udp").toInt());
        p.insert("tcp", settings.value("tcp").toInt());
        p.insert("heartbeat", settings.value("heartbeat").toInt());
        profiles.append(p);
    }
    settings.endArray();
    return profiles;
}

void AppState::saveConnectionProfile(const QString &name, const QString &ip, int udpPort, int tcpPort, int heartbeatPort)
{
    QSettings settings;

    // Read existing profiles
    QList<QVariantMap> profiles;
    int size = settings.beginReadArray("ConnectionProfiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QVariantMap p;
        p.insert("name", settings.value("name").toString());
        p.insert("ip", settings.value("ip").toString());
        p.insert("udp", settings.value("udp").toInt());
        p.insert("tcp", settings.value("tcp").toInt());
        p.insert("heartbeat", settings.value("heartbeat").toInt());
        profiles.append(p);
    }
    settings.endArray();

    // Update or append
    bool found = false;
    for (auto &p : profiles) {
        if (p.value("name").toString() == name) {
            p.insert("ip", ip);
            p.insert("udp", udpPort);
            p.insert("tcp", tcpPort);
            p.insert("heartbeat", heartbeatPort);
            found = true;
            break;
        }
    }
    if (!found) {
        QVariantMap p;
        p.insert("name", name);
        p.insert("ip", ip);
        p.insert("udp", udpPort);
        p.insert("tcp", tcpPort);
        p.insert("heartbeat", heartbeatPort);
        profiles.append(p);
    }

    // Write back
    settings.beginWriteArray("ConnectionProfiles", profiles.size());
    for (int i = 0; i < profiles.size(); ++i) {
        settings.setArrayIndex(i);
        const auto &p = profiles.at(i);
        settings.setValue("name", p.value("name"));
        settings.setValue("ip", p.value("ip"));
        settings.setValue("udp", p.value("udp"));
        settings.setValue("tcp", p.value("tcp"));
        settings.setValue("heartbeat", p.value("heartbeat"));
    }
    settings.endArray();

    settings.setValue("lastUsedProfile", name);
    settings.sync();
    emit profilesChanged();
}

void AppState::deleteConnectionProfile(const QString &name)
{
    QSettings settings;

    QList<QVariantMap> profiles;
    int size = settings.beginReadArray("ConnectionProfiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QVariantMap p;
        p.insert("name", settings.value("name").toString());
        p.insert("ip", settings.value("ip").toString());
        p.insert("udp", settings.value("udp").toInt());
        p.insert("tcp", settings.value("tcp").toInt());
        p.insert("heartbeat", settings.value("heartbeat").toInt());
        if (p.value("name").toString() != name)
            profiles.append(p);
    }
    settings.endArray();

    settings.beginWriteArray("ConnectionProfiles", profiles.size());
    for (int i = 0; i < profiles.size(); ++i) {
        settings.setArrayIndex(i);
        const auto &p = profiles.at(i);
        settings.setValue("name", p.value("name"));
        settings.setValue("ip", p.value("ip"));
        settings.setValue("udp", p.value("udp"));
        settings.setValue("tcp", p.value("tcp"));
        settings.setValue("heartbeat", p.value("heartbeat"));
    }
    settings.endArray();

    if (settings.value("lastUsedProfile").toString() == name)
        settings.remove("lastUsedProfile");

    settings.sync();
    emit profilesChanged();
}

QVariantMap AppState::loadConnectionProfile(const QString &name) const
{
    QSettings settings;
    int size = settings.beginReadArray("ConnectionProfiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        if (settings.value("name").toString() == name) {
            QVariantMap p;
            p.insert("name", name);
            p.insert("ip", settings.value("ip").toString());
            p.insert("udp", settings.value("udp").toInt());
            p.insert("tcp", settings.value("tcp").toInt());
            p.insert("heartbeat", settings.value("heartbeat").toInt());
            settings.endArray();
            return p;
        }
    }
    settings.endArray();
    return {};
}

QString AppState::lastUsedProfile() const
{
    return QSettings().value("lastUsedProfile").toString();
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
    controlState.insert("exec_state", 3);           // AUTO_HOLD
    controlState.insert("mission_mode", 1);          // HOVER
    controlState.insert("active_command_source", 5); // GROUND_STATION
    controlState.insert("pending_request", 0);       // NONE
    controlState.insert("request_active", false);
    m_telemetryStore->applyUavControlState(controlState);

    QVariantMap heartbeat;
    heartbeat.insert("count", 1);
    heartbeat.insert("message", "Zenith Link Ready");
    m_telemetryStore->applyHeartbeat(heartbeat);
}
