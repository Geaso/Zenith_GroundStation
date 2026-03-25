#include "ZenithProtocolClient.h"

#include "ZenithProtocol.h"

#include <QAbstractSocket>
#include <QDataStream>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
constexpr char kMagic0 = 0x61;
constexpr char kMagic1 = 0x6D;
constexpr int kFrameOverhead = 10;
}

ZenithProtocolClient::ZenithProtocolClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_udpSocket, &QUdpSocket::readyRead, this, [this]() {
        while (m_udpSocket.hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(static_cast<int>(m_udpSocket.pendingDatagramSize()));
            m_udpSocket.readDatagram(datagram.data(), datagram.size());
            processFrame(datagram);
        }
    });

    connect(&m_heartbeatServer, &QTcpServer::newConnection, this, [this]() {
        while (m_heartbeatServer.hasPendingConnections()) {
            QTcpSocket *socket = m_heartbeatServer.nextPendingConnection();
            handleHeartbeatConnection(socket);
        }
    });
}

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
    return m_udpSocket.state() == QAbstractSocket::BoundState && m_heartbeatPeer != nullptr;
}

void ZenithProtocolClient::setRemoteHostIp(const QString &ip) { m_remoteHostIp = ip; }
void ZenithProtocolClient::setUdpPort(quint16 port) { m_udpPort = port; }
void ZenithProtocolClient::setTcpPort(quint16 port) { m_tcpPort = port; }
void ZenithProtocolClient::setHeartbeatPort(quint16 port) { m_heartbeatPort = port; }
void ZenithProtocolClient::setRobotId(int robotId) { m_robotId = qMax(1, robotId); }

void ZenithProtocolClient::start()
{
    if (m_udpSocket.state() != QAbstractSocket::BoundState) {
        if (m_udpSocket.bind(QHostAddress::AnyIPv4, m_udpPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            m_udpState = QString("UDP Listen %1").arg(m_udpPort);
            appendLog("UDP telemetry listener started");
        } else {
            m_udpState = QString("UDP Bind Failed: %1").arg(m_udpSocket.errorString());
            appendLog(m_udpState);
        }
    }

    if (!m_heartbeatServer.isListening()) {
        if (m_heartbeatServer.listen(QHostAddress::AnyIPv4, m_heartbeatPort)) {
            m_heartbeatState = QString("Heartbeat Listen %1").arg(m_heartbeatPort);
            appendLog("Heartbeat listener started");
        } else {
            m_heartbeatState = QString("Heartbeat Listen Failed: %1").arg(m_heartbeatServer.errorString());
            appendLog(m_heartbeatState);
        }
    }

    sendModeSelection(true);
    updateLinkStates();
}

void ZenithProtocolClient::stop()
{
    if (m_heartbeatPeer) {
        m_heartbeatPeer->disconnectFromHost();
        m_heartbeatPeer->deleteLater();
        m_heartbeatPeer = nullptr;
    }

    m_udpSocket.close();
    m_heartbeatServer.close();
    m_udpState = QStringLiteral("UDP Idle");
    m_tcpState = QStringLiteral("TCP Disconnected");
    m_heartbeatState = QStringLiteral("Heartbeat Listening Off");
    appendLog("Protocol client stopped");
    updateLinkStates();
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

void ZenithProtocolClient::sendTcpMessage(int msgId, const QVariantMap &payload, int robotId)
{
    QTcpSocket socket;
    socket.connectToHost(m_remoteHostIp, m_tcpPort);
    if (!socket.waitForConnected(1200)) {
        m_tcpState = QString("TCP Connect Failed: %1").arg(socket.errorString());
        appendLog(m_tcpState);
        updateLinkStates();
        return;
    }

    m_tcpState = QString("TCP Sent %1:%2").arg(m_remoteHostIp).arg(m_tcpPort);
    const QByteArray packet = packFrame(msgId, robotId, payload);
    socket.write(packet);
    socket.flush();
    socket.waitForBytesWritten(1200);
    socket.disconnectFromHost();
    appendLog(QString("TCP send msg_id=%1 bytes=%2").arg(msgId).arg(packet.size()));
    updateLinkStates();
}

void ZenithProtocolClient::sendUdpMessage(int msgId, const QVariantMap &payload, int robotId)
{
    if (m_udpSocket.state() != QAbstractSocket::BoundState) {
        appendLog("UDP send skipped: listener not bound");
        return;
    }

    const QByteArray datagram = packFrame(msgId, robotId, payload);
    m_udpSocket.writeDatagram(datagram, QHostAddress(m_remoteHostIp), m_udpPort);
    appendLog(QString("UDP send msg_id=%1 bytes=%2").arg(msgId).arg(datagram.size()));
}

void ZenithProtocolClient::sendModeSelection(bool createMode)
{
    QVariantMap payload;
    payload.insert("mode", ZenithProtocol::UAVBASIC_MODE);
    payload.insert("selectId", QVariantList{m_robotId});
    payload.insert("use_mode", createMode ? ZenithProtocol::UM_CREATE : ZenithProtocol::UM_DELETE);
    payload.insert("is_simulation", false);
    payload.insert("swarm_num", 1);
    payload.insert("cmd", "");
    sendTcpMessage(ZenithProtocol::MODESELECTION, payload, m_robotId);
}

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
    const QJsonDocument document = QJsonDocument::fromJson(payloadBytes);
    if (!document.isObject()) {
        return result;
    }

    result.msgId = static_cast<quint8>(frame[6]);
    result.robotId = static_cast<quint8>(frame[7]);
    result.payload = document.object().toVariantMap();
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
    m_connectionSummary = QString("%1 | %2 | %3").arg(m_udpState, m_tcpState, m_heartbeatState);
    emit transportStateChanged(m_connectionSummary);
    emit linkStatesChanged();
}

void ZenithProtocolClient::processBuffer(QByteArray &buffer)
{
    while (!buffer.isEmpty()) {
        const DecodedFrame decoded = tryDecodeFrame(buffer);
        if (decoded.totalBytes > 0 && !decoded.valid) {
            buffer.remove(0, decoded.totalBytes);
            continue;
        }
        if (!decoded.valid) {
            break;
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
    m_heartbeatState = QString("Heartbeat Peer %1").arg(socket->peerAddress().toString());
    appendLog("Heartbeat connection accepted");
    updateLinkStates();

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (socket != m_heartbeatPeer) {
            return;
        }
        m_heartbeatBuffer.append(socket->readAll());
        processBuffer(m_heartbeatBuffer);
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        if (socket == m_heartbeatPeer) {
            m_heartbeatPeer = nullptr;
            m_heartbeatState = QString("Heartbeat Listen %1").arg(m_heartbeatPort);
            updateLinkStates();
        }
        socket->deleteLater();
    });
}
