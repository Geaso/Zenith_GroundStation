#include "ZenithMavlinkCodec.h"
#include "ZenithProtocol.h"
#include <zenith_protocol/mavlink_wire.hpp>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QtMath>
#include <array>
#include <cmath>

namespace {
QByteArray bytes(const zenith::mavlink::Bytes &value)
{
    return QByteArray(reinterpret_cast<const char *>(value.data()), qsizetype(value.size()));
}

QString px4Mode(quint32 customMode)
{
    const int main = (customMode >> 16) & 255, sub = (customMode >> 24) & 255;
    switch (main) {
    case 1: return "MANUAL";
    case 2: return "ALTCTL";
    case 3: return "POSCTL";
    case 4:
        switch (sub) {
        case 2: return "AUTO.TAKEOFF";
        case 3: return "AUTO.LOITER";
        case 4: return "AUTO.MISSION";
        case 5: return "AUTO.RTL";
        case 6: return "AUTO.LAND";
        default: return "AUTO";
        }
    case 5: return "ACRO";
    case 6: return "OFFBOARD";
    case 7: return "STABILIZED";
    case 8: return "RATTITUDE";
    default: return QString("CUSTOM(%1)").arg(customMode);
    }
}

bool finite(std::initializer_list<double> values)
{
    for (double value : values) if (!std::isfinite(value)) return false;
    return true;
}

// The UI's indexed representation predates the transport migration. Keep that
// model boundary local; the wire always uses the shared bridge's datas array.
bool customDataArray(const QVariantMap &payload, QVariantList &datas)
{
    if (payload.contains("datas")) {
        if (payload.value("datas").typeId() != QMetaType::QVariantList) return false;
        datas = payload.value("datas").toList();
    } else {
        bool valid = false;
        const int count = payload.value("datas_num").toInt(&valid);
        if (!valid || count < 0 || count > 1024) return false;
        for (int i = 0; i < count; ++i) {
            const auto name = QString("name[%1]").arg(i);
            const auto type = QString("type[%1]").arg(i);
            const auto value = QString("value[%1]").arg(i);
            if (!payload.contains(name) || !payload.contains(type) || !payload.contains(value)) return false;
            datas.append(QVariantMap{{"name", payload.value(name)}, {"type", payload.value(type)},
                                     {"value", payload.value(value)}});
        }
    }
    if (datas.size() > 1024) return false;
    for (const auto &data : datas) {
        if (data.typeId() != QMetaType::QVariantMap) return false;
        const auto item = data.toMap();
        const double type = item.value("type", -1).toDouble();
        if (item.value("name").typeId() != QMetaType::QString
            || item.value("value").typeId() != QMetaType::QString
            || !std::isfinite(type) || std::floor(type) != type || type < 1 || type > 5) return false;
    }
    return true;
}

bool customDataForModel(QVariantMap &payload)
{
    QVariantList datas;
    if (!customDataArray(payload, datas)) return false;
    payload = {{"datas_num", datas.size()}};
    for (int i = 0; i < datas.size(); ++i) {
        const auto item = datas.at(i).toMap();
        payload[QString("name[%1]").arg(i)] = item.value("name");
        payload[QString("type[%1]").arg(i)] = item.value("type");
        payload[QString("value[%1]").arg(i)] = item.value("value");
    }
    return true;
}
}

class ZenithMavlinkCodec::Impl
{
public:
    zenith::mavlink::Encoder encoder{255, MAV_COMP_ID_MISSIONPLANNER};
    std::array<zenith::mavlink::Parser, 4> parsers;
    std::array<zenith::mavlink::Reassembler, 4> assemblers;
    QElapsedTimer timer;
    quint32 transaction = 0;
    int target = 1;
    std::array<int, 4> mappedFcus{};
    int channel = 0;
    struct State { QVariantMap fields; QHash<QString, qint64> fieldTimes; qint64 at = 0; qint64 heartbeatAt = -1; };
    std::array<QHash<int, State>, 4> channelStates;

