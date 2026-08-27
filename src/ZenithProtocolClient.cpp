#include "ZenithProtocolClient.h"
#include <QTextStream>
#include <QFile>
#include <QDebug>

#include "ZenithProtocol.h"
#include "ZenithMsgPack.h"

#include <QAbstractSocket>
#include <QDataStream>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QSettings>

#include <algorithm>

namespace {
constexpr char kMagic0 = 0x61;
constexpr char kMagic1 = 0x6D;
constexpr int kFrameOverhead = 10;
// 栅格帧最坏 35378 字节（133×133 格 × 2 字节 RLE）+ MsgPack 头，取 40KB 留余量。
constexpr quint32 kMaxPayloadSize = 40960;
// 重组缓冲上限必须大于单帧最大长度，否则大栅格帧在慢链路上还没收全就被当成
// "链路噪声" 清掉，表现为栅格永远不刷新。
constexpr int kMaxBufferSize = 65536;
}

ZenithProtocolClient::ZenithProtocolClient(QObject *parent)
    : QObject(parent)
{
    // Bypass system proxy for direct socket connections
    m_tcpSocket.setProxy(QNetworkProxy::NoProxy);
    m_udpSocket.setProxy(QNetworkProxy::NoProxy);

    // UDP telemetry
    connect(&m_udpSocket, &QUdpSocket::readyRead, this, [this]() {
        while (m_udpSocket.hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(static_cast<int>(m_udpSocket.pendingDatagramSize()));
            m_udpSocket.readDatagram(datagram.data(), datagram.size());
            noteUdpRx();
            processFrame(datagram);
        }
    });

    // Persistent TCP connection signals
    connect(&m_tcpSocket, &QTcpSocket::connected, this, &ZenithProtocolClient::onTcpConnected);
    connect(&m_tcpSocket, &QTcpSocket::disconnected, this, &ZenithProtocolClient::onTcpDisconnected);
    connect(&m_tcpSocket, &QTcpSocket::readyRead, this, &ZenithProtocolClient::onTcpReadyRead);
    connect(&m_tcpSocket, &QAbstractSocket::errorOccurred, this, &ZenithProtocolClient::onTcpError);

    // Heartbeat receiving (Jetson connects to us)
    connect(&m_heartbeatServer, &QTcpServer::newConnection, this, [this]() {
        while (m_heartbeatServer.hasPendingConnections()) {
            QTcpSocket *socket = m_heartbeatServer.nextPendingConnection();
            handleHeartbeatConnection(socket);
        }
    });

    // Reconnect timer
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &ZenithProtocolClient::onReconnectTimer);

    // Heartbeat sending timer
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &ZenithProtocolClient::onHeartbeatTimer);

    connect(&m_linkMonitorTimer, &QTimer::timeout, this, &ZenithProtocolClient::updateLinkStates);
    m_linkMonitorTimer.setInterval(kLinkMonitorIntervalMs);

    connect(&m_serialPort, &QSerialPort::readyRead, this, &ZenithProtocolClient::onSerialReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred, this, &ZenithProtocolClient::onSerialError);

    m_radioPairingTimer.setSingleShot(true);
    connect(&m_radioPairingTimer, &QTimer::timeout,
            this, &ZenithProtocolClient::onRadioPairingTimeout);

    // ModeSelection ACK timeout
    m_modeSelectionAckTimer.setSingleShot(true);
    connect(&m_modeSelectionAckTimer, &QTimer::timeout, this, [this]() {
        if (!m_awaitingModeSelectionAck) return;
        if (m_modeSelectionRetries < kModeSelectionMaxRetries) {
            ++m_modeSelectionRetries;
            appendLog("ModeSelection ACK timeout, retrying...");
            sendModeSelection(true);
            m_modeSelectionAckTimer.start(kModeSelectionAckTimeoutMs);
        } else {
            appendLog("ModeSelection ACK timeout, proceeding without ACK");
            m_awaitingModeSelectionAck = false;
            updateLinkStates();
        }
    });
}

// ---------------------------------------------------------------------------
// Property getters
// ---------------------------------------------------------------------------
QString ZenithProtocolClient::remoteHostIp() const { return m_remoteHostIp; }
quint16 ZenithProtocolClient::udpPort() const { return m_udpPort; }
quint16 ZenithProtocolClient::tcpPort() const { return m_tcpPort; }
quint16 ZenithProtocolClient::heartbeatPort() const { return m_heartbeatPort; }
int ZenithProtocolClient::robotId() const { return m_robotId; }
QString ZenithProtocolClient::udpState() const { return m_udpState; }
QString ZenithProtocolClient::tcpState() const { return m_tcpState; }
QString ZenithProtocolClient::heartbeatState() const { return m_heartbeatState; }
QString ZenithProtocolClient::connectionSummary() const { return m_connectionSummary; }
QString ZenithProtocolClient::protocolLogText() const { return m_protocolLogText; }

bool ZenithProtocolClient::isConnected() const
{
    if (m_transportMode == TransportMode::Serial) {
        // An open COM handle is not a usable aircraft link. After every open or
        // reopen, wait for a new CRC-valid frame before exposing "connected".
        return m_serialPort.isOpen() && radioPairingReady()
            && m_serialHasValidFrameSinceOpen && telemetryFresh();
    }
    return m_tcpSocket.state() == QAbstractSocket::ConnectedState;
}

bool ZenithProtocolClient::telemetryFresh() const
{
    if (m_transportMode == TransportMode::Serial) {
        // Serial mode: all data arrives on one channel, check any rx timestamp
        const qint64 lastRx = std::max({m_lastUdpRxMs, m_lastTcpRxMs, m_lastHeartbeatRxMs});
        return lastRx > 0 && (nowMs() - lastRx) <= kUdpFreshnessTimeoutMs;
    }
    return m_lastUdpRxMs > 0 && (nowMs() - m_lastUdpRxMs) <= kUdpFreshnessTimeoutMs;
}

bool ZenithProtocolClient::heartbeatFresh() const
{
    if (m_transportMode == TransportMode::Serial) {
        // Serial mode: all data arrives on one channel, check any rx timestamp
        const qint64 lastRx = std::max({m_lastUdpRxMs, m_lastTcpRxMs, m_lastHeartbeatRxMs});
        return lastRx > 0 && (nowMs() - lastRx) <= kHeartbeatFreshnessTimeoutMs;
    }
    return m_lastHeartbeatRxMs > 0 && (nowMs() - m_lastHeartbeatRxMs) <= kHeartbeatFreshnessTimeoutMs;
}

bool ZenithProtocolClient::canSendControlCommands() const
{
    if (m_transportMode == TransportMode::Serial) {
        return m_active && m_serialPort.isOpen() && radioPairingReady()
            && telemetryFresh();
    }
    return m_active
        && m_tcpSocket.state() == QAbstractSocket::ConnectedState
        && telemetryFresh()
        && heartbeatFresh();
}

// ---------------------------------------------------------------------------
// Property setters
// ---------------------------------------------------------------------------
void ZenithProtocolClient::setRemoteHostIp(const QString &ip) { m_remoteHostIp = ip; }
void ZenithProtocolClient::setUdpPort(quint16 port) { m_udpPort = port; }
void ZenithProtocolClient::setTcpPort(quint16 port) { m_tcpPort = port; }
void ZenithProtocolClient::setHeartbeatPort(quint16 port) { m_heartbeatPort = port; }
void ZenithProtocolClient::setRobotId(int robotId) { m_robotId = qMax(1, robotId); }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void ZenithProtocolClient::start()
{
    if (m_active) {
        return;
    }
    m_active = true;
    m_hasEverConnected = false;
    resetFreshness();

    // Common timers for both transport modes
    m_heartbeatCount = 0;
    m_heartbeatTimer.start(kHeartbeatIntervalMs);
    m_linkMonitorTimer.start();

    if (m_transportMode == TransportMode::Serial) {
        m_serialReconnectCount = 0;
        m_serialReconnectBackoffStep = 0;
        m_serialHadValidFrame = false;
        m_serialHasValidFrameSinceOpen = false;
        m_serialDataInterrupted = false;
        m_lastSerialValidFrameMs = 0;
        m_lastSerialDisplayFrameMs = 0;
        m_serialOpenedAtMs = 0;
        m_serialRxBytes = 0;
        m_serialTxBytes = 0;
        m_serialRateSampleRxBytes = 0;
        m_serialRateSampleTxBytes = 0;
        m_serialRxBytesPerSecond = 0;
        m_serialTxBytesPerSecond = 0;
        m_serialRateSampleMs = nowMs();
        startSerial();
    } else {
        startNetwork();
    }

    updateLinkStates();
}

