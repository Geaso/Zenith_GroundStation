#ifndef ZENITH_MSGPACK_H
#define ZENITH_MSGPACK_H

#include <QByteArray>
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <cstring>

// MsgPack 整数键 → JSON 字段名映射（与 Bridge 端 MpKey 一一对应）
namespace ZenithMsgPack {

// ── 键ID→字段名映射表 ──
inline QString keyName(uint8_t key) {
    switch (key) {
    // UAVState (msg_id=1)
    case 1:  return QStringLiteral("secs");
    case 2:  return QStringLiteral("uav_id");
    case 3:  return QStringLiteral("location_source");
    case 4:  return QStringLiteral("mode");
    case 5:  return QStringLiteral("connected");
    case 6:  return QStringLiteral("armed");
    case 7:  return QStringLiteral("odom_valid");
    case 8:  return QStringLiteral("gps_status");
    case 9:  return QStringLiteral("gps_num");
    case 10: return QStringLiteral("latitude");
    case 11: return QStringLiteral("longitude");
    case 12: return QStringLiteral("altitude");
    case 13: return QStringLiteral("position");
    case 14: return QStringLiteral("velocity");
    case 15: return QStringLiteral("attitude");
    case 16: return QStringLiteral("attitude_rate");
    case 17: return QStringLiteral("battery_state");
    case 18: return QStringLiteral("battery_percetage");
    case 19: return QStringLiteral("rel_alt");
    case 20: return QStringLiteral("range");
    case 21: return QStringLiteral("vins_position");
    // UAVControlState (msg_id=9)
    case 30: return QStringLiteral("control_state");
    case 31: return QStringLiteral("pos_controller");
    case 32: return QStringLiteral("failsafe");
    case 33: return QStringLiteral("exec_state");
    case 34: return QStringLiteral("mission_mode");
    case 35: return QStringLiteral("active_command_source");
    case 36: return QStringLiteral("pending_request");
    case 37: return QStringLiteral("request_active");
    // Heartbeat (msg_id=6)
    case 40: return QStringLiteral("count");
    case 41: return QStringLiteral("message");
    // TextInfo (msg_id=3)
    case 50: return QStringLiteral("MessageType");
    case 51: return QStringLiteral("Message");
    case 52: return QStringLiteral("sec");
    default: return QString("_key_%1").arg(key);
    }
}

// ── MsgPack 解码器 ──
// 将 MsgPack 二进制 payload 解码为 QVariantMap（字段名与 JSON 模式一致）
// 返回 true 成功，false 解码失败

class Decoder {
public:
    Decoder(const QByteArray &data) : m_data(data), m_pos(0) {}

    bool decode(QVariantMap &result) {
        QVariant v = readValue();
        if (v.typeId() == QMetaType::QVariantMap) {
            result = v.toMap();
            return true;
        }
        return false;
    }

private:
    const QByteArray &m_data;
    int m_pos;

    uint8_t peekByte() const {
        return (m_pos < m_data.size()) ? static_cast<uint8_t>(m_data[m_pos]) : 0;
    }
    uint8_t readByte() {
        return (m_pos < m_data.size()) ? static_cast<uint8_t>(m_data[m_pos++]) : 0;
    }
    uint16_t readUint16BE() {
        uint16_t v = (static_cast<uint16_t>(readByte()) << 8) | readByte();
        return v;
    }
    uint32_t readUint32BE() {
        uint32_t v = (static_cast<uint32_t>(readByte()) << 24)
                   | (static_cast<uint32_t>(readByte()) << 16)
                   | (static_cast<uint32_t>(readByte()) << 8)
                   | readByte();
        return v;
    }
    float readFloat32BE() {
        uint32_t bits = readUint32BE();
        float f;
        std::memcpy(&f, &bits, 4);
        return f;
    }
    double readFloat64BE() {
        uint64_t bits = (static_cast<uint64_t>(readUint32BE()) << 32) | readUint32BE();
        double d;
        std::memcpy(&d, &bits, 8);
        return d;
    }

