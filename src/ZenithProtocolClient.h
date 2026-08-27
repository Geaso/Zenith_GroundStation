#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <QVariantMap>

#include "Lr24RadioProtocol.h"

enum class TransportMode { Network, Serial };

class ZenithProtocolClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int transportMode READ transportMode WRITE setTransportMode NOTIFY transportModeChanged)
    Q_PROPERTY(bool active READ active NOTIFY linkStatesChanged)
    Q_PROPERTY(QString serialPortName READ serialPortName WRITE setSerialPortName NOTIFY serialPortNameChanged)
    Q_PROPERTY(int serialBaudRate READ serialBaudRate WRITE setSerialBaudRate NOTIFY serialBaudRateChanged)
    Q_PROPERTY(QStringList availableSerialPorts READ availableSerialPorts NOTIFY availableSerialPortsChanged)
    Q_PROPERTY(QString serialConnectionState READ serialConnectionState NOTIFY linkStatesChanged)
    Q_PROPERTY(QString serialConnectionStateText READ serialConnectionStateText NOTIFY linkStatesChanged)
    Q_PROPERTY(QString serialDeviceName READ serialDeviceName NOTIFY linkStatesChanged)
    Q_PROPERTY(QString serialDeviceIdentity READ serialDeviceIdentity NOTIFY linkStatesChanged)
    Q_PROPERTY(QString serialActualPortName READ serialActualPortName NOTIFY linkStatesChanged)
    Q_PROPERTY(QString serialLastDataAgeText READ serialLastDataAgeText NOTIFY linkStatesChanged)
    Q_PROPERTY(qint64 serialRxBytesPerSecond READ serialRxBytesPerSecond NOTIFY linkStatesChanged)
    Q_PROPERTY(qint64 serialTxBytesPerSecond READ serialTxBytesPerSecond NOTIFY linkStatesChanged)
    Q_PROPERTY(qulonglong serialRxBytes READ serialRxBytes NOTIFY linkStatesChanged)
    Q_PROPERTY(qulonglong serialTxBytes READ serialTxBytes NOTIFY linkStatesChanged)
    Q_PROPERTY(int serialReconnectCount READ serialReconnectCount NOTIFY linkStatesChanged)
    Q_PROPERTY(int radioTargetAddress READ radioTargetAddress WRITE setRadioTargetAddress NOTIFY radioPairingChanged)
    Q_PROPERTY(int radioActualAddress READ radioActualAddress NOTIFY radioPairingChanged)
    Q_PROPERTY(QString radioActualAddressText READ radioActualAddressText NOTIFY radioPairingChanged)
    Q_PROPERTY(QString radioPairingState READ radioPairingState NOTIFY radioPairingChanged)
    Q_PROPERTY(QString radioPairingStateText READ radioPairingStateText NOTIFY radioPairingChanged)
    Q_PROPERTY(QString radioPairingErrorCode READ radioPairingErrorCode NOTIFY radioPairingChanged)
    Q_PROPERTY(QString radioPairingErrorText READ radioPairingErrorText NOTIFY radioPairingChanged)
    Q_PROPERTY(bool radioPairingReady READ radioPairingReady NOTIFY radioPairingChanged)
    Q_PROPERTY(bool radioAddressMatchesTarget READ radioAddressMatchesTarget NOTIFY radioPairingChanged)

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
    bool active() const;
    void setTransportMode(int mode);
    QString serialPortName() const;
    void setSerialPortName(const QString &name);
    int serialBaudRate() const;
    void setSerialBaudRate(int baud);
    QStringList availableSerialPorts() const;
    QString serialConnectionState() const;
    QString serialConnectionStateText() const;
    QString serialDeviceName() const;
    QString serialDeviceIdentity() const;
    QString serialActualPortName() const;
    QString serialLastDataAgeText() const;
    qint64 serialRxBytesPerSecond() const;
    qint64 serialTxBytesPerSecond() const;
    qulonglong serialRxBytes() const;
    qulonglong serialTxBytes() const;
    int serialReconnectCount() const;
    int radioTargetAddress() const;
    void setRadioTargetAddress(int address);
    int radioActualAddress() const;
    QString radioActualAddressText() const;
    QString radioPairingState() const;
    QString radioPairingStateText() const;
    QString radioPairingErrorCode() const;
    QString radioPairingErrorText() const;
    bool radioPairingReady() const;
    bool radioAddressMatchesTarget() const;
    Q_INVOKABLE bool reapplyRadioPairing();
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
    void radioPairingChanged();