void ZenithProtocolClient::startNetwork()
{
    // Bind UDP listener
    if (m_udpSocket.state() != QAbstractSocket::BoundState) {
        if (m_udpSocket.bind(QHostAddress::AnyIPv4, m_udpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            appendLog("UDP telemetry listener started");
        } else {
            appendLog(QString("UDP bind failed: %1").arg(m_udpSocket.errorString()));
        }
    }

    // Start heartbeat receiver
    if (!m_heartbeatServer.isListening()) {
        if (m_heartbeatServer.listen(QHostAddress::AnyIPv4, m_heartbeatPort)) {
            appendLog("Heartbeat listener started");
        } else {
            appendLog(QString("Heartbeat listen failed: %1").arg(m_heartbeatServer.errorString()));
        }
    }

    // Connect persistent TCP
    m_pendingModeSelection = true;
    connectTcp();
}

void ZenithProtocolClient::stop()
{
    m_active = false;

    // Stop common timers
    m_heartbeatTimer.stop();
    m_linkMonitorTimer.stop();
    m_modeSelectionAckTimer.stop();
    m_reconnectTimer.stop();
    m_awaitingModeSelectionAck = false;

    if (m_transportMode == TransportMode::Serial) {
        stopSerial();
    } else {
        stopNetwork();
    }

    resetFreshness();
    appendLog("Protocol client stopped");
    updateLinkStates();
}

void ZenithProtocolClient::stopNetwork()
{
    m_tcpIntentionalDisconnect = true;
    m_reconnectTimer.stop();

    // Close TCP
    disconnectTcp();

    // Close heartbeat peer
    if (m_heartbeatPeer) {
        m_heartbeatPeer->disconnectFromHost();
        m_heartbeatPeer->deleteLater();
        m_heartbeatPeer = nullptr;
    }

    // Close UDP & heartbeat server
    m_udpSocket.close();
    m_heartbeatServer.close();
}

bool ZenithProtocolClient::testConnection()
{
    QTcpSocket probe;
    probe.connectToHost(m_remoteHostIp, m_tcpPort);
    const bool ok = probe.waitForConnected(1200);
    if (ok) {
        appendLog("TCP control port reachable");
        probe.disconnectFromHost();
    } else {
        appendLog("TCP control port unreachable");
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Persistent TCP connection management
// ---------------------------------------------------------------------------
void ZenithProtocolClient::connectTcp()
{
    if (m_tcpSocket.state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    m_tcpIntentionalDisconnect = false;
    m_tcpRecvBuffer.clear();
    appendLog(QString("TCP connecting %1:%2").arg(m_remoteHostIp).arg(m_tcpPort));
    updateLinkStates();
    m_tcpSocket.connectToHost(m_remoteHostIp, m_tcpPort);
}

void ZenithProtocolClient::disconnectTcp()
{
    m_tcpIntentionalDisconnect = true;
    m_reconnectTimer.stop();
    if (m_tcpSocket.state() != QAbstractSocket::UnconnectedState) {
        m_tcpSocket.disconnectFromHost();
    }
}

void ZenithProtocolClient::onTcpConnected()
{
    m_hasEverConnected = true;
    noteTcpRx();
    appendLog(QString("TCP connected %1:%2").arg(m_remoteHostIp).arg(m_tcpPort));
    updateLinkStates();
    emit tcpConnectedChanged(true);

    // Send pending ModeSelection on fresh connect
    if (m_pendingModeSelection) {
        m_pendingModeSelection = false;
        m_awaitingModeSelectionAck = true;
        m_modeSelectionRetries = 0;
        sendModeSelection(true);
        m_modeSelectionAckTimer.start(kModeSelectionAckTimeoutMs);
    }
}

void ZenithProtocolClient::onTcpDisconnected()
{
    appendLog("TCP connection lost");
    updateLinkStates();
    emit tcpConnectedChanged(false);

    // Auto-reconnect if not intentionally disconnected
    if (m_active && !m_tcpIntentionalDisconnect) {
        appendLog(QString("Reconnecting in %1ms...").arg(kReconnectIntervalMs));
        m_reconnectTimer.start(kReconnectIntervalMs);
    }
}

void ZenithProtocolClient::onTcpReadyRead()
{
    noteTcpRx();
    m_tcpRecvBuffer.append(m_tcpSocket.readAll());
    processBuffer(m_tcpRecvBuffer);
}

void ZenithProtocolClient::onTcpError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    if (m_tcpSocket.state() == QAbstractSocket::UnconnectedState) {
        appendLog(QString("TCP error: %1").arg(m_tcpSocket.errorString()));
        updateLinkStates();

        if (m_active && !m_tcpIntentionalDisconnect) {
            m_reconnectTimer.start(kReconnectIntervalMs);
        }
    }
}

void ZenithProtocolClient::onReconnectTimer()
{
    if (!m_active) {
        return;
    }
    if (m_transportMode == TransportMode::Serial) {
        ++m_serialReconnectCount;
        openSerial(true);
    } else if (!m_tcpIntentionalDisconnect) {
        connectTcp();
    }
}

// ---------------------------------------------------------------------------
// Heartbeat sending
// ---------------------------------------------------------------------------
void ZenithProtocolClient::onHeartbeatTimer()
{
    if (m_transportMode == TransportMode::Serial) {
        if (!m_serialPort.isOpen() || !radioPairingReady()) {
            return;
        }
        QVariantMap payload;
        payload.insert("count", static_cast<int>(m_heartbeatCount++));
        payload.insert("message", QString());
        const QByteArray frame = packFrame(ZenithProtocol::HEARTBEAT, m_robotId, payload);
        writeSerial(frame);
        return;
    }

    if (m_tcpSocket.state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QVariantMap payload;
    payload.insert("count", static_cast<int>(m_heartbeatCount++));
    payload.insert("message", QString());

    const QByteArray frame = packFrame(ZenithProtocol::HEARTBEAT, m_robotId, payload);
    m_tcpSocket.write(frame);
}

// ---------------------------------------------------------------------------
// Send messages over persistent TCP
// ---------------------------------------------------------------------------
void ZenithProtocolClient::sendTcpMessage(int msgId, const QVariantMap &payload, int robotId)
{
    if (m_transportMode == TransportMode::Serial) {
        if (!m_serialPort.isOpen() || !radioPairingReady()) {
            appendLog("Serial send blocked until LR24 pairing is ready");
            updateLinkStates();
            return;
        }
        const QByteArray packet = packFrame(msgId, robotId, payload);
        writeSerial(packet);
        appendLog(QString("Serial send msg_id=%1 bytes=%2").arg(msgId).arg(packet.size()));
        return;
    }

    const bool isControlMessage = msgId == ZenithProtocol::UAVCOMMAND
        || msgId == ZenithProtocol::UAVSETUP
        || msgId == ZenithProtocol::CUSTOMDATASEGMENT_1;
    if (isControlMessage && !canSendControlCommands()) {
        appendLog(QString("TCP send blocked while link is not ready: msg_id=%1").arg(msgId));
        updateLinkStates();
        return;
    }

    if (m_tcpSocket.state() != QAbstractSocket::ConnectedState) {
        appendLog("TCP send skipped: not connected");
        updateLinkStates();
        return;
    }

    const QByteArray packet = packFrame(msgId, robotId, payload);
    m_tcpSocket.write(packet);
    m_tcpSocket.flush();
    appendLog(QString("TCP send msg_id=%1 bytes=%2").arg(msgId).arg(packet.size()));
}

void ZenithProtocolClient::sendUdpMessage(int msgId, const QVariantMap &payload, int robotId)
{
    if (m_transportMode == TransportMode::Serial) {
        if (!m_serialPort.isOpen() || !radioPairingReady()) {
            appendLog("Serial send blocked until LR24 pairing is ready");
            return;
        }
        const QByteArray packet = packFrame(msgId, robotId, payload);
        writeSerial(packet);
        appendLog(QString("Serial send msg_id=%1 bytes=%2").arg(msgId).arg(packet.size()));
        return;
    }

    if (m_udpSocket.state() != QAbstractSocket::BoundState) {
        appendLog("UDP send skipped: listener not bound");
        return;
    }

    const QByteArray datagram = packFrame(msgId, robotId, payload);
    m_udpSocket.writeDatagram(datagram, QHostAddress(m_remoteHostIp), m_udpPort);
    appendLog(QString("UDP send msg_id=%1 bytes=%2").arg(msgId).arg(datagram.size()));
}

void ZenithProtocolClient::noteModeSelectionAck()
{
    if (m_awaitingModeSelectionAck) {
        m_awaitingModeSelectionAck = false;
        m_modeSelectionAckTimer.stop();
        appendLog("ModeSelection ACK received");
        updateLinkStates();
    }
}

bool ZenithProtocolClient::awaitingModeSelectionAck() const
{
    return m_awaitingModeSelectionAck;
}

void ZenithProtocolClient::sendModeSelection(bool createMode)
{
    QVariantMap payload;
    payload.insert("mode", ZenithProtocol::UAVBASIC_MODE);
    payload.insert("selectId", QVariantList{m_robotId});
    payload.insert("use_mode", createMode ? ZenithProtocol::UM_CREATE : ZenithProtocol::UM_DELETE);
    payload.insert("is_simulation", false);
    payload.insert("swarm_num", 1);
    payload.insert("cmd", QString());
    sendTcpMessage(ZenithProtocol::MODESELECTION, payload, m_robotId);
}

// ---------------------------------------------------------------------------
// Frame codec
// ---------------------------------------------------------------------------
QByteArray ZenithProtocolClient::packFrame(int msgId, int robotId, const QVariantMap &payload) const
{
    const QByteArray jsonPayload = QJsonDocument(QJsonObject::fromVariantMap(payload)).toJson(QJsonDocument::Compact);
    QByteArray frame;
    frame.reserve(jsonPayload.size() + kFrameOverhead);
    frame.append(kMagic0);
    frame.append(kMagic1);

    quint32 payloadSize = static_cast<quint32>(jsonPayload.size());
    frame.append(static_cast<char>(payloadSize & 0xFF));
    frame.append(static_cast<char>((payloadSize >> 8) & 0xFF));
    frame.append(static_cast<char>((payloadSize >> 16) & 0xFF));
    frame.append(static_cast<char>((payloadSize >> 24) & 0xFF));
    frame.append(static_cast<char>(msgId & 0xFF));
    frame.append(static_cast<char>(robotId & 0xFF));
    frame.append(jsonPayload);

    const quint16 crc = crc16Arc(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

ZenithProtocolClient::DecodedFrame ZenithProtocolClient::tryDecodeFrame(const QByteArray &buffer) const
{
    DecodedFrame result;
    if (buffer.size() < kFrameOverhead) {
        return result;
    }

    int offset = 0;
    while (offset + 1 < buffer.size() && !(buffer[offset] == kMagic0 && buffer[offset + 1] == kMagic1)) {
        ++offset;
    }
    if (offset > 0 || offset + kFrameOverhead > buffer.size()) {
        result.totalBytes = offset;
        return result;
    }

    const quint32 payloadSize = static_cast<quint8>(buffer[2])
        | (static_cast<quint32>(static_cast<quint8>(buffer[3])) << 8)
        | (static_cast<quint32>(static_cast<quint8>(buffer[4])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(buffer[5])) << 24);

    // Sanity check. 上界由栅格帧决定，不是 8KB：机载 gridMapCb() 的窗口是
    // 20m/0.15m = 133×133 = 17689 格，RLE 最坏情况每格一对 (value,run) = 35378 字节，
    // 且机载侧不做任何截断。原来卡在 8192 会在障碍物一多时把整帧栅格丢掉，
    // 而丢帧后只跳 2 字节重找 magic，RLE 二进制里撞上 0x61 0x6D 还会假同步、
    // 连累后面几帧遥测 CRC 失败。
    if (payloadSize > kMaxPayloadSize) {
        result.totalBytes = 2; // skip past false magic bytes
        return result;
    }

    const int totalSize = static_cast<int>(payloadSize) + kFrameOverhead;
    if (buffer.size() < totalSize) {
        return result;
    }

    const QByteArray frame = buffer.left(totalSize);
    const quint16 expectedCrc = static_cast<quint8>(frame[totalSize - 2])
        | (static_cast<quint16>(static_cast<quint8>(frame[totalSize - 1])) << 8);
    const quint16 actualCrc = crc16Arc(frame.left(totalSize - 2));
    result.totalBytes = totalSize;
    if (expectedCrc != actualCrc) {
        return result;
    }

    const QByteArray payloadBytes = frame.mid(8, static_cast<int>(payloadSize));

    result.msgId = static_cast<quint8>(frame[6]);
    result.robotId = static_cast<quint8>(frame[7]);

    // Auto-detect: MsgPack (首字节 0x80-0x8F/0xDE/0xDF) vs JSON (首字节 '{')
    if (ZenithMsgPack::isMsgPack(payloadBytes)) {
        if (!ZenithMsgPack::decodeMsgPack(payloadBytes, result.payload)) {
            return result;
        }
    } else {
        const QJsonDocument document = QJsonDocument::fromJson(payloadBytes);
        if (!document.isObject()) {
            return result;
        }
        result.payload = document.object().toVariantMap();
    }

    result.valid = true;
    return result;
}

quint16 ZenithProtocolClient::crc16Arc(const QByteArray &data) const
{
    quint16 crc = 0x0000;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Logging & state
// ---------------------------------------------------------------------------
void ZenithProtocolClient::appendLog(const QString &line)
{
    if (!m_protocolLogText.isEmpty()) {
        m_protocolLogText.append('\n');
    }
    m_protocolLogText.append(line);
    if (m_protocolLogText.size() > 4000) {
        m_protocolLogText = m_protocolLogText.right(4000);
    }
    emit protocolLog(line);
    emit linkStatesChanged();
}

void ZenithProtocolClient::updateLinkStates()
{
    const qint64 now = nowMs();

    if (m_transportMode == TransportMode::Serial) {
        updateSerialRates(now);

        const bool validDataTimedOut = m_serialHasValidFrameSinceOpen
            && m_lastSerialValidFrameMs > 0
            && (now - m_lastSerialValidFrameMs) > kSerialStaleReconnectMs;
        const bool firstFrameAfterReconnectTimedOut = m_serialDataInterrupted
            && !m_serialHasValidFrameSinceOpen
            && m_serialOpenedAtMs > 0
            && (now - m_serialOpenedAtMs) > kSerialStaleReconnectMs;
        if (m_active && m_serialPort.isOpen() && radioPairingReady()
            && (validDataTimedOut || firstFrameAfterReconnectTimedOut)) {
            m_serialDataInterrupted = true;
            scheduleSerialReconnect(validDataTimedOut
                ? QStringLiteral("valid data timeout")
                : QStringLiteral("no valid frame after reconnect"));
        }

        // Serial mode: single channel, simplified state reporting
        const qint64 lastRx = m_lastSerialValidFrameMs;
        const bool portOpen = m_serialPort.isOpen();
        const bool fresh = lastRx > 0 && (now - lastRx) <= kUdpFreshnessTimeoutMs;

        QString serialState;
        if (!m_active) {
            serialState = QStringLiteral("IDLE");
        } else if (portOpen && m_radioPairingStage == RadioPairingStage::Failed) {
            serialState = QStringLiteral("PAIRING_ERROR");
        } else if (portOpen && !radioPairingReady()) {
            serialState = QStringLiteral("PAIRING");
        } else if (portOpen && fresh) {
            serialState = QStringLiteral("CONNECTED");
        } else if (portOpen && lastRx > 0) {
            serialState = QStringLiteral("STALE");
        } else if (portOpen) {
            serialState = QStringLiteral("WAITING");
        } else {
            serialState = QStringLiteral("DISCONNECTED");
        }

        // Mirror serial state to all three channel states for UI consistency
        m_tcpState = serialState;
        m_udpState = serialState;
        m_heartbeatState = serialState;

        QString overallState = QStringLiteral("STOPPED");
        if (m_active) {
            if (portOpen && m_radioPairingStage == RadioPairingStage::Failed) {
                overallState = QStringLiteral("PAIRING_ERROR");
            } else if (portOpen && !radioPairingReady()) {
                overallState = QStringLiteral("PAIRING");
            } else if (portOpen && fresh && !m_awaitingModeSelectionAck) {
                overallState = QStringLiteral("CONNECTED");
            } else if (portOpen && m_awaitingModeSelectionAck) {
                overallState = QStringLiteral("HANDSHAKING");
            } else if (portOpen) {
                overallState = QStringLiteral("DEGRADED");
            } else {
                overallState = QStringLiteral("DISCONNECTED");
            }
        }

        auto ageText = [now](qint64 ts) -> QString {
            if (ts <= 0) return QStringLiteral("never");
            return QString::number((now - ts) / 1000.0, 'f', 1) + QStringLiteral("s");
        };

        m_connectionSummary = QString("%1 | SERIAL=%2(last=%3) | LR24=%4 target=%5 actual=%6")
            .arg(overallState, serialState, ageText(lastRx), radioPairingState())
            .arg(m_radioTargetAddress)
            .arg(radioActualAddressText());
        emit transportStateChanged(m_connectionSummary);
        emit linkStatesChanged();
        return;
    }

    // Network mode: original 3-channel state logic
    if (!m_active) {
        m_udpState = QStringLiteral("IDLE");
    } else if (m_udpSocket.state() != QAbstractSocket::BoundState) {
        m_udpState = QStringLiteral("ERROR");
    } else if (telemetryFresh()) {
        m_udpState = QStringLiteral("CONNECTED");
    } else if (m_lastUdpRxMs > 0) {
        m_udpState = QStringLiteral("STALE");
    } else {
        m_udpState = QStringLiteral("WAITING");
    }

    if (!m_active) {
        m_heartbeatState = QStringLiteral("IDLE");
    } else if (!m_heartbeatServer.isListening()) {
        m_heartbeatState = QStringLiteral("ERROR");
    } else if (heartbeatFresh()) {
        m_heartbeatState = QStringLiteral("CONNECTED");
    } else if (m_lastHeartbeatRxMs > 0) {
        m_heartbeatState = QStringLiteral("STALE");
    } else {
        m_heartbeatState = QStringLiteral("WAITING");
    }

    if (!m_active) {
        m_tcpState = QStringLiteral("DISCONNECTED");
    } else if (m_tcpSocket.state() == QAbstractSocket::ConnectedState) {
        m_tcpState = QStringLiteral("CONNECTED");
    } else if (m_tcpSocket.state() == QAbstractSocket::ConnectingState) {
        m_tcpState = QStringLiteral("CONNECTING");
    } else if (m_reconnectTimer.isActive()) {
        m_tcpState = QStringLiteral("RECONNECTING");
    } else if (m_hasEverConnected) {
        m_tcpState = QStringLiteral("DISCONNECTED");
    } else {
        m_tcpState = QStringLiteral("CONNECTING");
    }

    QString overallState = QStringLiteral("STOPPED");
    if (m_active) {
        const bool tcpConnected = m_tcpSocket.state() == QAbstractSocket::ConnectedState;
        const bool udpOk = telemetryFresh();
        const bool heartbeatOk = heartbeatFresh();
        if (tcpConnected && udpOk && heartbeatOk && !m_awaitingModeSelectionAck) {
            overallState = QStringLiteral("CONNECTED");
        } else if (tcpConnected && m_awaitingModeSelectionAck) {
            overallState = QStringLiteral("HANDSHAKING");
        } else if (m_tcpSocket.state() == QAbstractSocket::ConnectingState || m_reconnectTimer.isActive()) {
            overallState = QStringLiteral("RECONNECTING");
        } else if (tcpConnected || m_lastUdpRxMs > 0 || m_lastHeartbeatRxMs > 0) {
            overallState = QStringLiteral("DEGRADED");
        } else {
            overallState = QStringLiteral("DISCONNECTED");
        }
    }

    auto ageText = [now](qint64 ts) -> QString {
        if (ts <= 0) {
            return QStringLiteral("never");
        }
        return QString::number((now - ts) / 1000.0, 'f', 1) + QStringLiteral("s");
    };

    m_connectionSummary = QString("%1 | TCP=%2 | UDP=%3(last=%4) | HB=%5(last=%6)")
        .arg(overallState, m_tcpState, m_udpState, ageText(m_lastUdpRxMs), m_heartbeatState, ageText(m_lastHeartbeatRxMs));
    emit transportStateChanged(m_connectionSummary);
    emit linkStatesChanged();
}

// ---------------------------------------------------------------------------
// Frame processing
// ---------------------------------------------------------------------------
int ZenithProtocolClient::processBuffer(QByteArray &buffer)
{
    int validFrames = 0;
    // Guard: 缓冲异常增长时丢弃陈旧数据（链路噪声恢复）
    if (buffer.size() > kMaxBufferSize) {
        appendLog(QString("Buffer overflow (%1 bytes), flushing").arg(buffer.size()));
        buffer.clear();
        return 0;
    }

    while (!buffer.isEmpty()) {
        const DecodedFrame decoded = tryDecodeFrame(buffer);
        if (decoded.totalBytes > 0 && !decoded.valid) {
            buffer.remove(0, decoded.totalBytes);
            continue;
        }
        if (!decoded.valid) {
            break;
        }

        if (decoded.msgId == ZenithProtocol::HEARTBEAT) {
            noteHeartbeatRx();
        }
        ++validFrames;
        emit decodedMessage(decoded.msgId, decoded.robotId, decoded.payload);
        buffer.remove(0, decoded.totalBytes);
    }
    return validFrames;
}

void ZenithProtocolClient::processFrame(const QByteArray &frame)
{
    QByteArray buffer = frame;
    processBuffer(buffer);
}

void ZenithProtocolClient::handleHeartbeatConnection(QTcpSocket *socket)
{
    if (!socket) {
        return;
    }

    if (m_heartbeatPeer && m_heartbeatPeer != socket) {
        m_heartbeatPeer->disconnectFromHost();
        m_heartbeatPeer->deleteLater();
    }
    m_heartbeatPeer = socket;
    m_heartbeatBuffer.clear();
    appendLog("Heartbeat connection accepted");
    updateLinkStates();

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (socket != m_heartbeatPeer) {
            return;
        }
        noteHeartbeatRx();
        m_heartbeatBuffer.append(socket->readAll());
        processBuffer(m_heartbeatBuffer);
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        if (socket == m_heartbeatPeer) {
            m_heartbeatPeer = nullptr;
            updateLinkStates();
        }
        socket->deleteLater();
    });
}

void ZenithProtocolClient::resetFreshness()
{
    m_lastUdpRxMs = 0;
    m_lastHeartbeatRxMs = 0;
    m_lastTcpRxMs = 0;
    m_lastSerialValidFrameMs = 0;
}

void ZenithProtocolClient::noteUdpRx()
{
    m_lastUdpRxMs = nowMs();
}

void ZenithProtocolClient::noteHeartbeatRx()
{
    m_lastHeartbeatRxMs = nowMs();
}

void ZenithProtocolClient::noteTcpRx()
{
    m_lastTcpRxMs = nowMs();
}

qint64 ZenithProtocolClient::nowMs() const
{
    return QDateTime::currentMSecsSinceEpoch();
}

// ---------------------------------------------------------------------------
// LR24 local address pairing
// ---------------------------------------------------------------------------
int ZenithProtocolClient::radioTargetAddress() const { return m_radioTargetAddress; }
int ZenithProtocolClient::radioActualAddress() const { return m_radioActualAddress; }
QString ZenithProtocolClient::radioPairingErrorCode() const { return m_radioPairingErrorCode; }
QString ZenithProtocolClient::radioPairingErrorText() const { return m_radioPairingErrorText; }

void ZenithProtocolClient::setRadioTargetAddress(int address)
{
    if (address != 0 && !Lr24RadioProtocol::isFleetAddressAssignable(address)) {
        failRadioPairing(QStringLiteral("PAIRING_INVALID_TARGET"),
                         QStringLiteral("数传目标地址必须在 1–254 之间"));
        return;
    }
    if (m_radioTargetAddress == address) {
        return;
    }
    m_radioTargetAddress = address;
    emit radioPairingChanged();
}

QString ZenithProtocolClient::radioActualAddressText() const
{
    if (m_radioActualAddress < 0) {
        return QStringLiteral("—");
    }
    if (m_radioActualAddress == Lr24RadioProtocol::UnpairedAddress) {
        return QStringLiteral("未配对（1000）");
    }
    return QString::number(m_radioActualAddress);
}

QString ZenithProtocolClient::radioPairingState() const
{
    switch (m_radioPairingStage) {
    case RadioPairingStage::Idle: return QStringLiteral("IDLE");
    case RadioPairingStage::Settling: return QStringLiteral("SETTLING");
    case RadioPairingStage::WaitingHeartbeat: return QStringLiteral("IDENTIFYING");
    case RadioPairingStage::WaitingInitialRead: return QStringLiteral("READING");
    case RadioPairingStage::WaitingSetAck: return QStringLiteral("WRITING");
    case RadioPairingStage::WaitingWriteDelay: return QStringLiteral("WRITING");
    case RadioPairingStage::WaitingVerify: return QStringLiteral("VERIFYING");
    case RadioPairingStage::Ready: return QStringLiteral("READY");
    case RadioPairingStage::Failed: return QStringLiteral("ERROR");
    }
    return QStringLiteral("ERROR");
}

QString ZenithProtocolClient::radioPairingStateText() const
{
    switch (m_radioPairingStage) {
    case RadioPairingStage::Idle: return QStringLiteral("等待串口连接");
    case RadioPairingStage::Settling: return QStringLiteral("等待数传就绪");
    case RadioPairingStage::WaitingHeartbeat: return QStringLiteral("正在识别本地数传");
    case RadioPairingStage::WaitingInitialRead: return QStringLiteral("正在读取当前数传地址");
    case RadioPairingStage::WaitingSetAck: return QStringLiteral("正在下发目标地址并等待确认");
    case RadioPairingStage::WaitingWriteDelay: return QStringLiteral("正在下发目标地址");
    case RadioPairingStage::WaitingVerify: return QStringLiteral("正在回读确认地址");
    case RadioPairingStage::Ready: return QStringLiteral("数传地址配对完成");
    case RadioPairingStage::Failed:
        return m_radioPairingErrorText.isEmpty()
            ? QStringLiteral("数传地址配对失败") : m_radioPairingErrorText;
    }
    return QStringLiteral("数传地址状态未知");
}

bool ZenithProtocolClient::radioPairingReady() const
{
    return m_radioPairingStage == RadioPairingStage::Ready;
}

bool ZenithProtocolClient::radioAddressMatchesTarget() const
{
    return m_radioActualAddress > 0
        && m_radioActualAddress == m_radioTargetAddress;
}

void ZenithProtocolClient::setRadioPairingStage(RadioPairingStage stage)
{
    if (m_radioPairingStage == stage) {
        return;
    }
    m_radioPairingStage = stage;
    emit radioPairingChanged();
    emit linkStatesChanged();
}

void ZenithProtocolClient::resetRadioPairing(bool clearActualAddress)
{
    m_radioPairingTimer.stop();
    m_radioConfigParser.reset();
    m_radioCurrentParameters.clear();
    m_radioProductModel = 0;
    m_radioSystemId = 0;
    m_radioPairingAttempts = 0;
    m_radioLastParseError.clear();
    m_radioPairingErrorCode.clear();
    m_radioPairingErrorText.clear();
    if (clearActualAddress) {
        m_radioActualAddress = -1;
    }
    setRadioPairingStage(RadioPairingStage::Idle);
    emit radioPairingChanged();
}

void ZenithProtocolClient::beginRadioPairing()
{
    m_modeSelectionAckTimer.stop();
    m_awaitingModeSelectionAck = false;
    m_serialRecvBuffer.clear();
    resetFreshness();
    m_serialHasValidFrameSinceOpen = false;
    m_serialOpenedAtMs = 0;

    if (!Lr24RadioProtocol::isFleetAddressAssignable(m_radioTargetAddress)) {
        failRadioPairing(QStringLiteral("PAIRING_NO_TARGET"),
                         QStringLiteral("未选择有效的数传配对项（地址需为 1–254）"));
        return;
    }

    m_radioConfigParser.reset();
    m_radioCurrentParameters.clear();
    m_radioActualAddress = -1;
    m_radioProductModel = 0;
    m_radioSystemId = 0;
    m_radioPairingAttempts = 0;
    m_radioLastParseError.clear();
    m_radioPairingErrorCode.clear();
    m_radioPairingErrorText.clear();
    setRadioPairingStage(RadioPairingStage::Settling);
    appendLog(QStringLiteral("LR24 pairing start: target=%1").arg(m_radioTargetAddress));
    m_radioPairingTimer.start(kRadioOpenSettleMs);
    emit radioPairingChanged();
}

bool ZenithProtocolClient::reapplyRadioPairing()
{
    if (!m_active || m_transportMode != TransportMode::Serial || !m_serialPort.isOpen()) {
        failRadioPairing(QStringLiteral("PAIRING_PORT_NOT_OPEN"),
                         QStringLiteral("串口未打开，无法重新下发数传地址"));
        return false;
    }
    beginRadioPairing();
    return m_radioPairingStage != RadioPairingStage::Failed;
}

void ZenithProtocolClient::sendRadioConfigHeartbeat()
{
    QString error;
    const QByteArray bytes = Lr24RadioProtocol::makeHostHeartbeat(
        m_radioConfigSequence++, static_cast<quint32>(nowMs() & 0xFFFFFFFF), 0, &error);
    if (bytes.isEmpty()) {
        failRadioPairing(QStringLiteral("PAIRING_HEARTBEAT_BUILD_FAILED"), error);
        return;
    }
    if (writeSerial(bytes) != bytes.size()) {
        failRadioPairing(QStringLiteral("PAIRING_WRITE_FAILED"),
                         QStringLiteral("数传配置心跳写入不完整"));
    }
}

void ZenithProtocolClient::sendRadioGetAddress()
{
    QString error;
    const QByteArray bytes = Lr24RadioProtocol::makeGetAddressCommand(
        m_radioProductModel, m_radioSystemId, m_radioConfigSequence++, &error);
    if (bytes.isEmpty()) {
        failRadioPairing(QStringLiteral("PAIRING_GET_BUILD_FAILED"), error);
        return;
    }
    if (writeSerial(bytes) != bytes.size()) {
        failRadioPairing(QStringLiteral("PAIRING_WRITE_FAILED"),
                         QStringLiteral("数传地址查询写入不完整"));
    }
}

void ZenithProtocolClient::sendRadioSetAddress()
{
    QString error;
    const QByteArray bytes = Lr24RadioProtocol::makeSetAddressCommand(
        m_radioProductModel, m_radioSystemId, m_radioConfigSequence++,
        m_radioCurrentParameters, static_cast<quint16>(m_radioTargetAddress), &error);
    if (bytes.isEmpty()) {
        failRadioPairing(QStringLiteral("PAIRING_SET_BUILD_FAILED"), error);
        return;
    }
    if (writeSerial(bytes) != bytes.size()) {
        failRadioPairing(QStringLiteral("PAIRING_WRITE_FAILED"),
                         QStringLiteral("数传地址设置写入不完整"));
    }
}

void ZenithProtocolClient::onRadioPairingTimeout()
{
    const auto failForParseError = [this]() {
        if (m_radioLastParseError.isEmpty()) {
            return false;
        }
        failRadioPairing(QStringLiteral("PAIRING_FRAME_INVALID"),
                         QStringLiteral("数传配置帧解析失败：%1").arg(m_radioLastParseError));
        return true;
    };

    switch (m_radioPairingStage) {
    case RadioPairingStage::Settling:
        m_radioPairingAttempts = 0;
        setRadioPairingStage(RadioPairingStage::WaitingHeartbeat);
        sendRadioConfigHeartbeat();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        break;
    case RadioPairingStage::WaitingHeartbeat:
        if (m_radioPairingAttempts >= kRadioHeartbeatAttempts) {
            if (failForParseError()) break;
            failRadioPairing(QStringLiteral("PAIRING_HEARTBEAT_TIMEOUT"),
                             QStringLiteral("未收到本地 LR24 心跳，请检查波特率和设备连接"));
            break;
        }
        sendRadioConfigHeartbeat();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        break;
    case RadioPairingStage::WaitingInitialRead:
        if (m_radioPairingAttempts >= kRadioQueryAttempts) {
            if (failForParseError()) break;
            failRadioPairing(QStringLiteral("PAIRING_READ_TIMEOUT"),
                             QStringLiteral("读取当前数传地址超时"));
            break;
        }
        sendRadioConfigHeartbeat();
        sendRadioGetAddress();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        break;
    case RadioPairingStage::WaitingSetAck:
        if (m_radioPairingAttempts >= kRadioQueryAttempts) {
            if (failForParseError()) break;
            failRadioPairing(QStringLiteral("PAIRING_SET_ACK_TIMEOUT"),
                             QStringLiteral("下发数传地址后未收到匹配的写入确认"));
            break;
        }
        sendRadioSetAddress();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        break;
    case RadioPairingStage::WaitingWriteDelay:
        m_radioPairingAttempts = 0;
        setRadioPairingStage(RadioPairingStage::WaitingVerify);
        sendRadioGetAddress();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        break;
    case RadioPairingStage::WaitingVerify:
        if (m_radioPairingAttempts >= kRadioQueryAttempts) {
            if (failForParseError()) break;
            failRadioPairing(QStringLiteral("PAIRING_VERIFY_TIMEOUT"),
                             QStringLiteral("数传地址写入后回读确认超时"));
            break;
        }
        sendRadioConfigHeartbeat();
        sendRadioGetAddress();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        break;
    default:
        break;
    }
}

void ZenithProtocolClient::handleRadioConfigFrame(const Lr24RadioProtocol::Frame &frame)
{
    if (frame.deviceId == Lr24RadioProtocol::RadioDeviceId
        && frame.messageId == Lr24RadioProtocol::HeartbeatMessageId
        && (m_radioPairingStage == RadioPairingStage::WaitingHeartbeat
            || m_radioPairingStage == RadioPairingStage::Settling)) {
        Lr24RadioProtocol::RadioHeartbeat heartbeat;
        QString error;
        if (!Lr24RadioProtocol::decodeRadioHeartbeat(frame, &heartbeat, &error)) {
            failRadioPairing(QStringLiteral("PAIRING_HEARTBEAT_INVALID"), error);
            return;
        }
        if (heartbeat.productModel != 2 && heartbeat.productModel != 32
            && heartbeat.productModel != 33 && heartbeat.productModel != 34) {
            failRadioPairing(QStringLiteral("PAIRING_UNSUPPORTED_MODEL"),
                             QStringLiteral("检测到不支持的数传型号 %1").arg(heartbeat.productModel));
            return;
        }
        m_radioProductModel = heartbeat.productModel;
        m_radioSystemId = heartbeat.systemId;
        m_radioPairingAttempts = 0;
        setRadioPairingStage(RadioPairingStage::WaitingInitialRead);
        sendRadioGetAddress();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        return;
    }

    if (frame.deviceId != Lr24RadioProtocol::RadioDeviceId
        || frame.messageId != Lr24RadioProtocol::CommandAckMessageId) {
        return;
    }

    if (m_radioPairingStage == RadioPairingStage::WaitingInitialRead) {
        quint16 address = 0;
        QByteArray parameters;
        QString error;
        if (!Lr24RadioProtocol::decodeAddressResponse(
                frame, m_radioProductModel, &address, &parameters, &error)) {
            failRadioPairing(QStringLiteral("PAIRING_READ_ACK_INVALID"), error);
            return;
        }
        m_radioPairingTimer.stop();
        m_radioActualAddress = address;
        m_radioCurrentParameters = parameters;
        emit radioPairingChanged();

        m_radioPairingAttempts = 0;
        setRadioPairingStage(RadioPairingStage::WaitingSetAck);
        sendRadioSetAddress();
        ++m_radioPairingAttempts;
        if (m_radioPairingStage != RadioPairingStage::Failed)
            m_radioPairingTimer.start(kRadioRetryIntervalMs);
        return;
    }

    if (m_radioPairingStage == RadioPairingStage::WaitingSetAck) {
        Lr24RadioProtocol::CommandAck ack;
        QString error;
        if (!Lr24RadioProtocol::decodeCommandAck(frame, &ack, &error)) {
            failRadioPairing(QStringLiteral("PAIRING_SET_ACK_INVALID"), error);
            return;
        }
        const quint16 expected = Lr24RadioProtocol::setParametersCommand(m_radioProductModel);
        if (ack.commandId != expected) {
            // A delayed GET reply can legally cross the SET write after a GET
            // retry. Ignore only that known stale reply; diagnose all others.
            if (ack.commandId == Lr24RadioProtocol::getParametersCommand(m_radioProductModel)) {
                return;
            }
            failRadioPairing(QStringLiteral("PAIRING_SET_ACK_MISMATCH"),
                             QStringLiteral("数传写入确认命令号不匹配：收到 %1，期望 %2")
                                 .arg(ack.commandId).arg(expected));
            return;
        }
        m_radioPairingTimer.stop();
        m_radioPairingAttempts = 0;
        setRadioPairingStage(RadioPairingStage::WaitingWriteDelay);
        m_radioPairingTimer.start(kRadioWriteSettleMs);
        return;
    }

    if (m_radioPairingStage == RadioPairingStage::WaitingVerify) {
        quint16 address = 0;
        QString error;
        if (!Lr24RadioProtocol::decodeAddressResponse(
                frame, m_radioProductModel, &address, nullptr, &error)) {
            Lr24RadioProtocol::CommandAck ack;
            QString ackError;
            if (Lr24RadioProtocol::decodeCommandAck(frame, &ack, &ackError)
                && ack.commandId == Lr24RadioProtocol::setParametersCommand(m_radioProductModel)) {
                // A duplicate SET confirmation can arrive after the settling
                // delay. It is harmless; keep waiting for the GET response.
                return;
            }
            failRadioPairing(QStringLiteral("PAIRING_VERIFY_ACK_INVALID"), error);
            return;
        }
        m_radioActualAddress = address;
        emit radioPairingChanged();
        if (address == m_radioTargetAddress) {
            completeRadioPairing();
        } else {
            failRadioPairing(QStringLiteral("PAIRING_VERIFY_MISMATCH"),
                             QStringLiteral("数传地址回读不一致：目标 %1，实际 %2")
                                 .arg(m_radioTargetAddress).arg(address));
        }
    }
}

void ZenithProtocolClient::completeRadioPairing()
{
    m_radioPairingTimer.stop();
    m_radioConfigParser.reset();
    m_serialRecvBuffer.clear();
    m_radioPairingErrorCode.clear();
    m_radioPairingErrorText.clear();
    m_serialOpenedAtMs = nowMs();
    m_serialHasValidFrameSinceOpen = false;
    m_serialDataInterrupted = false;
    setRadioPairingStage(RadioPairingStage::Ready);
    appendLog(QStringLiteral("LR24 pairing verified: address=%1 model=%2")
                  .arg(m_radioActualAddress).arg(m_radioProductModel));

    m_pendingModeSelection = false;
    m_awaitingModeSelectionAck = true;
    m_modeSelectionRetries = 0;
    sendModeSelection(true);
    m_modeSelectionAckTimer.start(kModeSelectionAckTimeoutMs);
    updateLinkStates();
}

void ZenithProtocolClient::failRadioPairing(const QString &code, const QString &message)
{
    m_radioPairingTimer.stop();
    m_radioPairingErrorCode = code;
    m_radioPairingErrorText = message.isEmpty()
        ? QStringLiteral("数传地址配对失败") : message;
    m_serialOpenedAtMs = 0;
    m_serialHasValidFrameSinceOpen = false;
    m_serialRecvBuffer.clear();
    setRadioPairingStage(RadioPairingStage::Failed);
    appendLog(QStringLiteral("LR24 pairing failed [%1]: %2")
                  .arg(m_radioPairingErrorCode, m_radioPairingErrorText));
    updateLinkStates();
}

// ---------------------------------------------------------------------------
// Serial transport
// ---------------------------------------------------------------------------
void ZenithProtocolClient::startSerial()
{
    openSerial(false);
}

void ZenithProtocolClient::stopSerial()
{
    m_reconnectTimer.stop();
    resetRadioPairing(true);
    m_serialClosing = true;
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }
    m_serialClosing = false;
    m_serialRecvBuffer.clear();
    m_serialActualPortName.clear();
    m_serialHasValidFrameSinceOpen = false;
    m_serialDataInterrupted = false;
    m_serialOpenedAtMs = 0;
}

void ZenithProtocolClient::openSerial(bool reconnectAttempt)
{
    if (!m_active || m_transportMode != TransportMode::Serial || m_serialOpening) {
        return;
    }

    bool ambiguous = false;
    const QString portName = resolveSerialPort(&ambiguous);
    if (portName.isEmpty()) {
        scheduleSerialReconnect(ambiguous
            ? QStringLiteral("multiple matching serial devices; select a port manually")
            : QStringLiteral("preferred serial device not found"));
        return;
    }

    m_serialOpening = true;
    m_serialRecvBuffer.clear();
    resetFreshness();
    m_serialHasValidFrameSinceOpen = false;
    m_serialActualPortName = portName;
    m_serialOpenedAtMs = 0;
    resetRadioPairing(true);

    m_serialPort.setPortName(portName);
    m_serialPort.setBaudRate(m_serialBaudRate);
    m_serialPort.setDataBits(QSerialPort::Data8);
    m_serialPort.setParity(QSerialPort::NoParity);
    m_serialPort.setStopBits(QSerialPort::OneStop);
    m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort.open(QIODevice::ReadWrite)) {
        const QSerialPortInfo info(portName);
        if (!info.isNull()) {
            captureSerialIdentity(info);
            rememberSerialIdentity(m_serialPortName.isEmpty() ? portName : m_serialPortName, info);
            rememberSerialIdentity(portName, info);
        }
        m_serialOpening = false;
        // Normal Zenith traffic starts only after the local LR24 address has
        // been read, written and verified. Pairing owns the COM port until then.
        m_serialOpenedAtMs = 0;
        appendLog(QString("Serial port opened: %1 @ %2%3")
            .arg(portName)
            .arg(m_serialBaudRate)
            .arg(reconnectAttempt ? QStringLiteral(" (reconnected, waiting for fresh data)") : QString()));

        beginRadioPairing();
        emit serialPortNameChanged();
        emit linkStatesChanged();
        return;
    }

    const QString error = m_serialPort.errorString();
    m_serialOpening = false;
    scheduleSerialReconnect(QString("open %1 failed: %2").arg(portName, error));
}

void ZenithProtocolClient::scheduleSerialReconnect(const QString &reason)
{
    if (!m_active || m_transportMode != TransportMode::Serial) {
        return;
    }

    m_modeSelectionAckTimer.stop();
    m_awaitingModeSelectionAck = false;
    resetRadioPairing(true);
    m_serialClosing = true;
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }
    m_serialClosing = false;
    m_serialRecvBuffer.clear();
    resetFreshness();
    m_serialHasValidFrameSinceOpen = false;
    m_serialOpenedAtMs = 0;

    if (!m_reconnectTimer.isActive()) {
        const int delayMs = qMin(1000 * (m_serialReconnectBackoffStep + 1), 3000);
        m_serialReconnectBackoffStep = qMin(m_serialReconnectBackoffStep + 1, 2);
        appendLog(QString("Serial reconnect in %1 ms: %2").arg(delayMs).arg(reason));
        m_reconnectTimer.start(delayMs);
    }
    emit linkStatesChanged();
}

QString ZenithProtocolClient::resolveSerialPort(bool *ambiguous) const
{
    if (ambiguous) {
        *ambiguous = false;
    }
    const auto infos = QSerialPortInfo::availablePorts();
    QList<QSerialPortInfo> matches;

    const bool hasIdentity = m_hasSerialVendorId || m_hasSerialProductId || !m_serialNumber.isEmpty();
    if (hasIdentity) {
        for (const QSerialPortInfo &info : infos) {
            if (m_hasSerialVendorId && (!info.hasVendorIdentifier() || info.vendorIdentifier() != m_serialVendorId)) continue;
            if (m_hasSerialProductId && (!info.hasProductIdentifier() || info.productIdentifier() != m_serialProductId)) continue;
            if (!m_serialNumber.isEmpty() && info.serialNumber() != m_serialNumber) continue;
            matches.append(info);
        }
        if (matches.size() == 1) {
            return matches.first().portName();
        }
        if (matches.size() > 1) {
            for (const QSerialPortInfo &info : matches) {
                if (info.portName() == m_serialPortName) {
                    return info.portName();
                }
            }
            if (ambiguous) *ambiguous = true;
        }
        return QString();
    }

    for (const QSerialPortInfo &info : infos) {
        if (info.portName() == m_serialPortName) {
            return info.portName();
        }
    }

    // First-run convenience: auto-select only when no named profile port is configured.
    // A missing profile must not inherit an unrelated attached CP210x.
    if (!m_serialPortName.isEmpty()) {
        return QString();
    }
    for (const QSerialPortInfo &info : infos) {
        if (info.hasVendorIdentifier() && info.hasProductIdentifier()
            && info.vendorIdentifier() == 0x10C4 && info.productIdentifier() == 0xEA60) {
            matches.append(info);
        }
    }
    if (matches.size() == 1) {
        return matches.first().portName();
    }
    if (matches.size() > 1 && ambiguous) {
        *ambiguous = true;
    }
    return QString();
}

void ZenithProtocolClient::captureSerialIdentity(const QSerialPortInfo &info)
{
    m_serialDeviceDescription = info.description();
    m_serialNumber = info.serialNumber();
    m_hasSerialVendorId = info.hasVendorIdentifier();
    m_hasSerialProductId = info.hasProductIdentifier();
    if (m_hasSerialVendorId) m_serialVendorId = info.vendorIdentifier();
    if (m_hasSerialProductId) m_serialProductId = info.productIdentifier();
}

void ZenithProtocolClient::clearSerialIdentity()
{
    m_serialDeviceDescription.clear();
    m_serialNumber.clear();
    m_serialVendorId = 0;
    m_serialProductId = 0;
    m_hasSerialVendorId = false;
    m_hasSerialProductId = false;
}

void ZenithProtocolClient::rememberSerialIdentity(const QString &portName, const QSerialPortInfo &info)
{
    if (portName.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.beginGroup(QStringLiteral("serialDevices/%1").arg(portName));
    settings.setValue(QStringLiteral("description"), info.description());
    settings.setValue(QStringLiteral("serialNumber"), info.serialNumber());
    settings.setValue(QStringLiteral("hasVendorId"), info.hasVendorIdentifier());
    settings.setValue(QStringLiteral("hasProductId"), info.hasProductIdentifier());
    if (info.hasVendorIdentifier()) settings.setValue(QStringLiteral("vendorId"), info.vendorIdentifier());
    else settings.remove(QStringLiteral("vendorId"));
    if (info.hasProductIdentifier()) settings.setValue(QStringLiteral("productId"), info.productIdentifier());
    else settings.remove(QStringLiteral("productId"));
    settings.endGroup();
}

bool ZenithProtocolClient::restoreSerialIdentity(const QString &portName)
{
    if (portName.isEmpty()) {
        clearSerialIdentity();
        return false;
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("serialDevices/%1").arg(portName));
    const bool hasVendor = settings.value(QStringLiteral("hasVendorId"), false).toBool();
    const bool hasProduct = settings.value(QStringLiteral("hasProductId"), false).toBool();
    const QString serial = settings.value(QStringLiteral("serialNumber")).toString();
    if (!hasVendor && !hasProduct && serial.isEmpty()) {
        settings.endGroup();
        clearSerialIdentity();
        return false;
    }

    m_serialDeviceDescription = settings.value(QStringLiteral("description")).toString();
    m_serialNumber = serial;
    m_hasSerialVendorId = hasVendor;
    m_hasSerialProductId = hasProduct;
    m_serialVendorId = static_cast<quint16>(settings.value(QStringLiteral("vendorId"), 0).toUInt());
    m_serialProductId = static_cast<quint16>(settings.value(QStringLiteral("productId"), 0).toUInt());
    settings.endGroup();
    return true;
}

QSerialPortInfo ZenithProtocolClient::currentSerialPortInfo() const
{
    const QString actual = !m_serialActualPortName.isEmpty() ? m_serialActualPortName : resolveSerialPort();
    return actual.isEmpty() ? QSerialPortInfo() : QSerialPortInfo(actual);
}

qint64 ZenithProtocolClient::writeSerial(const QByteArray &data)
{
    const qint64 written = m_serialPort.write(data);
    if (written > 0) {
        m_serialTxBytes += static_cast<quint64>(written);
    }
    return written;
}

void ZenithProtocolClient::updateSerialRates(qint64 now)
{
    if (m_serialRateSampleMs <= 0) {
        m_serialRateSampleMs = now;
        return;
    }
    const qint64 elapsed = now - m_serialRateSampleMs;
    if (elapsed < 500) {
        return;
    }
    m_serialRxBytesPerSecond = static_cast<qint64>((m_serialRxBytes - m_serialRateSampleRxBytes) * 1000 / elapsed);
    m_serialTxBytesPerSecond = static_cast<qint64>((m_serialTxBytes - m_serialRateSampleTxBytes) * 1000 / elapsed);
    m_serialRateSampleRxBytes = m_serialRxBytes;
    m_serialRateSampleTxBytes = m_serialTxBytes;
    m_serialRateSampleMs = now;
}

void ZenithProtocolClient::onSerialReadyRead()
{
    const QByteArray bytes = m_serialPort.readAll();
    m_serialRxBytes += static_cast<quint64>(bytes.size());

    int normalDataOffset = 0;
    if (!radioPairingReady()) {
        // Feed incrementally so that if the final GET_ACK and the first Zenith
        // telemetry frame share one readAll() chunk, bytes after the ACK remain
        // available to the normal decoder instead of being discarded as noise.
        while (normalDataOffset < bytes.size() && !radioPairingReady()) {
            const auto events = m_radioConfigParser.append(bytes.mid(normalDataOffset, 1));
            ++normalDataOffset;
            for (const Lr24RadioProtocol::ParseEvent &event : events) {
                if (event.type == Lr24RadioProtocol::ParseEvent::Type::FrameDecoded) {
                    handleRadioConfigFrame(event.frame);
                } else if (event.error.contains(QStringLiteral("校验和错误"))
                           || event.error.contains(QStringLiteral("负载长度"))) {
                    m_radioLastParseError = event.error;
                    appendLog(QStringLiteral("LR24 config parse error: %1").arg(event.error));
                }
            }
        }
        if (!radioPairingReady()) {
            return;
        }
    }

    m_serialRecvBuffer.append(normalDataOffset > 0 ? bytes.mid(normalDataOffset) : bytes);
    if (m_serialRecvBuffer.isEmpty()) {
        return;
    }

    // Only CRC-valid decoded frames make a newly opened/reopened link ready.
    if (processBuffer(m_serialRecvBuffer) > 0) {
        const qint64 now = nowMs();
        m_lastTcpRxMs = now;
        m_lastUdpRxMs = now;
        m_lastHeartbeatRxMs = now;
        m_lastSerialValidFrameMs = now;
        m_lastSerialDisplayFrameMs = now;
        m_serialHadValidFrame = true;
        m_serialHasValidFrameSinceOpen = true;
        m_serialDataInterrupted = false;
        m_serialReconnectBackoffStep = 0;
        emit linkStatesChanged();
    }
}

void ZenithProtocolClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    if (m_serialClosing || m_serialOpening || !m_active) {
        return;
    }
    appendLog(QString("Serial error: %1").arg(m_serialPort.errorString()));
    if (error != QSerialPort::TimeoutError) {
        m_serialDataInterrupted = m_serialHadValidFrame;
        scheduleSerialReconnect(m_serialPort.errorString());
    }
}

// ---------------------------------------------------------------------------
// Transport mode property accessors
// ---------------------------------------------------------------------------
int ZenithProtocolClient::transportMode() const
{
    return static_cast<int>(m_transportMode);
}

bool ZenithProtocolClient::active() const
{
    return m_active;
}

void ZenithProtocolClient::setTransportMode(int mode)
{
    const auto newMode = static_cast<TransportMode>(mode);
    if (m_transportMode != newMode) {
        m_transportMode = newMode;
        emit transportModeChanged();
    }
}

QString ZenithProtocolClient::serialPortName() const
{
    return m_serialPortName;
}

void ZenithProtocolClient::setSerialPortName(const QString &name)
{
    const bool changed = m_serialPortName != name;
    if (changed) {
        m_serialPortName = name;
    }

    const QSerialPortInfo info(name);
    if (!info.isNull()) {
        captureSerialIdentity(info);
        rememberSerialIdentity(name, info);
    } else if (changed) {
        // Never carry device A's identity into a newly selected but currently
        // absent profile B. Restore B's identity, or clear it completely.
        restoreSerialIdentity(name);
    }

    if (changed) {
        emit serialPortNameChanged();
        emit linkStatesChanged();
    }
}

int ZenithProtocolClient::serialBaudRate() const
{
    return m_serialBaudRate;
}

void ZenithProtocolClient::setSerialBaudRate(int baud)
{
    if (m_serialBaudRate != baud) {
        m_serialBaudRate = baud;
        emit serialBaudRateChanged();
    }
}

QStringList ZenithProtocolClient::availableSerialPorts() const
{
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports.append(info.portName());
    }
    return ports;
}

QString ZenithProtocolClient::serialConnectionState() const
{
    bool ambiguous = false;
    const bool detected = !resolveSerialPort(&ambiguous).isEmpty();
    if (!m_active) return detected ? QStringLiteral("DETECTED") : QStringLiteral("NOT_DETECTED");
    if (m_serialPort.isOpen() && m_radioPairingStage == RadioPairingStage::Failed)
        return QStringLiteral("PAIRING_FAILED");
    if (m_serialPort.isOpen() && !radioPairingReady())
        return QStringLiteral("PAIRING");
    if (m_serialPort.isOpen() && m_serialHasValidFrameSinceOpen && telemetryFresh()) return QStringLiteral("COMMUNICATING");
    if (m_serialPort.isOpen()) return QStringLiteral("WAITING_DATA");
    if (m_serialHadValidFrame || m_serialDataInterrupted) return QStringLiteral("RECONNECTING");
    if (m_serialOpening || detected) return QStringLiteral("CONNECTING");
    return QStringLiteral("NOT_DETECTED");
}

QString ZenithProtocolClient::serialConnectionStateText() const
{
    const QString state = serialConnectionState();
    if (state == QLatin1String("DETECTED")) return QStringLiteral("已检测、未连接");
    if (state == QLatin1String("CONNECTING")) return QStringLiteral("正在连接");
    if (state == QLatin1String("PAIRING")) return radioPairingStateText();
    if (state == QLatin1String("PAIRING_FAILED")) return radioPairingStateText();
    if (state == QLatin1String("WAITING_DATA")) return QStringLiteral("串口已打开、等待飞机数据");
    if (state == QLatin1String("COMMUNICATING")) return QStringLiteral("通信正常");
    if (state == QLatin1String("RECONNECTING")) return QStringLiteral("数据中断、自动重连中");
    return QStringLiteral("未检测到设备");
}

QString ZenithProtocolClient::serialDeviceName() const
{
    const QSerialPortInfo info = currentSerialPortInfo();
    const QString description = !info.isNull() ? info.description() : m_serialDeviceDescription;
    return description.isEmpty() ? QStringLiteral("CP210x 串口数传") : description;
}

QString ZenithProtocolClient::serialDeviceIdentity() const
{
    QStringList parts;
    if (!m_serialNumber.isEmpty()) parts << QStringLiteral("S/N %1").arg(m_serialNumber);
    if (m_hasSerialVendorId) parts << QStringLiteral("VID %1").arg(m_serialVendorId, 4, 16, QLatin1Char('0')).toUpper();
    if (m_hasSerialProductId) parts << QStringLiteral("PID %1").arg(m_serialProductId, 4, 16, QLatin1Char('0')).toUpper();
    return parts.isEmpty() ? QStringLiteral("身份待识别") : parts.join(QStringLiteral("  ·  "));
}

QString ZenithProtocolClient::serialActualPortName() const
{
    return m_serialActualPortName.isEmpty() ? m_serialPortName : m_serialActualPortName;
}

QString ZenithProtocolClient::serialLastDataAgeText() const
{
    if (m_lastSerialDisplayFrameMs <= 0) return QStringLiteral("—");
    return QString::number((nowMs() - m_lastSerialDisplayFrameMs) / 1000.0, 'f', 1) + QStringLiteral(" 秒");
}

qint64 ZenithProtocolClient::serialRxBytesPerSecond() const { return m_serialRxBytesPerSecond; }
qint64 ZenithProtocolClient::serialTxBytesPerSecond() const { return m_serialTxBytesPerSecond; }
qulonglong ZenithProtocolClient::serialRxBytes() const { return m_serialRxBytes; }
qulonglong ZenithProtocolClient::serialTxBytes() const { return m_serialTxBytes; }
int ZenithProtocolClient::serialReconnectCount() const { return m_serialReconnectCount; }

void ZenithProtocolClient::refreshSerialPorts()
{
    emit availableSerialPortsChanged();
    emit linkStatesChanged();
}
