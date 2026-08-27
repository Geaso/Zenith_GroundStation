#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Lr24RadioProtocol {

inline constexpr quint8 Sof = 0xEF;
inline constexpr int HeaderSize = 6;
inline constexpr int ChecksumSize = 1;
inline constexpr int MaximumPayloadSize = 100;

inline constexpr quint8 PcGcsDeviceId = 1;
inline constexpr quint8 RadioDeviceId = 30;
inline constexpr quint8 HeartbeatMessageId = 1;
inline constexpr quint8 CommandMessageId = 2;
inline constexpr quint8 CommandAckMessageId = 3;
inline constexpr quint8 RadioProductModel = 30;

inline constexpr quint16 UnpairedAddress = 1000;

enum class ParameterVersion {
    V1,
    V2,
    V3,
    V4
};

struct Frame {
    quint8 deviceId{0};
    quint8 systemId{0};
    quint8 messageId{0};
    quint8 sequence{0};
    QByteArray payload;
};

enum class DecodeStatus {
    FrameDecoded,
    NeedMoreData,
    InvalidData
};

struct DecodeResult {
    DecodeStatus status{DecodeStatus::NeedMoreData};
    Frame frame;
    int bytesConsumed{0};
    QString error;
};

struct ParseEvent {
    enum class Type {
        FrameDecoded,
        Error
    };

    Type type{Type::Error};
    Frame frame;
    int bytesConsumed{0};
    QString error;
};

// Keeps incomplete input between calls and resynchronizes after noise or a bad
// frame. It does no I/O, so it can be owned by any serial-port state machine.
class StreamParser
{
public:
    QVector<ParseEvent> append(const QByteArray &data);
    void reset();
    int bufferedBytes() const;

private:
    QByteArray m_buffer;
};

struct RadioHeartbeat {
    quint8 systemId{0};
    quint32 systemTime{0};
    quint8 productModel{0};
    quint8 hardwareVersion{0};
    quint8 softwarePatch{0};
    quint8 softwareMinor{0};
    quint8 softwareMajor{0};
};

struct CommandAck {
    quint16 commandId{0};
    QByteArray parameters;
};

quint8 calculateChecksum(const QByteArray &bytes);
QByteArray encodeFrame(const Frame &frame, QString *error = nullptr);
DecodeResult decodeOne(const QByteArray &bytes);

QByteArray makeHostHeartbeat(quint8 sequence, quint32 systemTime,
                             quint8 systemId = 0, QString *error = nullptr);
bool decodeRadioHeartbeat(const Frame &frame, RadioHeartbeat *heartbeat,
                          QString *error = nullptr);

ParameterVersion parameterVersionForProductModel(quint8 productModel);
int parameterSize(ParameterVersion version);
quint16 getParametersCommand(quint8 productModel);
quint16 setParametersCommand(quint8 productModel);

QByteArray makeGetAddressCommand(quint8 productModel, quint8 systemId,
                                 quint8 sequence, QString *error = nullptr);
bool decodeCommandAck(const Frame &frame, CommandAck *ack,
                      QString *error = nullptr);
bool decodeAddressResponse(const Frame &frame, quint8 productModel,
                           quint16 *address, QByteArray *currentParameters = nullptr,
                           QString *error = nullptr);

// currentParameters must be the parameter bytes returned by GET, without the
// two-byte command id. The complete SET payload is always padded to 18 bytes.
QByteArray makeSetAddressCommand(quint8 productModel, quint8 systemId,
                                 quint8 sequence, const QByteArray &currentParameters,
                                 quint16 address, QString *error = nullptr);

// Radio hardware accepts 1..30000. The GroundStation product policy narrows
// assignable aircraft numbers to 1..254 and treats 1000 as "unpaired".
bool isHardwareAddressValid(int address);
bool isFleetAddressAssignable(int address);

} // namespace Lr24RadioProtocol