private slots:
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpReadyRead();
    void onTcpError(QAbstractSocket::SocketError error);
    void onReconnectTimer();
    void onHeartbeatTimer();
    void onRadioPairingTimeout();

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
    int processBuffer(QByteArray &buffer);
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
    void openSerial(bool reconnectAttempt);
    void scheduleSerialReconnect(const QString &reason);
    QString resolveSerialPort(bool *ambiguous = nullptr) const;
    void captureSerialIdentity(const QSerialPortInfo &info);
    void clearSerialIdentity();
    void rememberSerialIdentity(const QString &portName, const QSerialPortInfo &info);
    bool restoreSerialIdentity(const QString &portName);
    QSerialPortInfo currentSerialPortInfo() const;
    qint64 writeSerial(const QByteArray &data);
    void updateSerialRates(qint64 now);
    void onSerialReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);
    void beginRadioPairing();
    void resetRadioPairing(bool clearActualAddress);
    void sendRadioConfigHeartbeat();
    void sendRadioGetAddress();
    void sendRadioSetAddress();
    void handleRadioConfigFrame(const Lr24RadioProtocol::Frame &frame);
    void completeRadioPairing();
    void failRadioPairing(const QString &code, const QString &message);

    enum class RadioPairingStage {
        Idle,
        Settling,
        WaitingHeartbeat,
        WaitingInitialRead,
        WaitingSetAck,
        WaitingWriteDelay,
        WaitingVerify,
        Ready,
        Failed
    };
    void setRadioPairingStage(RadioPairingStage stage);

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
    QString m_serialActualPortName;
    QString m_serialDeviceDescription;
    QString m_serialNumber;
    quint16 m_serialVendorId{0};
    quint16 m_serialProductId{0};
    bool m_hasSerialVendorId{false};
    bool m_hasSerialProductId{false};
    bool m_serialOpening{false};
    bool m_serialClosing{false};
    bool m_serialHadValidFrame{false};
    bool m_serialHasValidFrameSinceOpen{false};
    bool m_serialDataInterrupted{false};
    qint64 m_lastSerialValidFrameMs{0};
    qint64 m_lastSerialDisplayFrameMs{0};
    qint64 m_serialOpenedAtMs{0};
    qint64 m_serialRateSampleMs{0};
    quint64 m_serialRxBytes{0};
    quint64 m_serialTxBytes{0};
    quint64 m_serialRateSampleRxBytes{0};
    quint64 m_serialRateSampleTxBytes{0};
    qint64 m_serialRxBytesPerSecond{0};
    qint64 m_serialTxBytesPerSecond{0};
    int m_serialReconnectCount{0};
    int m_serialReconnectBackoffStep{0};
    qint32 m_serialBaudRate{921600};
    static constexpr int kSerialStaleReconnectMs = 3500;

    // LR24 local configuration handshake. While this state machine is active,
    // the COM port is exclusively consumed by Mico configuration frames and
    // normal Zenith traffic is deliberately gated.
    QTimer m_radioPairingTimer;
    Lr24RadioProtocol::StreamParser m_radioConfigParser;
    RadioPairingStage m_radioPairingStage{RadioPairingStage::Idle};
    int m_radioTargetAddress{0};
    int m_radioActualAddress{-1};
    quint8 m_radioProductModel{0};
    quint8 m_radioSystemId{0};
    quint8 m_radioConfigSequence{0};
    int m_radioPairingAttempts{0};
    QByteArray m_radioCurrentParameters;
    QString m_radioLastParseError;
    QString m_radioPairingErrorCode;
    QString m_radioPairingErrorText;
    static constexpr int kRadioOpenSettleMs = 280;
    static constexpr int kRadioRetryIntervalMs = 500;
    static constexpr int kRadioWriteSettleMs = 300;
    static constexpr int kRadioHeartbeatAttempts = 6;
    static constexpr int kRadioQueryAttempts = 4;
};
