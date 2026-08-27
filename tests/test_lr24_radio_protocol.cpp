#include "Lr24RadioProtocol.h"

#include <QCoreApplication>

using namespace Lr24RadioProtocol;

namespace {

bool require(bool condition, const char *message)
{
    if (!condition) {
        qCritical("FAILED: %s", message);
    }
    return condition;
}

QByteArray makeGetAck(quint8 productModel, quint16 address, quint8 systemId = 7)
{
    QByteArray parameters(parameterSize(parameterVersionForProductModel(productModel)), '\0');
    parameters[0] = 0; // duplex
    parameters[1] = 2; // 8 KB/s
    parameters[2] = 3; // power enum; value is preserved by SET
    parameters[3] = 12;
    parameters[4] = static_cast<char>(address & 0xFF);
    parameters[5] = static_cast<char>((address >> 8) & 0xFF);
    parameters[6] = 8;

    QByteArray payload;
    const quint16 command = getParametersCommand(productModel);
    payload.append(static_cast<char>(command & 0xFF));
    payload.append(static_cast<char>((command >> 8) & 0xFF));
    payload.append(parameters);

    Frame frame;
    frame.deviceId = RadioDeviceId;
    frame.systemId = systemId;
    frame.messageId = CommandAckMessageId;
    frame.sequence = 9;
    frame.payload = payload;
    return encodeFrame(frame);
}

QByteArray makeCommandAck(quint16 commandId, quint8 systemId = 7)
{
    Frame frame;
    frame.deviceId = RadioDeviceId;
    frame.systemId = systemId;
    frame.messageId = CommandAckMessageId;
    frame.sequence = 10;
    frame.payload.append(static_cast<char>(commandId & 0xFF));
    frame.payload.append(static_cast<char>((commandId >> 8) & 0xFF));
    return encodeFrame(frame);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    {
        QString error;
        const QByteArray heartbeat = makeHostHeartbeat(5, 0x12345678, 0, &error);
        if (!require(error.isEmpty() && heartbeat.size() == 20, "encode 13-byte host heartbeat")) return 1;
        const DecodeResult decoded = decodeOne(heartbeat);
        if (!require(decoded.status == DecodeStatus::FrameDecoded, "decode host heartbeat")) return 1;
        if (!require(decoded.frame.deviceId == PcGcsDeviceId
                     && decoded.frame.messageId == HeartbeatMessageId
                     && decoded.frame.payload.size() == 13
                     && static_cast<quint8>(decoded.frame.payload.at(4)) == RadioProductModel,
                     "host heartbeat fields")) return 1;
    }

    {
        const QByteArray get = makeGetAddressCommand(32, 0, 1);
        if (!require(get.toHex() == QByteArray("ef01000201020300f8"), "known GET_PARAMS_2 bytes")) return 1;
        if (!require(getParametersCommand(33) == 5 && setParametersCommand(33) == 6,
                     "LR24-P model 33 uses v3 commands")) return 1;
        if (!require(getParametersCommand(48) == 7 && setParametersCommand(49) == 8,
                     "MSTAR models use v4 commands")) return 1;
        if (!require(getParametersCommand(99) == 1 && setParametersCommand(99) == 2,
                     "unknown models fall back to v1 commands")) return 1;
    }

    QByteArray currentParameters;
    {
        const DecodeResult decoded = decodeOne(makeGetAck(32, 301));
        quint16 address = 0;
        QString error;
        if (!require(decoded.status == DecodeStatus::FrameDecoded, "decode GET ACK frame")) return 1;
        if (!require(decodeAddressResponse(decoded.frame, 32, &address, &currentParameters, &error),
                     "parse LR24-F v2 address response")) return 1;
        if (!require(address == 301 && currentParameters.size() == 9,
                     "address is little-endian at parameter offset 4")) return 1;
    }

    {
        QString error;
        const QByteArray set = makeSetAddressCommand(32, 7, 10, currentParameters, 214, &error);
        const DecodeResult decoded = decodeOne(set);
        if (!require(error.isEmpty() && decoded.status == DecodeStatus::FrameDecoded,
                     "encode SET_PARAMS_2 frame")) return 1;
        if (!require(decoded.frame.payload.size() == 18
                     && static_cast<quint8>(decoded.frame.payload.at(0)) == 4
                     && static_cast<quint8>(decoded.frame.payload.at(1)) == 0,
                     "SET command id and fixed payload size")) return 1;
        if (!require(static_cast<quint8>(decoded.frame.payload.at(6)) == 214
                     && static_cast<quint8>(decoded.frame.payload.at(7)) == 0,
                     "SET address at command payload offset 6")) return 1;
        if (!require(decoded.frame.payload.mid(2, 4) == currentParameters.left(4)
                     && decoded.frame.payload.mid(8, 3) == currentParameters.mid(6, 3),
                     "SET preserves every non-address parameter")) return 1;

        const DecodeResult setAckFrame = decodeOne(makeCommandAck(setParametersCommand(32)));
        CommandAck setAck;
        if (!require(setAckFrame.status == DecodeStatus::FrameDecoded
                     && decodeCommandAck(setAckFrame.frame, &setAck, &error)
                     && setAck.commandId == setParametersCommand(32),
                     "decode matching SET command ACK")) return 1;
    }

    {
        StreamParser parser;
        const QByteArray valid = makeGetAck(33, 214);
        QVector<ParseEvent> events = parser.append(QByteArray::fromHex("010203") + valid.left(4));
        if (!require(events.size() == 1 && events.at(0).type == ParseEvent::Type::Error
                     && parser.bufferedBytes() == 4,
                     "stream parser reports noise and retains partial frame")) return 1;
        events = parser.append(valid.mid(4));
        if (!require(events.size() == 1 && events.at(0).type == ParseEvent::Type::FrameDecoded,
                     "stream parser completes fragmented frame")) return 1;

        QByteArray bad = valid;
        bad[bad.size() - 1] ^= 0x01;
        events = parser.append(bad + valid);
        int errors = 0;
        int frames = 0;
        for (const ParseEvent &event : events) {
            errors += event.type == ParseEvent::Type::Error;
            frames += event.type == ParseEvent::Type::FrameDecoded;
        }
        if (!require(errors >= 1 && frames == 1, "stream parser resynchronizes after checksum failure")) return 1;
    }

    {
        QString error;
        const QByteArray shortParams(5, '\0');
        if (!require(makeSetAddressCommand(32, 0, 0, shortParams, 214, &error).isEmpty()
                     && !error.isEmpty(), "reject SET without complete current parameters")) return 1;
        if (!require(makeSetAddressCommand(32, 0, 0, QByteArray(9, '\0'), 0, &error).isEmpty(),
                     "reject hardware address zero")) return 1;
        if (!require(makeSetAddressCommand(32, 0, 0, QByteArray(9, '\0'), 30001, &error).isEmpty(),
                     "reject address beyond hardware range")) return 1;
        if (!require(!makeSetAddressCommand(32, 0, 0, QByteArray(9, '\0'), UnpairedAddress, &error).isEmpty(),
                     "protocol permits unpaired sentinel 1000")) return 1;
        if (!require(isFleetAddressAssignable(1) && isFleetAddressAssignable(254)
                     && !isFleetAddressAssignable(255) && !isFleetAddressAssignable(1000),
                     "product assignment policy is 1..254")) return 1;
    }

    {
        const DecodeResult ackDecoded = decodeOne(makeGetAck(32, 214));
        Frame wrong = ackDecoded.frame;
        wrong.payload[0] = 5;
        quint16 address = 0;
        QString error;
        if (!require(!decodeAddressResponse(wrong, 32, &address, nullptr, &error)
                     && error.contains(QStringLiteral("不匹配")),
                     "reject ACK for a different command")) return 1;

        Frame oversized;
        oversized.payload = QByteArray(MaximumPayloadSize + 1, '\0');
        if (!require(encodeFrame(oversized, &error).isEmpty() && !error.isEmpty(),
                     "reject oversized payload")) return 1;
    }

    qInfo("LR24 radio protocol codec tests passed");
    return 0;
}