    QVariant readValue() {
        if (m_pos >= m_data.size()) return QVariant();
        uint8_t b = readByte();

        // positive fixint (0x00 - 0x7F)
        if (b <= 0x7F) return QVariant(static_cast<int>(b));

        // fixmap (0x80 - 0x8F)
        if ((b & 0xF0) == 0x80) return readMap(b & 0x0F);

        // fixarray (0x90 - 0x9F)
        if ((b & 0xF0) == 0x90) return readArray(b & 0x0F);

        // fixstr (0xA0 - 0xBF)
        if ((b & 0xE0) == 0xA0) return readString(b & 0x1F);

        // nil
        if (b == 0xC0) return QVariant();

        // false / true
        if (b == 0xC2) return QVariant(false);
        if (b == 0xC3) return QVariant(true);

        // uint8
        if (b == 0xCC) return QVariant(static_cast<int>(readByte()));
        // uint16
        if (b == 0xCD) return QVariant(static_cast<int>(readUint16BE()));
        // uint32
        if (b == 0xCE) return QVariant(static_cast<quint32>(readUint32BE()));

        // int8
        if (b == 0xD0) return QVariant(static_cast<int>(static_cast<int8_t>(readByte())));
        // int16
        if (b == 0xD1) return QVariant(static_cast<int>(static_cast<int16_t>(readUint16BE())));
        // int32
        if (b == 0xD2) return QVariant(static_cast<int>(static_cast<int32_t>(readUint32BE())));

        // float32
        if (b == 0xCA) return QVariant(static_cast<double>(readFloat32BE()));
        // float64
        if (b == 0xCB) return QVariant(readFloat64BE());

        // str8
        if (b == 0xD9) { uint8_t len = readByte(); return readString(len); }
        // str16
        if (b == 0xDA) { uint16_t len = readUint16BE(); return readString(len); }

        // map16
        if (b == 0xDE) { uint16_t n = readUint16BE(); return readMap(n); }
        // map32
        if (b == 0xDF) { uint32_t n = readUint32BE(); return readMap(static_cast<int>(n)); }

        // array16
        if (b == 0xDC) { uint16_t n = readUint16BE(); return readArray(n); }

        // negative fixint (0xE0 - 0xFF)
        if (b >= 0xE0) return QVariant(static_cast<int>(static_cast<int8_t>(b)));

        return QVariant(); // unsupported type
    }

    QVariant readMap(int count) {
        QVariantMap map;
        for (int i = 0; i < count; ++i) {
            QVariant keyVar = readValue();
            QVariant val = readValue();
            // 整数键 → 字段名
            QString key;
            if (keyVar.typeId() == QMetaType::Int || keyVar.typeId() == QMetaType::UInt) {
                key = keyName(static_cast<uint8_t>(keyVar.toUInt()));
            } else {
                key = keyVar.toString();
            }
            map.insert(key, val);
        }
        return QVariant(map);
    }

    QVariant readArray(int count) {
        QVariantList list;
        list.reserve(count);
        for (int i = 0; i < count; ++i) {
            list.append(readValue());
        }
        return QVariant(list);
    }

    QVariant readString(int len) {
        if (m_pos + len > m_data.size()) len = m_data.size() - m_pos;
        QString s = QString::fromUtf8(m_data.data() + m_pos, len);
        m_pos += len;
        return QVariant(s);
    }
};

// ── 便捷函数：检测 payload 是否为 MsgPack 格式 ──
inline bool isMsgPack(const QByteArray &payload) {
    if (payload.isEmpty()) return false;
    uint8_t first = static_cast<uint8_t>(payload[0]);
    // MsgPack map 的首字节: fixmap(0x80-0x8F), map16(0xDE), map32(0xDF)
    return (first >= 0x80 && first <= 0x8F) || first == 0xDE || first == 0xDF;
}

// ── 便捷函数：解码 MsgPack payload 为 QVariantMap ──
inline bool decodeMsgPack(const QByteArray &payload, QVariantMap &result) {
    Decoder dec(payload);
    return dec.decode(result);
}

} // namespace ZenithMsgPack

#endif // ZENITH_MSGPACK_H
