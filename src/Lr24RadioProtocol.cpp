#include "Lr24RadioProtocol.h"

#include <algorithm>

namespace Lr24RadioProtocol {
namespace {

void clearError(QString *error)
{
    if (error) {
        error->clear();
    }
}

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

void appendLe16(QByteArray &bytes, quint16 value)
{
    bytes.append(static_cast<char>(value & 0xFF));
    bytes.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendLe32(QByteArray &bytes, quint32 value)
{
    bytes.append(static_cast<char>(value & 0xFF));
    bytes.append(static_cast<char>((value >> 8) & 0xFF));
    bytes.append(static_cast<char>((value >> 16) & 0xFF));
    bytes.append(static_cast<char>((value >> 24) & 0xFF));
}

quint16 readLe16(const QByteArray &bytes, int offset)
{
    return static_cast<quint16>(static_cast<quint8>(bytes.at(offset)))
        | (static_cast<quint16>(static_cast<quint8>(bytes.at(offset + 1))) << 8);
}

quint32 readLe32(const QByteArray &bytes, int offset)
{
    return static_cast<quint32>(static_cast<quint8>(bytes.at(offset)))
        | (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 1))) << 8)
        | (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 2))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(bytes.at(offset + 3))) << 24);
}

QByteArray makeCommandFrame(quint16 commandId, quint8 systemId, quint8 sequence,
                            const QByteArray &parameters, bool padSetPayload,
                            QString *error)
{
    QByteArray payload;
    payload.reserve(padSetPayload ? 18 : 2 + parameters.size());
    appendLe16(payload, commandId);
    payload.append(parameters);

    if (padSetPayload) {
        if (payload.size() > 18) {
            setError(error, QStringLiteral("SET 命令负载超过 18 字节"));
            return {};
        }
        payload.resize(18, '\0');
    }

    Frame frame;
    frame.deviceId = PcGcsDeviceId;
    frame.systemId = systemId;
    frame.messageId = CommandMessageId;
    frame.sequence = sequence;
    frame.payload = payload;
    return encodeFrame(frame, error);
}

} // namespace

quint8 calculateChecksum(const QByteArray &bytes)
{
    quint32 sum = 0;
    for (const char byte : bytes) {
        sum += static_cast<quint8>(byte);
    }
    return static_cast<quint8>(sum & 0xFF);
}

QByteArray encodeFrame(const Frame &frame, QString *error)
{
    clearError(error);
    if (frame.payload.size() > MaximumPayloadSize) {
        setError(error,
                 QStringLiteral("负载长度 %1 超过协议上限 %2")
                     .arg(frame.payload.size()).arg(MaximumPayloadSize));
        return {};
    }

    QByteArray bytes;
    bytes.reserve(HeaderSize + frame.payload.size() + ChecksumSize);
    bytes.append(static_cast<char>(Sof));
    bytes.append(static_cast<char>(frame.deviceId));
    bytes.append(static_cast<char>(frame.systemId));
    bytes.append(static_cast<char>(frame.messageId));
    bytes.append(static_cast<char>(frame.sequence));
    bytes.append(static_cast<char>(frame.payload.size()));
    bytes.append(frame.payload);
    bytes.append(static_cast<char>(calculateChecksum(bytes)));
    return bytes;
}

DecodeResult decodeOne(const QByteArray &bytes)
{
    DecodeResult result;
    if (bytes.isEmpty()) {
        return result;
    }

    const int start = bytes.indexOf(static_cast<char>(Sof));
    if (start < 0) {
        result.status = DecodeStatus::InvalidData;
        result.bytesConsumed = bytes.size();
        result.error = QStringLiteral("未找到帧头 0xEF，丢弃 %1 字节").arg(bytes.size());
        return result;
    }
    if (start > 0) {
        result.status = DecodeStatus::InvalidData;
        result.bytesConsumed = start;
        result.error = QStringLiteral("帧头前存在 %1 字节噪声").arg(start);
        return result;
    }
    if (bytes.size() < HeaderSize) {
        return result;
    }

    const int payloadLength = static_cast<quint8>(bytes.at(5));
    if (payloadLength > MaximumPayloadSize) {
        result.status = DecodeStatus::InvalidData;
        result.bytesConsumed = 1;
        result.error = QStringLiteral("负载长度 %1 超过协议上限 %2")
                           .arg(payloadLength).arg(MaximumPayloadSize);
        return result;
    }

    const int totalLength = HeaderSize + payloadLength + ChecksumSize;
    if (bytes.size() < totalLength) {
        return result;
    }

    const quint8 expected = calculateChecksum(bytes.left(totalLength - 1));
    const quint8 actual = static_cast<quint8>(bytes.at(totalLength - 1));
    if (actual != expected) {
        result.status = DecodeStatus::InvalidData;
        result.bytesConsumed = 1;
        result.error = QStringLiteral("校验和错误：收到 0x%1，期望 0x%2")
                           .arg(actual, 2, 16, QLatin1Char('0'))
                           .arg(expected, 2, 16, QLatin1Char('0'));
        return result;
    }

    result.status = DecodeStatus::FrameDecoded;
    result.bytesConsumed = totalLength;
    result.frame.deviceId = static_cast<quint8>(bytes.at(1));
    result.frame.systemId = static_cast<quint8>(bytes.at(2));
    result.frame.messageId = static_cast<quint8>(bytes.at(3));
    result.frame.sequence = static_cast<quint8>(bytes.at(4));
    result.frame.payload = bytes.mid(HeaderSize, payloadLength);
    return result;
}