    Impl() { timer.start(); mappedFcus.fill(-1); }
    void state(QVector<Message> &result, const mavlink_message_t &message,
               QVariantMap fields, bool sample, bool heartbeat = false)
    {
        auto &states = channelStates[channel];
        const int mappedFcu = mappedFcus[channel];
        if (states.size() >= 16 && !states.contains(message.sysid)) return;
        auto &cached = states[message.sysid];
        for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
            cached.fields[it.key()] = it.value();
            cached.fieldTimes[it.key()] = timer.elapsed();
        }
        cached.at = timer.elapsed();
        if (heartbeat) cached.heartbeatAt = cached.at;
        const int expected = mappedFcu >= 0 ? mappedFcu : target;
        if (message.sysid != expected) return;
        fields["_partial"] = true;
        fields["_telemetry_sample"] = sample;
        result.push_back({ZenithProtocol::UAVSTATE, target, fields, false, sample});
    }

    void standard(QVector<Message> &out, const mavlink_message_t &message)
    {
        const int mappedFcu = mappedFcus[channel];
        if (message.compid == zenith::mavlink::kComponent && message.sysid == target) {
            QVariantMap fields;
            if (message.msgid == MAVLINK_MSG_ID_ALTITUDE) {
                mavlink_altitude_t v{}; mavlink_msg_altitude_decode(&message, &v);
                if (std::isfinite(v.altitude_relative)) fields["rel_alt"] = v.altitude_relative;
            } else if (message.msgid == MAVLINK_MSG_ID_DISTANCE_SENSOR) {
                mavlink_distance_sensor_t v{}; mavlink_msg_distance_sensor_decode(&message, &v);
                if (v.orientation == MAV_SENSOR_ROTATION_PITCH_270 && v.current_distance >= v.min_distance
                    && v.current_distance <= v.max_distance) fields["range"] = v.current_distance / 100.0;
            }
            const auto fcu = channelStates[channel].value(mappedFcu >= 0 ? mappedFcu : target);
            for (auto it = fields.begin(); it != fields.end();)
                if (fcu.fieldTimes.contains(it.key()) && timer.elapsed() - fcu.fieldTimes.value(it.key()) < 3500)
                    it = fields.erase(it);
                else ++it;
            if (!fields.isEmpty()) {
                fields["_partial"] = true; fields["_telemetry_sample"] = false;
                out.push_back({ZenithProtocol::UAVSTATE, target, fields, false, false});
            }
            return;
        }
        if (message.compid != MAV_COMP_ID_AUTOPILOT1) return;
        switch (message.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            mavlink_heartbeat_t value{};
            mavlink_msg_heartbeat_decode(&message, &value);
            if (value.autopilot == MAV_AUTOPILOT_INVALID || value.type == MAV_TYPE_GCS) return;
            QVariantMap patch{{"connected", true}, {"armed", bool(value.base_mode & MAV_MODE_FLAG_SAFETY_ARMED)},
                              {"mode", value.autopilot == MAV_AUTOPILOT_PX4 ? px4Mode(value.custom_mode)
                                  : QString("CUSTOM(%1)").arg(value.custom_mode)}};
            state(out, message, patch, false, true);
            if (message.sysid == (mappedFcu >= 0 ? mappedFcu : target))
                out.push_back({ZenithProtocol::HEARTBEAT, target, {{"count", message.seq}}, true, false});
            break;
        }
        case MAVLINK_MSG_ID_LOCAL_POSITION_NED: {
            mavlink_local_position_ned_t v{}; mavlink_msg_local_position_ned_decode(&message, &v);
            if (!finite({v.x, v.y, v.z, v.vx, v.vy, v.vz})) return;
            state(out, message, {{"position", QVariantList{v.y, v.x, -v.z}},
                                 {"velocity", QVariantList{v.vy, v.vx, -v.vz}}}, true);
            break;
        }
        case MAVLINK_MSG_ID_ATTITUDE: {
            mavlink_attitude_t v{}; mavlink_msg_attitude_decode(&message, &v);
            if (!finite({v.roll, v.pitch, v.yaw, v.rollspeed, v.pitchspeed, v.yawspeed})) return;
            state(out, message, {{"attitude", QVariantList{v.roll, -v.pitch, std::remainder(M_PI_2 - v.yaw, 2 * M_PI)}},
                                 {"attitude_rate", QVariantList{v.rollspeed, -v.pitchspeed, -v.yawspeed}}}, true);
            break;
        }
        case MAVLINK_MSG_ID_SYS_STATUS: {
            mavlink_sys_status_t v{}; mavlink_msg_sys_status_decode(&message, &v);
            QVariantMap fields;
            if (v.voltage_battery != UINT16_MAX && v.voltage_battery > 0) fields["battery_state"] = v.voltage_battery / 1000.0;
            if (v.battery_remaining >= 0) fields["battery_percetage"] = v.battery_remaining / 100.0;
            if (!fields.isEmpty()) state(out, message, fields, true);
            break;
        }
        case MAVLINK_MSG_ID_BATTERY_STATUS: {
            mavlink_battery_status_t v{}; mavlink_msg_battery_status_decode(&message, &v);
            if (v.id != 0) return; // The dashboard is the primary battery, not an arbitrary accessory.
            double volts = 0;
            for (int i = 0; i < 10; ++i) if (v.voltages[i] != UINT16_MAX) volts += v.voltages[i] / 1000.0;
            for (int i = 0; i < 4; ++i) if (v.voltages_ext[i] != 0 && v.voltages_ext[i] != UINT16_MAX) volts += v.voltages_ext[i] / 1000.0;
            QVariantMap fields;
            if (volts > 0) fields["battery_state"] = volts;
            if (v.battery_remaining >= 0) fields["battery_percetage"] = v.battery_remaining / 100.0;
            if (!fields.isEmpty()) state(out, message, fields, true);
            break;
        }
        case MAVLINK_MSG_ID_GPS_RAW_INT: {
            mavlink_gps_raw_int_t v{}; mavlink_msg_gps_raw_int_decode(&message, &v);
            QVariantMap fields{{"gps_status", v.fix_type}, {"gps_num", v.satellites_visible}};
            if (v.fix_type >= GPS_FIX_TYPE_3D_FIX) {
                fields["latitude"] = v.lat / 1e7; fields["longitude"] = v.lon / 1e7;
                fields["altitude"] = v.alt / 1000.0;
            }
            state(out, message, fields, true);
            break;
        }
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
            mavlink_global_position_int_t v{}; mavlink_msg_global_position_int_decode(&message, &v);
            state(out, message, {{"latitude", v.lat / 1e7}, {"longitude", v.lon / 1e7},
                                 {"altitude", v.alt / 1000.0}, {"rel_alt", v.relative_alt / 1000.0}}, true);
            break;
        }
        case MAVLINK_MSG_ID_ALTITUDE: {
            mavlink_altitude_t v{}; mavlink_msg_altitude_decode(&message, &v);
            if (std::isfinite(v.altitude_relative)) state(out, message, {{"rel_alt", v.altitude_relative}}, true);
            break;
        }
        case MAVLINK_MSG_ID_DISTANCE_SENSOR: {
            mavlink_distance_sensor_t v{}; mavlink_msg_distance_sensor_decode(&message, &v);
            if (v.orientation == MAV_SENSOR_ROTATION_PITCH_270 && v.current_distance >= v.min_distance
                && v.current_distance <= v.max_distance)
                state(out, message, {{"range", v.current_distance / 100.0}}, true);
            break;
        }
        case MAVLINK_MSG_ID_STATUSTEXT: {
            if (message.sysid != (mappedFcu >= 0 ? mappedFcu : target)) return;
            mavlink_statustext_t v{}; mavlink_msg_statustext_decode(&message, &v);
            out.push_back({ZenithProtocol::TEXTINFO, target,
                           {{"MessageType", v.severity <= MAV_SEVERITY_ERROR ? 2 : v.severity <= MAV_SEVERITY_WARNING ? 1 : 0},
                            {"Message", QString::fromUtf8(v.text, qsizetype(strnlen(v.text, sizeof v.text)))}}, false, false});
            break;
        }
        case MAVLINK_MSG_ID_COMMAND_ACK: {
            if (message.sysid != (mappedFcu >= 0 ? mappedFcu : target)) return;
            mavlink_command_ack_t v{}; mavlink_msg_command_ack_decode(&message, &v);
            out.push_back({ZenithProtocol::TEXTINFO, target,
                           {{"MessageType", v.result == MAV_RESULT_ACCEPTED ? 0 : 1},
                            {"Message", QString("COMMAND_ACK: command=%1 result=%2").arg(v.command).arg(v.result)}}, false, false});
            break;
        }
        default: break;
        }
    }

    void extension(QVector<Message> &out, const mavlink_message_t &message)
    {
        if (message.compid != zenith::mavlink::kComponent) return;
        mavlink_v2_extension_t raw{}; mavlink_msg_v2_extension_decode(&message, &raw);
        if ((raw.target_system && raw.target_system != 255)
            || (raw.target_component && raw.target_component != MAV_COMP_ID_MISSIONPLANNER)) return;
        if (message.sysid != target) {
            // Preserve the existing pairing mismatch indicator without applying foreign state.
            out.push_back({-1, message.sysid, {}, false, false});
            return;
        }
        auto value = assemblers[channel].accept(message, timer.elapsed());
        if (!value || value->encoding != 1) return;
        const QJsonDocument json = QJsonDocument::fromJson(bytes(value->payload));
        if (!json.isObject()) return;
        QVariantMap payload = json.object().toVariantMap();
        if (value->kind == ZenithProtocol::CUSTOMDATASEGMENT_1 && !customDataForModel(payload)) return;
        for (const QString &key : {QStringLiteral("gm_data"), QStringLiteral("pp_data"), QStringLiteral("vm_data")}) {
            if (!payload.contains(key)) continue;
            if (payload.value(key).typeId() != QMetaType::QString) return;
            const auto decoded = QByteArray::fromBase64Encoding(payload.value(key).toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
            if (!decoded) return;
            payload[key] = decoded.decoded;
        }
        if (value->kind == ZenithProtocol::UAVSTATE) {
            // Only companion-owned fields are allowed through this extension.
            const QStringList allowed = {"uav_id", "fcu_system_id", "location_source", "odom_valid",
                                         "vins_position", "secs", "nsecs"};
            for (auto it = payload.begin(); it != payload.end();)
                if (!allowed.contains(it.key())) it = payload.erase(it); else ++it;
            const double associationNumber = payload.value("fcu_system_id", 0).toDouble();
            if (!std::isfinite(associationNumber) || associationNumber < 0 || associationNumber > 255
                || std::floor(associationNumber) != associationNumber) return;
            const int association = int(associationNumber);
            int &mappedFcu = mappedFcus[channel];
            if (payload.contains("fcu_system_id") && association >= 0 && association < 256 && association != mappedFcu) {
                mappedFcu = association;
                out.push_back({ZenithProtocol::UAVSTATE, target,
                    {{"_partial", true}, {"_reset_fcu", true}, {"connected", false}}, false, false});
                const auto cached = channelStates[channel].value(mappedFcu);
                if (mappedFcu && timer.elapsed() - cached.at < 5000 && !cached.fields.isEmpty()) {
                    auto replay = cached.fields;
                    replay["connected"] = cached.heartbeatAt >= 0 && timer.elapsed() - cached.heartbeatAt < 3500;
                    replay["_partial"] = true; replay["_telemetry_sample"] = false;
                    out.push_back({ZenithProtocol::UAVSTATE, target, replay, false, false});
                }
            }
            payload["_partial"] = true;
            payload["_telemetry_sample"] = false;
        }
        out.push_back({value->kind, target, payload, value->kind == ZenithProtocol::HEARTBEAT, false});
    }
};

