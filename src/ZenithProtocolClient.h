#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
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
    bool telemetryFresh() const;
    bool heartbeatFresh() const;
    bool canSendControlCommands() const;

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
    void noteModeSelectionAck();
    bool awaitingModeSelectionAck() const;

signals:
    void decodedMessage(int msgId, int robotId, const QVariantMap &payload);
    void transportStateChanged(const QString &stateText);
    void protocolLog(const QString &line);
    void linkStatesChanged();
    void tcpConnectedChanged(bool connected);

private slots:
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpReadyRead();
    void onTcpError(QAbstractSocket::SocketError error);
    void onReconnectTimer();
    void onHeartbeatTimer();

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
    void resetFreshness();
    void noteUdpRx();
    void noteHeartbeatRx();
    void noteTcpRx();
    qint64 nowMs() const;
    void connectTcp();
    void disconnectTcp();

    QString m_remoteHostIp = QStringLiteral("127.0.0.1");
    quint16 m_udpPort = 8889;
    quint16 m_tcpPort = 55555;
    quint16 m_heartbeatPort = 55556;
    int m_robotId = 1;

    // UDP telemetry receiver
    QUdpSocket m_udpSocket;

    // Persistent TCP command connection
    QTcpSocket m_tcpSocket;
    QByteArray m_tcpRecvBuffer;
    bool m_tcpIntentionalDisconnect = false;

    // Reconnect timer
    QTimer m_reconnectTimer;
    static constexpr int kReconnectIntervalMs = 3000;

    // Heartbeat sending timer
    QTimer m_heartbeatTimer;
    quint32 m_heartbeatCount = 0;
    static constexpr int kHeartbeatIntervalMs = 1000;

    // Heartbeat receiving (from Jetson)
    QTcpServer m_heartbeatServer;
    QTcpSocket *m_heartbeatPeer = nullptr;
    QByteArray m_heartbeatBuffer;
    QTimer m_linkMonitorTimer;

    qint64 m_lastUdpRxMs = 0;
    qint64 m_lastHeartbeatRxMs = 0;
    qint64 m_lastTcpRxMs = 0;
    bool m_hasEverConnected = false;

    static constexpr int kUdpFreshnessTimeoutMs = 3000;
    static constexpr int kHeartbeatFreshnessTimeoutMs = 3500;
    static constexpr int kLinkMonitorIntervalMs = 500;

    // State strings
    QString m_udpState = QStringLiteral("IDLE");
    QString m_tcpState = QStringLiteral("DISCONNECTED");
    QString m_heartbeatState = QStringLiteral("IDLE");
    QString m_connectionSummary = QStringLiteral("STOPPED");
    QString m_protocolLogText;

    // Whether start() has been called
    bool m_active = false;
    // Whether ModeSelection needs to be sent on TCP connect
    bool m_pendingModeSelection = false;
    // HANDSHAKING: waiting for ModeSelection ACK from Jetson
    bool m_awaitingModeSelectionAck = false;
    QTimer m_modeSelectionAckTimer;
    int m_modeSelectionRetries = 0;
    static constexpr int kModeSelectionAckTimeoutMs = 5000;
    static constexpr int kModeSelectionMaxRetries = 1;
};