QVector<ParseEvent> StreamParser::append(const QByteArray &data)
{
    m_buffer.append(data);
    QVector<ParseEvent> events;

    while (!m_buffer.isEmpty()) {
        const DecodeResult decoded = decodeOne(m_buffer);
        if (decoded.status == DecodeStatus::NeedMoreData) {
            break;
        }

        ParseEvent event;
        event.bytesConsumed = decoded.bytesConsumed;
        if (decoded.status == DecodeStatus::FrameDecoded) {
            event.type = ParseEvent::Type::FrameDecoded;
            event.frame = decoded.frame;
        } else {
            event.type = ParseEvent::Type::Error;
            event.error = decoded.error;
        }
        events.append(event);

        const int consumed = std::clamp(decoded.bytesConsumed, 1,
                                        static_cast<int>(m_buffer.size()));
        m_buffer.remove(0, consumed);
    }
    return events;
}

void StreamParser::reset()
{
    m_buffer.clear();
}

int StreamParser::bufferedBytes() const
{
    return m_buffer.size();
}

QByteArray makeHostHeartbeat(quint8 sequence, quint32 systemTime,
                             quint8 systemId, QString *error)
{
    QByteArray payload;
    payload.reserve(13);
    appendLe32(payload, systemTime);
    payload.append(static_cast<char>(RadioProductModel));
    payload.append('\0');
    payload.append('\1');
    payload.append('\0');
    payload.append('\1');
    appendLe32(payload, 0);

    Frame frame;
    frame.deviceId = PcGcsDeviceId;
    frame.systemId = systemId;
    frame.messageId = HeartbeatMessageId;
    frame.sequence = sequence;
    frame.payload = payload;
    return encodeFrame(frame, error);
}

bool decodeRadioHeartbeat(const Frame &frame, RadioHeartbeat *heartbeat,
                          QString *error)
{
    clearError(error);
    if (!heartbeat) {
        setError(error, QStringLiteral("心跳输出指针为空"));
        return false;
    }
    if (frame.deviceId != RadioDeviceId || frame.messageId != HeartbeatMessageId) {
        setError(error, QStringLiteral("不是数传设备心跳帧"));
        return false;
    }
    if (frame.payload.size() < 13) {
        setError(error, QStringLiteral("数传心跳负载不足 13 字节"));
        return false;
    }

    heartbeat->systemId = frame.systemId;
    heartbeat->systemTime = readLe32(frame.payload, 0);
    heartbeat->productModel = static_cast<quint8>(frame.payload.at(4));
    heartbeat->hardwareVersion = static_cast<quint8>(frame.payload.at(5));
    heartbeat->softwarePatch = static_cast<quint8>(frame.payload.at(6));
    heartbeat->softwareMinor = static_cast<quint8>(frame.payload.at(7));
    heartbeat->softwareMajor = static_cast<quint8>(frame.payload.at(8));
    return true;
}

ParameterVersion parameterVersionForProductModel(quint8 productModel)
{
    switch (productModel) {
    case 2:
    case 3:
    case 4:
    case 16:
    case 18:
    case 32:
    case 34:
        return ParameterVersion::V2;
    case 33:
        return ParameterVersion::V3;
    case 48:
    case 49:
        return ParameterVersion::V4;
    default:
        return ParameterVersion::V1;
    }
}

int parameterSize(ParameterVersion version)
{
    switch (version) {
    case ParameterVersion::V1: return 8;
    case ParameterVersion::V2: return 9;
    case ParameterVersion::V3: return 10;
    case ParameterVersion::V4: return 12;
    }
    return 0;
}