ZenithMavlinkCodec::ZenithMavlinkCodec() : d(std::make_unique<Impl>()) {}
ZenithMavlinkCodec::~ZenithMavlinkCodec() = default;

QByteArray ZenithMavlinkCodec::encode(int kind, int targetSystem, const QVariantMap &payload)
{
    if (targetSystem <= 0 || targetSystem >= 255 || kind < 0 || kind > 255) return {};
    if (kind == ZenithProtocol::HEARTBEAT) {
        mavlink_heartbeat_t hb{}; hb.type = MAV_TYPE_GCS; hb.autopilot = MAV_AUTOPILOT_INVALID;
        hb.system_status = MAV_STATE_ACTIVE; hb.mavlink_version = 3;
        mavlink_message_t message{};
        mavlink_msg_heartbeat_encode(255, MAV_COMP_ID_MISSIONPLANNER, &message, &hb);
        return bytes(d->encoder.encode(message));
    }
    QVariantMap jsonPayload = payload;
    if (kind == ZenithProtocol::CUSTOMDATASEGMENT_1) {
        QVariantList datas;
        if (!customDataArray(payload, datas)) return {};
        jsonPayload = {{"datas", datas}};
    }
    for (auto it = jsonPayload.begin(); it != jsonPayload.end(); ++it)
        if (it.value().typeId() == QMetaType::QByteArray) it.value() = QString::fromLatin1(it.value().toByteArray().toBase64());
    const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(jsonPayload)).toJson(QJsonDocument::Compact);
    zenith::mavlink::Bytes body(json.begin(), json.end());
    QByteArray result;
    for (const auto &packet : d->encoder.encodeExtension(quint8(kind), ++d->transaction, body, 1, quint8(targetSystem), zenith::mavlink::kComponent))
        result += bytes(packet);
    return result;
}

