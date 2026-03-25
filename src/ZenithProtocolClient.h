#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QVariantMap>

class ZenithProtocolClient : public QObject
{
    Q_OBJECT
public:
    explicit ZenithProtocolClient(QObject *parent = nullptr);

    QString remoteHostIp() const;
    quint16 udpPort() const;
    quint16 tcpPort() const;
    quint16 heartbeatPort() const;
    int robotId() const;
    QString udpState() const;
    QString tcpState() const;
    QString heartbeatState() const;
    QString connectionSummary() const;
    QString protocolLogText() const;
    bool isConnected() const;

    void setRemoteHostIp(const QString &ip);
    void setUdpPort(quint16 port);
    void setTcpPort(quint16 port);
    void setHeartbeatPort(quint16 port);
    void setRobotId(int robotId);

    void start();
    void stop();
    bool testConnection();

    void sendTcpMessage(int msgId, const QVariantMap &payload, int robotId);
    void sendUdpMessage(int msgId, const QVariantMap &payload, int robotId);
    void sendModeSelection(bool createMode = true);

signals:
    void decodedMessage(int msgId, int robotId, const QVariantMap &payload);
    void transportStateChanged(const QString &stateText);
    void protocolLog(const QString &line);
    void linkStatesChanged();

private:
    struct DecodedFrame {
        int msgId = -1;
        int robotId = 0;
        QVariantMap payload;
        int totalBytes = 0;
        bool valid = false;
    };

    QByteArray packFrame(int msgId, int robotId, const QVariantMap &payload) const;
    DecodedFrame tryDecodeFrame(const QByteArray &buffer) const;
    quint16 crc16Arc(const QByteArray &data) const;
    void appendLog(const QString &line);
    void updateLinkStates();
    void processBuffer(QByteArray &buffer);
    void processFrame(const QByteArray &frame);
    void handleHeartbeatConnection(QTcpSocket *socket);

    QString m_remoteHostIp = QStringLiteral("127.0.0.1");
    quint16 m_udpPort = 8889;
    quint16 m_tcpPort = 55555;
    quint16 m_heartbeatPort = 55556;
    int m_robotId = 1;
    QUdpSocket m_udpSocket;
    QTcpServer m_heartbeatServer;
    QTcpSocket *m_heartbeatPeer = nullptr;
    QByteArray m_heartbeatBuffer;
    QString m_udpState = QStringLiteral("UDP Idle");
    QString m_tcpState = QStringLiteral("TCP Disconnected");
    QString m_heartbeatState = QStringLiteral("Heartbeat Listening Off");
    QString m_connectionSummary = QStringLiteral("Disconnected");
    QString m_protocolLogText;
};
