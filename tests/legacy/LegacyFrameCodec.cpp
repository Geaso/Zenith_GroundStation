#include "LegacyFrameCodec.h"
#include "LegacyMsgPack.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <cstring>
namespace {
constexpr char kMagic0 = 0x61;
constexpr char kMagic1 = 0x6D;
constexpr int kFrameOverhead = 10;
// 栅格帧最坏 35378 字节（133×133 格 × 2 字节 RLE）+ MsgPack 头，取 40KB 留余量。
constexpr quint32 kMaxPayloadSize = 40960;
// 重组缓冲上限必须大于单帧最大长度，否则大栅格帧在慢链路上还没收全就被当成
// "链路噪声" 清掉，表现为栅格永远不刷新。
constexpr int kMaxBufferSize = 65536;

quint16 frameCrc16(const QByteArray &data)
{
    quint16 crc = 0;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

// The legacy generic MsgPack decoder tolerates truncated strings/bins.
// Parse the fixed VOXELMAP contract strictly, without changing other messages.
class VoxelPayloadReader {
public:
    explicit VoxelPayloadReader(const QByteArray &bytes) : data(bytes) {}

    bool decode(QVariantMap &out)
    {
        static const char *const names[] = {
            "vm_origin_x", "vm_origin_y", "vm_xy_resolution", "vm_width", "vm_height",
            "vm_z_min", "vm_z_resolution", "vm_layers", "vm_frame_id",
            "vm_part_index", "vm_part_count", "vm_data", "vm_encoding"
        };
        quint64 tag = 0, count = 0;
        if (!read(1, tag)) return false;
        if ((tag & 0xf0) == 0x80) count = tag & 0x0f;
        else if (tag == 0xde) { if (!read(2, count)) return false; }
        else if (tag == 0xdf) { if (!read(4, count)) return false; }
        else return false;
        if (count != 13) return false;

        quint32 seen = 0;
        for (int i = 0; i < 13; ++i) {
            quint32 key = 0;
            if (!readUnsigned(key) || key < 80 || key > 92) return false;
            const quint32 bit = quint32(1) << (key - 80);
            if (seen & bit) return false;
            seen |= bit;
            QVariant value;
            if (key == 80 || key == 81 || key == 82 || key == 85 || key == 86) {
                double number = 0;
                if (!readFloat(number)) return false;
                value = number;
            } else if (key == 91) {
                quint64 length = 0;
                if (!read(1, tag)) return false;
                if (tag == 0xc4) { if (!read(1, length)) return false; }
                else if (tag == 0xc5) { if (!read(2, length)) return false; }
                else if (tag == 0xc6) { if (!read(4, length)) return false; }
                else return false;
                if (length == 0 || length > ZenithProtocol::VoxelMap::kMaxPartBytes
                    || length % 5 != 0 || length > quint64(data.size() - pos)) return false;
                value = data.mid(pos, qsizetype(length));
                pos += qsizetype(length);
            } else {
                quint32 number = 0;
                if (!readUnsigned(number)) return false;
                value = number;
            }
            out.insert(QString::fromLatin1(names[key - 80]), value);
        }
        return pos == data.size();
    }

private:
    bool read(int count, quint64 &value)
    {
        if (count > data.size() - pos) return false;
        value = 0;
        for (int i = 0; i < count; ++i)
            value = (value << 8) | static_cast<quint8>(data[pos++]);
        return true;
    }
    bool readUnsigned(quint32 &value)
    {
        quint64 tag = 0, number = 0;
        if (!read(1, tag)) return false;
        if (tag <= 0x7f) number = tag;
        else if (tag == 0xcc) { if (!read(1, number)) return false; }
        else if (tag == 0xcd) { if (!read(2, number)) return false; }
        else if (tag == 0xce) { if (!read(4, number)) return false; }
        else return false;
        value = quint32(number);
        return true;
    }
    bool readFloat(double &value)
    {
        quint64 tag = 0, bits = 0;
        if (!read(1, tag)) return false;
        if (tag == 0xca) {
            if (!read(4, bits)) return false;
            const quint32 word = quint32(bits);
            float number;
            std::memcpy(&number, &word, sizeof(number));
            value = number;
        } else if (tag == 0xcb) {
            if (!read(8, bits)) return false;
            std::memcpy(&value, &bits, sizeof(value));
        } else return false;
        return std::isfinite(value);
    }
    const QByteArray &data;
    qsizetype pos = 0;
};
}

ZenithProtocol::FrameDecodeResult ZenithProtocol::decodeFrame(const QByteArray &buffer)
{
    FrameDecodeResult result;
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
    const quint16 actualCrc = frameCrc16(frame.left(totalSize - 2));
    result.totalBytes = totalSize;
    if (expectedCrc != actualCrc) {
        return result;
    }

    const QByteArray payloadBytes = frame.mid(8, static_cast<int>(payloadSize));

    result.msgId = static_cast<quint8>(frame[6]);
    result.robotId = static_cast<quint8>(frame[7]);

    // Auto-detect: MsgPack (首字节 0x80-0x8F/0xDE/0xDF) vs JSON (首字节 '{')
    if (result.msgId == ZenithProtocol::VOXELMAP) {
        if (!VoxelPayloadReader(payloadBytes).decode(result.payload)) return result;
    } else if (ZenithMsgPack::isMsgPack(payloadBytes)) {
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

int ZenithProtocol::consumeFrames(QByteArray &buffer,
                                 const std::function<void(const FrameDecodeResult &)> &onFrame)
{
    int validFrames = 0;
    // A readAll() burst may contain many valid map parts. Drain complete
    // frames before bounding the unconsumed remainder, never flush that burst.
    while (!buffer.isEmpty()) {
        const FrameDecodeResult decoded = decodeFrame(buffer);
        if (decoded.totalBytes > 0 && !decoded.valid) {
            buffer.remove(0, decoded.totalBytes);
            continue;
        }
        if (!decoded.valid) {
            break;
        }

        ++validFrames;
        onFrame(decoded);
        buffer.remove(0, decoded.totalBytes);
    }
    if (buffer.size() > kMaxBufferSize) {
        buffer.clear();
    }
    return validFrames;
}

