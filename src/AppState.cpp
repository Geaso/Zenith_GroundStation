#include "AppState.h"

#include "CommandDispatcher.h"
#include "FlightRecorder.h"
#include "ParamStore.h"
#include "TelemetryStore.h"
#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <cmath>

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
    connect(m_telemetryStore, &TelemetryStore::gridMapChanged, this, [this]() {
        ++m_gridMapVersion;
        emit gridMapChanged();
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
        // 只要收到任一 CRC 通过的帧就算"已对频" —— 这是最快的链路反馈，
        // 不依赖任何业务数据，上电几秒内即可点亮。
        m_telemetryStore->noteFrameReceived();
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
        case ZenithProtocol::CUSTOMDATASEGMENT_1:
            // 机载 preflight_reporter 用它上报 PX4 SYS_STATUS 传感器健康位
            m_telemetryStore->applyCustomDataSegment(payload);
            break;
        case ZenithProtocol::GRIDMAP:
            m_telemetryStore->applyGridMap(payload);
            break;
        case ZenithProtocol::PLANNEDPATH:
            m_telemetryStore->applyPlannedPath(payload);
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
            if (p.value("type").toString() == "serial") {
                applySerialSettings(p.value("portName").toString(),
                                    p.value("baudRate").toInt());
            } else {
                applyConnectionSettings(p.value("ip").toString(),
                                        p.value("udp").toInt(),
                                        p.value("tcp").toInt(),
                                        p.value("heartbeat").toInt());
            }
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
double AppState::vinsPositionX() const { return m_vinsPositionX; }
double AppState::vinsPositionY() const { return m_vinsPositionY; }
double AppState::vinsPositionZ() const { return m_vinsPositionZ; }
bool AppState::odomValid() const { return m_odomValid; }
bool AppState::preflightValid() const { return m_preflightValid; }
bool AppState::preflightArmOk() const { return m_preflightArmOk; }
QString AppState::preflightFail() const { return m_preflightFail; }
QVariantList AppState::preflightChecks() const { return m_preflightChecks; }
bool AppState::preflightPrearmBit() const { return m_preflightPrearmBit; }
int AppState::preflightArmAck() const { return m_preflightArmAck; }
QString AppState::preflightArmAckText() const { return m_preflightArmAckText; }
QString AppState::missionLogText() const { return m_missionLogText; }
bool AppState::linkEstablished() const { return m_linkEstablished; }
bool AppState::batteryValid() const { return m_batteryValid; }
bool AppState::telemetryStable() const { return m_telemetryStable; }
bool AppState::allReady() const { return m_allReady; }
int  AppState::aircraftUptime() const { return m_aircraftUptime; }
QVariantList AppState::readinessSteps() const { return m_readinessSteps; }
void AppState::clearMissionLog() { m_telemetryStore->clearMissionLog(); }
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
QString AppState::managedTaskName() const { return m_telemetryStore->managedTaskName(); }
QString AppState::managedTaskPath() const { return m_telemetryStore->managedTaskPath(); }
QString AppState::managedTaskState() const { return m_telemetryStore->managedTaskState(); }
QString AppState::managedTaskReason() const { return m_telemetryStore->managedTaskReason(); }
QString AppState::managedTaskAck() const { return m_telemetryStore->managedTaskAck(); }
QString AppState::managedTaskRequestId() const { return m_telemetryStore->managedTaskRequestId(); }
bool AppState::managedTaskActive() const { return m_telemetryStore->managedTaskActive(); }
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

void AppState::startManagedTask(const QString &taskName)
{
    m_commandDispatcher->sendManagedTaskRequest(
        taskName, QStringLiteral("START"),
        taskName == QLatin1String("zenith_tracking_unified"));
    emit commandTriggered(QStringLiteral("Task START: %1").arg(taskName));
}

bool AppState::startCustomManagedTask(const QString &taskName, const QString &taskPath)
{
    const QString normalizedName = taskName.trimmed();
    QString normalizedPath = taskPath.trimmed();
    normalizedPath.replace(QLatin1Char('\\'), QLatin1Char('/'));

    static const QRegularExpression taskNamePattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$"));
    if (!taskNamePattern.match(normalizedName).hasMatch()) {
        m_telemetryStore->setCommandFeedback(
            QStringLiteral("Custom Task"),
            QStringLiteral("Blocked: 任务 ID 只能包含字母、数字、点、下划线和短横线（最多 64 字符）"));
        return false;
    }
    if (!normalizedPath.startsWith(QLatin1Char('/')) ||
        normalizedPath.size() > 512 || normalizedPath.contains(QLatin1Char('\n')) ||
        normalizedPath.contains(QLatin1Char('\r'))) {
        m_telemetryStore->setCommandFeedback(
            QStringLiteral("Custom Task"),
            QStringLiteral("Blocked: 请输入 Jetson 上不超过 512 字符的绝对路径"));
        return false;
    }

    m_commandDispatcher->sendManagedTaskRequest(
        normalizedName, QStringLiteral("START"), false, normalizedPath);
    emit commandTriggered(
        QStringLiteral("Custom Task START: %1 (%2)").arg(normalizedName, normalizedPath));
    return true;
}

void AppState::stopManagedTask(const QString &taskName)
{
    m_commandDispatcher->sendManagedTaskRequest(taskName, QStringLiteral("STOP"));
    emit commandTriggered(QStringLiteral("Task STOP: %1").arg(taskName));
}

void AppState::queryManagedTask()
{
    m_commandDispatcher->sendManagedTaskRequest(QString(), QStringLiteral("STATUS"));
}

void AppState::sendRemoteScript(const QString &cmd)
{
    QVariantMap payload;
    payload.insert("mode", ZenithProtocol::CUSTOMMODE_MODE);
    payload.insert("selectId", QVariantList{m_telemetryStore->currentVehicleId()});
    payload.insert("use_mode", ZenithProtocol::UM_CREATE);
    payload.insert("is_simulation", false);
    payload.insert("swarm_num", 1);
    payload.insert("cmd", cmd);
    m_protocolClient->sendTcpMessage(ZenithProtocol::MODESELECTION, payload, m_telemetryStore->currentVehicleId());
    m_telemetryStore->setCommandFeedback(QString("Remote: %1").arg(cmd), "Sent");
    emit commandTriggered(QString("Remote Script: %1").arg(cmd));
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
    m_protocolClient->setTransportMode(0); // Network
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

void AppState::applySerialSettings(const QString &portName, int baudRate)
{
    m_protocolClient->setTransportMode(1); // Serial
    m_protocolClient->setSerialPortName(portName);
    m_protocolClient->setSerialBaudRate(baudRate);
    emit linkSettingsChanged();
}

QObject *AppState::protocolClientObj() const
{
    return m_protocolClient;
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
    m_vinsPositionX = m_telemetryStore->vinsPositionX();
    m_vinsPositionY = m_telemetryStore->vinsPositionY();
    m_vinsPositionZ = m_telemetryStore->vinsPositionZ();
    m_odomValid = m_telemetryStore->odomValid();
    m_preflightValid = m_telemetryStore->preflightValid();
    m_preflightArmOk = m_telemetryStore->preflightArmOk();
    m_preflightFail = m_telemetryStore->preflightFail();
    m_preflightChecks = m_telemetryStore->preflightChecks();
    m_preflightPrearmBit = m_telemetryStore->preflightPrearmBit();
    m_preflightArmAck = m_telemetryStore->preflightArmAck();
    m_preflightArmAckText = m_telemetryStore->preflightArmAckText();
    m_missionLogText = m_telemetryStore->missionLogText();
    m_linkEstablished = m_telemetryStore->linkEstablished();
    m_telemetryStable = m_telemetryStore->telemetryStable();
    m_batteryValid = m_telemetryStore->batteryValid();
    m_aircraftUptime = m_telemetryStore->aircraftUptime();
    m_readinessSteps = m_telemetryStore->readinessSteps();
    m_allReady = m_telemetryStore->fcuReady() && m_telemetryStore->batteryValid()
              && m_telemetryStore->locReady() && m_telemetryStore->ctrlReady();
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

    // 航线任务推进：检查是否到达当前航点的 Command_ID
    checkMissionProgress();
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
        QString type = settings.value("type", "tcp").toString();
        p.insert("type", type);
        if (type == "serial") {
            p.insert("portName", settings.value("portName").toString());
            p.insert("baudRate", settings.value("baudRate").toInt());
        } else {
            p.insert("ip", settings.value("ip").toString());
            p.insert("udp", settings.value("udp").toInt());
            p.insert("tcp", settings.value("tcp").toInt());
            p.insert("heartbeat", settings.value("heartbeat").toInt());
        }
        profiles.append(p);
    }
    settings.endArray();
    return profiles;
}

// Helper: read all profiles from QSettings into a list
static QList<QVariantMap> readAllProfiles(QSettings &settings)
{
    QList<QVariantMap> profiles;
    int size = settings.beginReadArray("ConnectionProfiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QVariantMap p;
        p.insert("name", settings.value("name").toString());
        QString type = settings.value("type", "tcp").toString();
        p.insert("type", type);
        if (type == "serial") {
            p.insert("portName", settings.value("portName").toString());
            p.insert("baudRate", settings.value("baudRate").toInt());
        } else {
            p.insert("ip", settings.value("ip").toString());
            p.insert("udp", settings.value("udp").toInt());
            p.insert("tcp", settings.value("tcp").toInt());
            p.insert("heartbeat", settings.value("heartbeat").toInt());
        }
        profiles.append(p);
    }
    settings.endArray();
    return profiles;
}

// Helper: write all profiles back to QSettings
static void writeAllProfiles(QSettings &settings, const QList<QVariantMap> &profiles)
{
    settings.beginWriteArray("ConnectionProfiles", profiles.size());
    for (int i = 0; i < profiles.size(); ++i) {
        settings.setArrayIndex(i);
        const auto &p = profiles.at(i);
        settings.setValue("name", p.value("name"));
        QString type = p.value("type", "tcp").toString();
        settings.setValue("type", type);
        if (type == "serial") {
            settings.setValue("portName", p.value("portName"));
            settings.setValue("baudRate", p.value("baudRate"));
        } else {
            settings.setValue("ip", p.value("ip"));
            settings.setValue("udp", p.value("udp"));
            settings.setValue("tcp", p.value("tcp"));
            settings.setValue("heartbeat", p.value("heartbeat"));
        }
    }
    settings.endArray();
}

void AppState::saveConnectionProfile(const QString &name, const QString &ip, int udpPort, int tcpPort, int heartbeatPort)
{
    QSettings settings;
    QList<QVariantMap> profiles = readAllProfiles(settings);

    // Update or append
    bool found = false;
    for (auto &p : profiles) {
        if (p.value("name").toString() == name) {
            p.insert("type", QStringLiteral("tcp"));
            p.insert("ip", ip);
            p.insert("udp", udpPort);
            p.insert("tcp", tcpPort);
            p.insert("heartbeat", heartbeatPort);
            // Remove serial fields if profile was previously serial
            p.remove("portName");
            p.remove("baudRate");
            found = true;
            break;
        }
    }
    if (!found) {
        QVariantMap p;
        p.insert("name", name);
        p.insert("type", QStringLiteral("tcp"));
        p.insert("ip", ip);
        p.insert("udp", udpPort);
        p.insert("tcp", tcpPort);
        p.insert("heartbeat", heartbeatPort);
        profiles.append(p);
    }

    writeAllProfiles(settings, profiles);

    settings.setValue("lastUsedProfile", name);
    settings.sync();
    emit profilesChanged();
}

void AppState::saveSerialConnectionProfile(const QString &name, const QString &portName, int baudRate)
{
    QSettings settings;
    QList<QVariantMap> profiles = readAllProfiles(settings);

    bool found = false;
    for (auto &p : profiles) {
        if (p.value("name").toString() == name) {
            p.insert("type", QStringLiteral("serial"));
            p.insert("portName", portName);
            p.insert("baudRate", baudRate);
            // Remove TCP fields if profile was previously TCP
            p.remove("ip");
            p.remove("udp");
            p.remove("tcp");
            p.remove("heartbeat");
            found = true;
            break;
        }
    }
    if (!found) {
        QVariantMap p;
        p.insert("name", name);
        p.insert("type", QStringLiteral("serial"));
        p.insert("portName", portName);
        p.insert("baudRate", baudRate);
        profiles.append(p);
    }

    writeAllProfiles(settings, profiles);

    settings.setValue("lastUsedProfile", name);
    settings.sync();
    emit profilesChanged();
}

void AppState::deleteConnectionProfile(const QString &name)
{
    QSettings settings;
    QList<QVariantMap> allProfiles = readAllProfiles(settings);

    QList<QVariantMap> filtered;
    for (const auto &p : allProfiles) {
        if (p.value("name").toString() != name)
            filtered.append(p);
    }

    writeAllProfiles(settings, filtered);

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
            QString type = settings.value("type", "tcp").toString();
            p.insert("type", type);
            if (type == "serial") {
                p.insert("portName", settings.value("portName").toString());
                p.insert("baudRate", settings.value("baudRate").toInt());
            } else {
                p.insert("ip", settings.value("ip").toString());
                p.insert("udp", settings.value("udp").toInt());
                p.insert("tcp", settings.value("tcp").toInt());
                p.insert("heartbeat", settings.value("heartbeat").toInt());
            }
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

// ─────────────────────────────────────────────────────────────
//  航线任务（waypoint mission）：地面站按 callback 顺序下发
//  - yaw 按 B 方案：每点朝向下一点（atan2），末点沿用倒数第二段方向
//  - Command_ID bit31 编码 waypoint_mission（true 表示后面还有航点）
//  - 到达检测在飞机端，回传 last_reached_waypoint_id；超时未推进 → FAILSAFE LAND
// ─────────────────────────────────────────────────────────────
QString AppState::missionState() const { return m_missionState; }
int AppState::missionCurrentIndex() const { return m_missionCurrentIndex; }
int AppState::missionTotal() const { return m_missionWaypoints.size(); }

void AppState::startMission(const QVariantList &waypoints)
{
    if (waypoints.isEmpty()) {
        m_telemetryStore->setCommandFeedback("Mission", "Empty waypoint list");
        return;
    }
    if (!m_protocolClient->canSendControlCommands()) {
        m_telemetryStore->setCommandFeedback("Mission", "Blocked: link not CONNECTED");
        return;
    }

    m_missionWaypoints.clear();
    const int n = waypoints.size();
    // 默认 yaw = 当前 yaw（单点 mission 或路径退化时不改变姿态）
    const double currentYaw = m_yaw;
    for (int i = 0; i < n; ++i) {
        const QVariantMap wp = waypoints.at(i).toMap();
        const double x = wp.value("wx").toDouble();
        const double y = wp.value("wy").toDouble();
        const double z = wp.value("wz").toDouble();
        double yawRad = currentYaw;
        if (i + 1 < n) {
            const QVariantMap next = waypoints.at(i + 1).toMap();
            const double dx = next.value("wx").toDouble() - x;
            const double dy = next.value("wy").toDouble() - y;
            if (std::hypot(dx, dy) > 0.05) yawRad = std::atan2(dy, dx);
        } else if (i > 0) {
            const QVariantMap prev = waypoints.at(i - 1).toMap();
            const double dx = x - prev.value("wx").toDouble();
            const double dy = y - prev.value("wy").toDouble();
            if (std::hypot(dx, dy) > 0.05) yawRad = std::atan2(dy, dx);
        }
        QVariantMap entry;
        entry.insert("x", x);
        entry.insert("y", y);
        entry.insert("z", z);
        entry.insert("yaw", yawRad);
        m_missionWaypoints.append(entry);
    }

    m_lastSeenReachedId = m_telemetryStore->lastReachedWaypointId();
    m_missionCurrentIndex = 0;
    m_missionState = QStringLiteral("RUNNING");
    sendNextMissionWaypoint();
    emit missionStateChanged();
}

void AppState::abortMission()
{
    if (m_missionState != QStringLiteral("RUNNING")) return;
    m_missionState = QStringLiteral("ABORTED");
    m_missionCurrentIndex = -1;
    m_missionExpectedReachId = 0;
    m_commandDispatcher->issueQuickAction(QStringLiteral("Hover Here"));
    emit missionStateChanged();
}

void AppState::sendNextMissionWaypoint()
{
    const int n = m_missionWaypoints.size();
    if (m_missionCurrentIndex < 0 || m_missionCurrentIndex >= n) return;
    const QVariantMap wp = m_missionWaypoints.at(m_missionCurrentIndex).toMap();
    const bool isContinuation = (m_missionCurrentIndex < n - 1);
    m_missionExpectedReachId = m_commandDispatcher->sendWaypoint(
        wp.value("x").toDouble(),
        wp.value("y").toDouble(),
        wp.value("z").toDouble(),
        wp.value("yaw").toDouble(),
        isContinuation);
}

// ─────────────────────────────────────────────────────────────
//  一键起飞 — 在位置模式下解锁，切 OFFBOARD，飞到指定高度悬停
//  序列总时长 ~3s，全程预检 + 状态保护
// ─────────────────────────────────────────────────────────────
void AppState::takeoffTo(double heightMeters)
{
    // 边界裁剪：禁止 0.3-3m 之外的高度
    if (heightMeters < 0.3 || heightMeters > 3.0) {
        m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"),
            QStringLiteral("Blocked: height %1m out of [0.3, 3.0]").arg(heightMeters));
        return;
    }
    // 安全检查 1：飞机必须 disarmed（防止误触把飞行中的飞机再 arm）
    if (m_armed) {
        m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"), QStringLiteral("Blocked: 已解锁"));
        return;
    }
    // 安全检查 2：必须有有效定位
    if (!m_odomValid) {
        m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"), QStringLiteral("Blocked: odom 无效（VINS 未就绪）"));
        return;
    }
    // 安全检查 3：飞控连接通
    if (!m_connected) {
        m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"), QStringLiteral("Blocked: 飞控未连接"));
        return;
    }
    // 安全检查 4：链路可发指令
    if (!m_protocolClient->canSendControlCommands()) {
        m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"), QStringLiteral("Blocked: link 未 CONNECTED"));
        return;
    }
    // 安全检查 5：FAILSAFE 不能在告警态起飞
    if (m_failsafe) {
        m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"), QStringLiteral("Blocked: FAILSAFE 中"));
        return;
    }

    // 锁定当前 ENU 位置作为起飞点（防止 arm 期间 odom 抖动飘）
    const double startX = m_positionX;
    const double startY = m_positionY;
    const double startYaw = m_yaw;

    m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"),
        QStringLiteral("Sequence start → POSCTL → arm → OFFBOARD → climb to %1m").arg(heightMeters));

    // T+0:     切 POSCTL（确保从已知安全态起飞）
    m_commandDispatcher->setPx4Mode(QStringLiteral("POSCTL"));

    // T+400ms: arm
    QTimer::singleShot(400, this, [this]() {
        if (m_armed) return; // 上一拍可能已经 arm 过
        m_commandDispatcher->armVehicle(true);
    });

    // T+1500ms: 切 OFFBOARD（armed 状态下 Zenith FSM 会进 COMMAND_CONTROL）
    QTimer::singleShot(1500, this, [this]() {
        if (!m_armed) {
            m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"), QStringLiteral("Abort: arm 失败"));
            return;
        }
        m_commandDispatcher->setPx4Mode(QStringLiteral("OFFBOARD"));
    });

    // T+2500ms: 用 Init_Pos_Hover (Agent_CMD=1)，飞机端 px4ctrl 路径已验证。
    // Init_Pos_Hover 目标 = Takeoff_position + (0,0,Takeoff_height)，由 lingkong1_opi.yaml 控制。
    // 之前 sendManualMove(XYZ_POS) 在 pos_controller=PX4_CTRL 下不驱动起飞（log_22 验证）。
    Q_UNUSED(heightMeters); Q_UNUSED(startX); Q_UNUSED(startY); Q_UNUSED(startYaw);
    QTimer::singleShot(2500, this, [this]() {
        if (!m_armed) {
            m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"), QStringLiteral("Abort: 序列中飞机已被 disarmed"));
            return;
        }
        m_commandDispatcher->issueQuickAction(QStringLiteral("初始点悬停"));  // Agent_CMD=1
        m_telemetryStore->setCommandFeedback(QStringLiteral("Takeoff"),
            QStringLiteral("起飞指令已发（Init_Pos_Hover，高度由 yaml Takeoff_height 控制）"));
    });
}

void AppState::checkMissionProgress()
{
    if (m_missionState != QStringLiteral("RUNNING")) return;
    const quint32 reached = m_telemetryStore->lastReachedWaypointId();
    if (reached == m_lastSeenReachedId) return;   // 没变化
    m_lastSeenReachedId = reached;
    if (reached == 0 || reached != m_missionExpectedReachId) return;  // 不是当前航点

    const int n = m_missionWaypoints.size();
    if (m_missionCurrentIndex >= n - 1) {
        // 末点到达 → 任务完成（飞机端自动悬停，因 waypoint_mission=false）
        m_missionState = QStringLiteral("DONE");
        m_missionCurrentIndex = -1;
        m_missionExpectedReachId = 0;
        emit missionStateChanged();
        return;
    }
    m_missionCurrentIndex++;
    sendNextMissionWaypoint();
    emit missionStateChanged();
}

double AppState::gridMapOriginX() const { return m_telemetryStore->gridMapOriginX(); }
double AppState::gridMapOriginY() const { return m_telemetryStore->gridMapOriginY(); }
double AppState::gridMapResolution() const { return m_telemetryStore->gridMapResolution(); }
int AppState::gridMapWidth() const { return m_telemetryStore->gridMapWidth(); }
int AppState::gridMapHeight() const { return m_telemetryStore->gridMapHeight(); }
