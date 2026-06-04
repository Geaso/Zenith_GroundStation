#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantMap>

enum class TransportMode { Network, Serial };

class ZenithProtocolClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int transportMode READ transportMode WRITE setTransportMode NOTIFY transportModeChanged)
    Q_PROPERTY(QString serialPortName READ serialPortName WRITE setSerialPortName NOTIFY serialPortNameChanged)
    Q_PROPERTY(int serialBaudRate READ serialBaudRate WRITE setSerialBaudRate NOTIFY serialBaudRateChanged)
    Q_PROPERTY(QStringList availableSerialPorts READ availableSerialPorts NOTIFY availableSerialPortsChanged)

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

    int transportMode() const;
    void setTransportMode(int mode);
    QString serialPortName() const;
    void setSerialPortName(const QString &name);
    int serialBaudRate() const;
    void setSerialBaudRate(int baud);
    QStringList availableSerialPorts() const;
    Q_INVOKABLE void refreshSerialPorts();

signals:
    void decodedMessage(int msgId, int robotId, const QVariantMap &payload);
    void transportStateChanged(const QString &stateText);
    void protocolLog(const QString &line);
    void linkStatesChanged();
    void tcpConnectedChanged(bool connected);
    void transportModeChanged();
    void serialPortNameChanged();
    void serialBaudRateChanged();
    void availableSerialPortsChanged();

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
    void startNetwork();
    void stopNetwork();
    void startSerial();
    void stopSerial();
    void onSerialReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

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

    // Serial transport
    TransportMode m_transportMode{TransportMode::Network};
    QSerialPort m_serialPort;
    QByteArray m_serialRecvBuffer;
    QString m_serialPortName;
    qint32 m_serialBaudRate{57600};
};