quint16 getParametersCommand(quint8 productModel)
{
    switch (parameterVersionForProductModel(productModel)) {
    case ParameterVersion::V1: return 1;
    case ParameterVersion::V2: return 3;
    case ParameterVersion::V3: return 5;
    case ParameterVersion::V4: return 7;
    }
    return 1;
}

quint16 setParametersCommand(quint8 productModel)
{
    switch (parameterVersionForProductModel(productModel)) {
    case ParameterVersion::V1: return 2;
    case ParameterVersion::V2: return 4;
    case ParameterVersion::V3: return 6;
    case ParameterVersion::V4: return 8;
    }
    return 2;
}

QByteArray makeGetAddressCommand(quint8 productModel, quint8 systemId,
                                 quint8 sequence, QString *error)
{
    clearError(error);
    return makeCommandFrame(getParametersCommand(productModel), systemId, sequence,
                            QByteArray(), false, error);
}

bool decodeCommandAck(const Frame &frame, CommandAck *ack, QString *error)
{
    clearError(error);
    if (!ack) {
        setError(error, QStringLiteral("ACK 输出指针为空"));
        return false;
    }
    if (frame.deviceId != RadioDeviceId || frame.messageId != CommandAckMessageId) {
        setError(error, QStringLiteral("不是数传 CMD_ACK 帧"));
        return false;
    }
    if (frame.payload.size() < 2) {
        setError(error, QStringLiteral("CMD_ACK 负载不足 2 字节"));
        return false;
    }
    ack->commandId = readLe16(frame.payload, 0);
    ack->parameters = frame.payload.mid(2);
    return true;
}

bool decodeAddressResponse(const Frame &frame, quint8 productModel,
                           quint16 *address, QByteArray *currentParameters,
                           QString *error)
{
    clearError(error);
    if (!address) {
        setError(error, QStringLiteral("地址输出指针为空"));
        return false;
    }

    CommandAck ack;
    if (!decodeCommandAck(frame, &ack, error)) {
        return false;
    }
    const quint16 expectedCommand = getParametersCommand(productModel);
    if (ack.commandId != expectedCommand) {
        setError(error, QStringLiteral("CMD_ACK 命令号 %1 与查询命令 %2 不匹配")
                            .arg(ack.commandId).arg(expectedCommand));
        return false;
    }

    const int requiredSize = parameterSize(parameterVersionForProductModel(productModel));
    if (ack.parameters.size() < requiredSize) {
        setError(error, QStringLiteral("参数区只有 %1 字节，型号 %2 至少需要 %3 字节")
                            .arg(ack.parameters.size()).arg(productModel).arg(requiredSize));
        return false;
    }

    const quint16 decodedAddress = readLe16(ack.parameters, 4);
    if (!isHardwareAddressValid(decodedAddress)) {
        setError(error, QStringLiteral("数传返回超出硬件范围的地址 %1").arg(decodedAddress));
        return false;
    }

    *address = decodedAddress;
    if (currentParameters) {
        *currentParameters = ack.parameters.left(requiredSize);
    }
    return true;
}

QByteArray makeSetAddressCommand(quint8 productModel, quint8 systemId,
                                 quint8 sequence, const QByteArray &currentParameters,
                                 quint16 address, QString *error)
{
    clearError(error);
    if (!isHardwareAddressValid(address)) {
        setError(error, QStringLiteral("数传地址 %1 超出硬件范围 1..30000").arg(address));
        return {};
    }

    const int requiredSize = parameterSize(parameterVersionForProductModel(productModel));
    if (currentParameters.size() < requiredSize) {
        setError(error, QStringLiteral("当前参数只有 %1 字节，型号 %2 至少需要 %3 字节；拒绝覆盖其他配置")
                            .arg(currentParameters.size()).arg(productModel).arg(requiredSize));
        return {};
    }

    QByteArray parameters = currentParameters.left(requiredSize);
    parameters[4] = static_cast<char>(address & 0xFF);
    parameters[5] = static_cast<char>((address >> 8) & 0xFF);
    return makeCommandFrame(setParametersCommand(productModel), systemId, sequence,
                            parameters, true, error);
}

bool isHardwareAddressValid(int address)
{
    return address >= 1 && address <= 30000;
}

bool isFleetAddressAssignable(int address)
{
    return address >= 1 && address <= 254;
}

} // namespace Lr24RadioProtocol
