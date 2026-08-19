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

#include <algorithm>

namespace {
constexpr char kMagic0 = 0x61;
constexpr char kMagic1 = 0x6D;
constexpr int kFrameOverhead = 10;
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
        return m_serialPort.isOpen();
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
        return m_active && m_serialPort.isOpen() && telemetryFresh();
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
    m_active = true;
    m_hasEverConnected = false;
    resetFreshness();

    // Common timers for both transport modes
    m_heartbeatCount = 0;
    m_heartbeatTimer.start(kHeartbeatIntervalMs);
    m_linkMonitorTimer.start();

    if (m_transportMode == TransportMode::Serial) {
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
    if (m_active && !m_tcpIntentionalDisconnect) {
        connectTcp();
    }
}

// ---------------------------------------------------------------------------
// Heartbeat sending
// ---------------------------------------------------------------------------
void ZenithProtocolClient::onHeartbeatTimer()
{
    if (m_transportMode == TransportMode::Serial) {
        if (!m_serialPort.isOpen()) {
            return;
        }
        QVariantMap payload;
        payload.insert("count", static_cast<int>(m_heartbeatCount++));
        payload.insert("message", QString());
        const QByteArray frame = packFrame(ZenithProtocol::HEARTBEAT, m_robotId, payload);
        m_serialPort.write(frame);
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
        if (!m_serialPort.isOpen()) {
            appendLog("Serial send skipped: port not open");
            updateLinkStates();
            return;
        }
        const QByteArray packet = packFrame(msgId, robotId, payload);
        m_serialPort.write(packet);
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
        if (!m_serialPort.isOpen()) {
            appendLog("Serial send skipped: port not open");
            return;
        }
        const QByteArray packet = packFrame(msgId, robotId, payload);
        m_serialPort.write(packet);
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

    // Sanity check: Zenith frames should never exceed 8KB (grid map frames can be ~4KB).
    if (payloadSize > 8192) {
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
        // Serial mode: single channel, simplified state reporting
        const qint64 lastRx = std::max({m_lastUdpRxMs, m_lastTcpRxMs, m_lastHeartbeatRxMs});
        const bool portOpen = m_serialPort.isOpen();
        const bool fresh = lastRx > 0 && (now - lastRx) <= kUdpFreshnessTimeoutMs;

        QString serialState;
        if (!m_active) {
            serialState = QStringLiteral("IDLE");
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
            if (portOpen && fresh && !m_awaitingModeSelectionAck) {
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

        m_connectionSummary = QString("%1 | SERIAL=%2(last=%3)")
            .arg(overallState, serialState, ageText(lastRx));
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
void ZenithProtocolClient::processBuffer(QByteArray &buffer)
{
    // Guard: if buffer grows beyond 16KB, discard stale data (radio corruption recovery)
    if (buffer.size() > 16384) {
        appendLog(QString("Buffer overflow (%1 bytes), flushing").arg(buffer.size()));
        buffer.clear();
        return;
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
        emit decodedMessage(decoded.msgId, decoded.robotId, decoded.payload);
        buffer.remove(0, decoded.totalBytes);
    }
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
// Serial transport
// ---------------------------------------------------------------------------
void ZenithProtocolClient::startSerial()
{
    if (m_serialPortName.isEmpty()) {
        appendLog("Serial start failed: no port name configured");
        return;
    }

    m_serialPort.setPortName(m_serialPortName);
    m_serialPort.setBaudRate(m_serialBaudRate);
    m_serialPort.setDataBits(QSerialPort::Data8);
    m_serialPort.setParity(QSerialPort::NoParity);
    m_serialPort.setStopBits(QSerialPort::OneStop);
    m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    connect(&m_serialPort, &QSerialPort::readyRead, this, &ZenithProtocolClient::onSerialReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred, this, &ZenithProtocolClient::onSerialError);

    if (m_serialPort.open(QIODevice::ReadWrite)) {
        m_serialRecvBuffer.clear();
        appendLog(QString("Serial port opened: %1 @ %2").arg(m_serialPortName).arg(m_serialBaudRate));

        m_pendingModeSelection = true;
        m_awaitingModeSelectionAck = true;
        m_modeSelectionRetries = 0;
        sendModeSelection(true);
        m_modeSelectionAckTimer.start(kModeSelectionAckTimeoutMs);
    } else {
        appendLog(QString("Serial open failed: %1").arg(m_serialPort.errorString()));
    }
}

void ZenithProtocolClient::stopSerial()
{
    disconnect(&m_serialPort, &QSerialPort::readyRead, this, &ZenithProtocolClient::onSerialReadyRead);
    disconnect(&m_serialPort, &QSerialPort::errorOccurred, this, &ZenithProtocolClient::onSerialError);

    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }
    m_serialRecvBuffer.clear();
}

void ZenithProtocolClient::onSerialReadyRead()
{
    m_serialRecvBuffer.append(m_serialPort.readAll());

    // Update ALL freshness timestamps since serial is a single multiplexed channel
    const qint64 now = nowMs();
    m_lastTcpRxMs = now;
    m_lastUdpRxMs = now;
    m_lastHeartbeatRxMs = now;

    processBuffer(m_serialRecvBuffer);
}

void ZenithProtocolClient::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    appendLog(QString("Serial error: %1").arg(m_serialPort.errorString()));
    updateLinkStates();
}

// ---------------------------------------------------------------------------
// Transport mode property accessors
// ---------------------------------------------------------------------------
int ZenithProtocolClient::transportMode() const
{
    return static_cast<int>(m_transportMode);
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
    if (m_serialPortName != name) {
        m_serialPortName = name;
        emit serialPortNameChanged();
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

void ZenithProtocolClient::refreshSerialPorts()
{
    emit availableSerialPortsChanged();
}