QVector<ZenithMavlinkCodec::Message> ZenithMavlinkCodec::append(const QByteArray &data, int channel)
{
    QVector<Message> result;
    if (channel < 0 || channel >= int(d->parsers.size())) return result;
    d->channel = channel;
    for (unsigned char byte : data) {
        const auto decoded = d->parsers[channel].consume(byte);
        if (!decoded || decoded->magic != MAVLINK_STX) continue;
        if (decoded->msgid == MAVLINK_MSG_ID_V2_EXTENSION) d->extension(result, *decoded);
        else if (decoded->msgid == MAVLINK_MSG_ID_HEARTBEAT && decoded->compid == zenith::mavlink::kComponent && decoded->sysid == d->target)
            result.push_back({ZenithProtocol::HEARTBEAT, d->target, {{"count", decoded->seq}}, true, false});
        else d->standard(result, *decoded);
    }
    return result;
}

void ZenithMavlinkCodec::resetReceive()
{
    for (auto &parser : d->parsers) parser.reset();
    for (auto &assembler : d->assemblers) assembler.reset();
    for (auto &states : d->channelStates) states.clear();
    d->mappedFcus.fill(-1);
}

void ZenithMavlinkCodec::setTargetSystem(int systemId)
{
    if (systemId > 0 && systemId < 255 && d->target != systemId) {
        d->target = systemId;
        resetReceive();
    }
}

bool ZenithMavlinkCodec::fcuHeartbeatFresh() const
{
    for (size_t i = 0; i < d->channelStates.size(); ++i) {
        const auto state = d->channelStates[i].value(d->mappedFcus[i] >= 0 ? d->mappedFcus[i] : d->target);
        if (state.heartbeatAt >= 0 && d->timer.elapsed() - state.heartbeatAt < 3500) return true;
    }
    return false;
}
