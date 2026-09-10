#include "ZenithMavlinkCodec.h"
#include "ZenithProtocolClient.h"
#include "ZenithProtocol.h"
#include "TelemetryStore.h"
#include "ManualMoveCommand.h"
#include "CommandDispatcher.h"
#include "ParamStore.h"
#include <zenith_protocol/mavlink_wire.hpp>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpServer>
#include <QThread>
#include <limits>
#include <functional>

namespace {
bool check(bool good, const char *message)
{
    if (!good) qCritical("FAILED: %s", message);
    return good;
}
QByteArray bytes(const zenith::mavlink::Bytes &value)
{
    return QByteArray(reinterpret_cast<const char *>(value.data()), qsizetype(value.size()));
}
QByteArray extension(zenith::mavlink::Encoder &encoder, int kind, const QVariantMap &payload,
                     quint32 transaction = 1, int target = 255, int component = 190)
{
    const auto json = QJsonDocument(QJsonObject::fromVariantMap(payload)).toJson(QJsonDocument::Compact);
    QByteArray result;
    for (const auto &packet : encoder.encodeExtension(quint8(kind), transaction,
             zenith::mavlink::Bytes(json.begin(), json.end()), 1, quint8(target), quint8(component))) result += bytes(packet);
    return result;
}
QByteArray heartbeat(zenith::mavlink::Encoder &encoder, bool fcu)
{
    mavlink_heartbeat_t payload{};
    payload.type = fcu ? MAV_TYPE_QUADROTOR : MAV_TYPE_ONBOARD_CONTROLLER;
    payload.autopilot = fcu ? MAV_AUTOPILOT_PX4 : MAV_AUTOPILOT_INVALID;
    payload.custom_mode = 6u << 16;
    payload.base_mode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED | MAV_MODE_FLAG_SAFETY_ARMED;
    payload.mavlink_version = 3;
    mavlink_message_t m{}; mavlink_msg_heartbeat_encode(1, 1, &m, &payload);
    return bytes(encoder.encode(m));
}
QByteArray position(zenith::mavlink::Encoder &encoder, float north = 10)
{
    mavlink_local_position_ned_t payload{};
    payload.x = north; payload.y = 20; payload.z = -3;
    payload.vx = 1; payload.vy = 2; payload.vz = -0.5f;
    mavlink_message_t m{}; mavlink_msg_local_position_ned_encode(1, 1, &m, &payload);
    return bytes(encoder.encode(m));
}
QByteArray battery(zenith::mavlink::Encoder &encoder)
{
    mavlink_sys_status_t payload{}; payload.voltage_battery = 22500; payload.battery_remaining = 75;
    mavlink_message_t m{}; mavlink_msg_sys_status_encode(1, 1, &m, &payload);
    return bytes(encoder.encode(m));
}
void apply(TelemetryStore &store, const QVector<ZenithMavlinkCodec::Message> &messages)
{
    for (const auto &message : messages) {
        if (message.kind == ZenithProtocol::UAVSTATE) store.applyUavState(message.payload, message.systemId);
        if (message.kind == ZenithProtocol::VOXELMAP) store.applyVoxelMap(message.payload, message.systemId);
    }
}
bool waitFor(const std::function<bool()> &predicate, int timeout = 1500)
{
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < timeout) {
        QCoreApplication::processEvents(); QThread::msleep(2);
    }
    return predicate();
}

bool commands()
{
    auto v = ManualMoveCommand::build("XYZ_VEL", 1, 2, 3, 90, 42);
    if (!check(v.value("velocity_ref").toList() == QVariantList{1.0, 2.0, 3.0}
             && v.value("position_ref").toList() == QVariantList{0.0, 0.0, 0.0}, "velocity input populates velocity fields")) return false;
    v = ManualMoveCommand::build("XY_VEL_Z_POS_BODY", 1, 2, 3, 90, 42);
    if (!check(v.value("velocity_ref").toList() == QVariantList{1.0, 2.0, 0.0}
             && v.value("position_ref").toList() == QVariantList{0.0, 0.0, 3.0}, "mixed control preserves velocity XY and position Z")) return false;
    v = ManualMoveCommand::build("LAT_LON_ALT", 22, 113, 100, 0, 42);
    if (!check(v.value("latitude").toDouble() == 22 && v.value("longitude").toDouble() == 113
             && v.value("altitude").toDouble() == 100, "global coordinates use global fields")) return false;
    if (!check(ManualMoveCommand::build("INVALID", 1, 2, 3, 0, 1).isEmpty()
             && ManualMoveCommand::build("XYZ_POS", std::numeric_limits<double>::quiet_NaN(), 2, 3, 0, 1).isEmpty(), "invalid commands are rejected")) return false;

    ZenithMavlinkCodec codec;
    const QByteArray frame = codec.encode(ZenithProtocol::HEARTBEAT, 214, {});
    zenith::mavlink::Parser parser;
    std::optional<mavlink_message_t> decoded;
    for (unsigned char ch : frame) if (auto m = parser.consume(ch)) decoded = m;
    if (!check(decoded && decoded->magic == 0xfd && decoded->sysid == 255 && decoded->compid == 190
               && decoded->msgid == MAVLINK_MSG_ID_HEARTBEAT, "GCS heartbeat is native common.xml MAVLink2")) return false;
    const QVariantMap command{{"cmd", QString(1800, QLatin1Char('x'))}, {"Command_ID", 0x80000001u}};
    const auto encoded = codec.encode(ZenithProtocol::MODESELECTION, 214, command);
    zenith::mavlink::Reassembler reassembler;
    std::optional<zenith::mavlink::Extension> transfer;
    for (unsigned char ch : encoded) if (auto m = parser.consume(ch)) {
        if (!check(m->msgid == MAVLINK_MSG_ID_V2_EXTENSION, "custom command uses standard V2_EXTENSION id")) return false;
        if (auto value = reassembler.accept(*m, 100)) transfer = value;
    }
    if (!check(transfer && transfer->source_system == 255 && transfer->target_system == 214
               && transfer->target_component == 191 && transfer->kind == 202,
               "fragmented command addresses the selected companion")) return false;
    return check(QJsonDocument::fromJson(bytes(transfer->payload)).object().toVariantMap() == command,
                 "fragmented command preserves payload and uint32 command ids");
}

bool telemetry()
{
    ZenithMavlinkCodec codec; codec.setTargetSystem(214);
    zenith::mavlink::Encoder companion(214, 191), fcu(7, 1), foreign(42, 1);
    TelemetryStore store; store.setVehicleName("UAV214");
    apply(store, codec.append(heartbeat(companion, false)));
    if (!check(!codec.fcuHeartbeatFresh(), "companion heartbeat does not assert FCU online")) return false;
    if (!check(codec.append(heartbeat(fcu, true) + position(fcu) + battery(fcu)).isEmpty(),
               "FCU telemetry is held until its logical vehicle association arrives")) return false;
    codec.append(heartbeat(foreign, true) + position(foreign, 999));
    const auto association = extension(companion, ZenithProtocol::UAVSTATE,
        {{"uav_id", 214}, {"fcu_system_id", 7}, {"connected", true}, {"odom_valid", true}, {"location_source", 12}});
    for (char ch : association) apply(store, codec.append(QByteArray(1, ch)));
    if (!check(codec.fcuHeartbeatFresh() && store.connected() && store.armed() && store.flightMode() == "OFFBOARD"
               && store.positionX() == 20 && store.positionY() == 10 && store.positionZ() == 3
               && store.batteryVoltage() == 22.5 && store.batteryPercent() == 0.75,
               "late mapping replays the correct FCU fields and NED converts to ENU")) return false;
    apply(store, codec.append(position(fcu, 12)));
    if (!check(store.positionY() == 12 && store.batteryVoltage() == 22.5 && store.armed(),
               "a position packet preserves independent battery and arming fields")) return false;
    const auto foreignMessages = codec.append(position(foreign, 1000));
    apply(store, foreignMessages);
    if (!check(foreignMessages.isEmpty() && store.positionY() == 12, "unrelated autopilot cannot overwrite selected vehicle")) return false;
    apply(store, codec.append(extension(companion, 1,
        {{"fcu_system_id", 7}, {"armed", false}, {"mode", "FAKE"}, {"position", QVariantList{999, 999, 999}}}, 9)));
    if (!check(store.armed() && store.flightMode() == "OFFBOARD" && store.positionY() == 12,
               "companion extras cannot replace fields owned by the real FCU")) return false;
    mavlink_attitude_t attitude{}; attitude.roll = .2f; attitude.pitch = .3f; attitude.yaw = 0;
    mavlink_message_t m{}; mavlink_msg_attitude_encode(7, 1, &m, &attitude);
    apply(store, codec.append(bytes(fcu.encode(m))));
    if (!check(std::abs(store.heading() - 90) < .001 && store.pitch() < 0 && store.roll() > 0,
               "FRD/NED attitude converts to FLU/ENU")) return false;
    mavlink_altitude_t altitude{}; altitude.altitude_relative = 4.5f;
    mavlink_msg_altitude_encode(214, 191, &m, &altitude);
    apply(store, codec.append(bytes(companion.encode(m))));
    if (!check(store.relativeAltitude() == 4.5, "companion relative altitude uses the common ALTITUDE message")) return false;
    altitude.altitude_relative = 8.5f;
    mavlink_msg_altitude_encode(7, 1, &m, &altitude);
    apply(store, codec.append(bytes(fcu.encode(m))));
    altitude.altitude_relative = 1.0f;
    mavlink_msg_altitude_encode(214, 191, &m, &altitude);
    apply(store, codec.append(bytes(companion.encode(m))));
    if (!check(store.relativeAltitude() == 8.5, "fresh FCU altitude takes precedence over companion fallback")) return false;
    mavlink_distance_sensor_t range{};
    range.orientation = MAV_SENSOR_ROTATION_PITCH_270; range.current_distance = 235; range.max_distance = 1000;
    mavlink_msg_distance_sensor_encode(214, 191, &m, &range);
    apply(store, codec.append(bytes(companion.encode(m))));
    if (!check(store.range() == 2.35, "companion range uses the common downward DISTANCE_SENSOR message")) return false;

    auto malformed = position(fcu, 55); malformed[12] ^= 0x77;
    if (!check(codec.append(malformed).isEmpty(), "CRC-corrupt packet cannot update vehicle state")) return false;
    apply(store, codec.append(position(fcu, 13)));
    if (!check(store.positionY() == 13, "parser recovers after corrupt packet")) return false;
    auto targetedElsewhere = extension(companion, 1, {{"location_source", 99}}, 5, 254);
    if (!check(codec.append(targetedElsewhere).isEmpty(), "wrong extension target is ignored")) return false;
    codec.resetReceive();
    store.setTransportHealth(false, false, "lost");
    apply(store, codec.append(heartbeat(companion, false)));
    return check(!codec.fcuHeartbeatFresh() && store.batteryVoltage() == 0 && !store.telemetryStable(),
                 "reconnection discards FCU mappings and stale battery values");
}

bool sourceIsolation()
{
    ZenithMavlinkCodec codec; codec.setTargetSystem(214);
    zenith::mavlink::Encoder onboard(214, 191), fcu(7, 1);
    TelemetryStore store;
    codec.append(heartbeat(fcu, true) + battery(fcu), 0);
    apply(store, codec.append(extension(onboard, 1, {{"fcu_system_id", 7}}, 1), 1));
    if (!check(!codec.fcuHeartbeatFresh() && store.batteryVoltage() == 0,
               "a TCP association cannot bind an FCU cached on a different byte stream")) return false;
    apply(store, codec.append(heartbeat(fcu, true) + battery(fcu) + position(fcu), 1));
    if (!check(codec.fcuHeartbeatFresh() && store.armed() && store.batteryVoltage() == 22.5,
               "matching FCU on the associated byte stream becomes available")) return false;
    apply(store, codec.append(extension(onboard, 1, {{"fcu_system_id", 9}}, 2), 1));
    if (!check(!codec.fcuHeartbeatFresh() && !store.connected() && !store.armed()
               && store.batteryVoltage() == 0 && store.positionX() == 0,
               "changing FCU association clears the previous complete state even without replacement data")) return false;
    zenith::mavlink::Encoder coincidentId(214, 1);
    codec.append(heartbeat(coincidentId, true), 1);
    apply(store, codec.append(extension(onboard, 1, {{"fcu_system_id", 0}}, 20), 1));
    if (!check(!codec.fcuHeartbeatFresh(), "explicit absent FCU id never falls back to a coincident logical system id")) return false;
    auto multi = extension(onboard, 6, {{"message", QString(900, 'x')}}, 3);
    const auto firstLength = 12 + quint8(multi[1]);
    if (!check(codec.append(multi.left(firstLength), 0).isEmpty()
               && codec.append(multi.mid(firstLength), 1).isEmpty(),
               "fragments from distinct streams never combine into one transfer")) return false;
    if (!check(codec.append(multi.left(firstLength), 1).size() == 1,
               "out-of-order transfer completes only with fragments from its own stream")) return false;
    codec.setTargetSystem(215);
    return check(!codec.fcuHeartbeatFresh() && codec.append(position(fcu), 1).isEmpty(),
                 "changing selected target discards every old source association");
}

bool maps()
{
    ZenithMavlinkCodec codec; codec.setTargetSystem(214);
    zenith::mavlink::Encoder onboard(214, 191);
    QByteArray raw = QByteArray::fromHex("0500000002");
    QVariantMap map{{"vm_origin_x", 0.0}, {"vm_origin_y", 0.0}, {"vm_xy_resolution", 0.15},
        {"vm_width", 2}, {"vm_height", 1}, {"vm_z_min", 0.0}, {"vm_z_resolution", 0.15},
        {"vm_layers", 4}, {"vm_frame_id", 100}, {"vm_part_index", 0}, {"vm_part_count", 1},
        {"vm_encoding", 1}, {"vm_data", QString::fromLatin1(raw.toBase64())}};
    const auto wire = extension(onboard, ZenithProtocol::VOXELMAP, map);
    TelemetryStore store;
    for (qsizetype n = 0; n < wire.size(); n += 7) apply(store, codec.append(wire.mid(n, 7), 3));
    if (!check(store.voxelMapValid() && store.voxelMap().columns == QVector<quint32>{5, 5},
               "MAVLink fragments preserve binary voxel masks through base64 JSON")) return false;
    map["vm_data"] = "*** invalid base64 ***";
    if (!check(codec.append(extension(onboard, ZenithProtocol::VOXELMAP, map, 2)).isEmpty(), "invalid base64 rejected before map reassembly")) return false;

    TelemetryStore gridStore;
    const auto gridMessages = codec.append(extension(onboard, ZenithProtocol::GRIDMAP,
        {{"gm_origin_x", 1.0}, {"gm_origin_y", 2.0}, {"gm_resolution", .15}, {"gm_width", 2}, {"gm_height", 1},
         {"gm_data", QString::fromLatin1(QByteArray::fromHex("6402").toBase64())}}, 30));
    for (const auto &message : gridMessages) gridStore.applyGridMap(message.payload, message.systemId);
    if (!check(gridStore.gridMapValid() && gridStore.gridMapCells() == QVector<uint8_t>{100, 100},
        "canonical GRIDMAP base64 preserves independent 2D RLE display")) return false;
    auto pathPayload = QVariantMap{{"pp_num_points", 1},
        {"pp_data", QString::fromLatin1(QByteArray::fromHex("0000803f0000004000004040").toBase64())}};
    auto applyPath = [&](quint32 transaction) {
        for (const auto &message : codec.append(extension(onboard, ZenithProtocol::PLANNEDPATH, pathPayload, transaction)))
            store.applyPlannedPath(message.payload);
    };
    applyPath(31);
    if (!check(store.plannedPathSize() == 1 && store.plannedPath().first().x == 1 && store.plannedPath().first().z == 3,
        "canonical planned path decodes little-endian float points")) return false;
    pathPayload["pp_num_points"] = 0x40000001u;
    applyPath(32);
    if (!check(store.plannedPathSize() == 1, "overflowing point count cannot read outside the path payload")) return false;
    pathPayload["pp_num_points"] = 1;
    pathPayload["pp_data"] = QString::fromLatin1(QByteArray::fromHex("0000c07f0000004000004040").toBase64());
    applyPath(33);
    if (!check(store.plannedPathSize() == 1 && store.plannedPath().first().x == 1, "NaN point cannot replace valid displayed path")) return false;
    pathPayload = {{"pp_num_points", 0}, {"pp_data", ""}};
    applyPath(34);
    if (!check(store.plannedPathSize() == 0, "explicit empty planner update clears finished path")) return false;
    // Independent parser instances must not mix partial TCP and serial packets.
    auto a = extension(onboard, 6, {{"message", "tcp"}}, 3);
    auto b = extension(onboard, 6, {{"message", "serial"}}, 4);
    if (!check(codec.append(a.left(7), 1).isEmpty() && codec.append(b, 3).size() == 1
               && codec.append(a.mid(7), 1).size() == 1, "transport stream parser state is independent")) return false;
    return true;
}

bool businessSchemas()
{
    ZenithMavlinkCodec codec; codec.setTargetSystem(214);
    const QVariantMap indexed{{"datas_num", 3},
        {"name[0]", "task_name"}, {"type[0]", 5}, {"value[0]", "custom_test"},
        {"name[1]", "task_yaw_enable"}, {"type[1]", 2}, {"value[1]", "false"},
        {"name[2]", "task_path"}, {"type[2]", 5}, {"value[2]", "/home/jetson/task_ws/src/user_tasks/test.py"}};
    zenith::mavlink::Parser parser; zenith::mavlink::Reassembler assembler;
    std::optional<zenith::mavlink::Extension> request;
    for (unsigned char ch : codec.encode(ZenithProtocol::CUSTOMDATASEGMENT_1, 214, indexed))
        if (auto m = parser.consume(ch)) if (auto e = assembler.accept(*m, 1)) request = e;
    if (!check(bool(request), "managed task model encodes into a complete extension")) return false;
    const auto wire = QJsonDocument::fromJson(bytes(request->payload)).object();
    if (!check(wire.size() == 1 && wire.value("datas").toArray().size() == 3
        && wire.value("datas").toArray().at(2).toObject().value("value") == indexed.value("value[2]").toString(),
        "managed task wire uses canonical datas array and preserves task_path")) return false;
    if (!check(codec.encode(113, 214, {{"datas_num", 100000}}).isEmpty(), "unbounded indexed custom data is rejected")) return false;

    zenith::mavlink::Encoder onboard(214, 191);
    QVariantList entries{
        QVariantMap{{"name", "task_state"}, {"type", 5}, {"value", "RUNNING"}},
        QVariantMap{{"name", "task_name"}, {"type", 5}, {"value", "custom_test"}},
        QVariantMap{{"name", "task_ack"}, {"type", 5}, {"value", "ACCEPTED"}},
        QVariantMap{{"name", "task_active"}, {"type", 2}, {"value", "true"}},
        QVariantMap{{"name", "pf_arm_ok"}, {"type", 1}, {"value", "1"}},
        QVariantMap{{"name", "pf_prearm_bit"}, {"type", 1}, {"value", "1"}}
    };
    TelemetryStore store;
    const auto decoded = codec.append(extension(onboard, 113, {{"datas", entries}}));
    for (const auto &message : decoded) store.applyCustomDataSegment(message.payload);
    if (!check(decoded.size() == 1 && store.managedTaskState() == "RUNNING" && store.managedTaskActive()
        && store.managedTaskAck() == "ACCEPTED" && store.preflightValid() && store.preflightArmOk()
        && store.preflightPrearmBit(), "canonical task and preflight reports reach production TelemetryStore")) return false;
    entries.append(QVariantMap{{"name", "bad"}, {"type", 5}, {"value", 42}});
    if (!check(codec.append(extension(onboard, 113, {{"datas", entries}}, 2)).isEmpty(), "non-string custom value rejects whole invalid report")) return false;

    ParamStore params;
    auto batch = [](const QString &name, const QString &value) {
        return QVariantMap{{"param_module", 2}, {"params", QVariantList{
            QVariantMap{{"type", 5}, {"param_name", name}, {"param_value", value}}}}};
    };
    params.applyParamSettings(batch("/bridge/a", "one"));
    params.setValue(0, "local edit");
    params.applyParamSettings(batch("/bridge/b", "two"));
    params.applyParamSettings(batch("/bridge/a", "remote value"));
    if (!check(params.rowCount() == 2 && params.valueAt(0) == "local edit" && params.valueAt(1) == "two"
        && params.dirtyEntries().size() == 1, "parameter batches merge without losing previous batches or local pending edits")) return false;
    params.clearDirty();
    params.applyParamSettings(batch("/bridge/a", "confirmed"));
    if (!check(params.rowCount() == 2 && params.valueAt(0) == "confirmed", "confirmed parameter response updates existing row without duplicates")) return false;
    for (const auto &message : codec.append(extension(onboard, ZenithProtocol::UAVCONTROLSTATE,
        {{"control_state", 2}, {"last_reached_waypoint_id", 4000000000u}, {"request_active", true}}, 3)))
        store.applyUavControlState(message.payload);
    return check(store.lastReachedWaypointId() == 4000000000u && store.requestActive(),
        "waypoint feedback preserves uint32 IDs and active request state");
}

bool tcpLoopback()
{
    QTcpServer server;
    if (!check(server.listen(QHostAddress::LocalHost, 0), "loopback listener binds")) return false;
    ZenithProtocolClient client;
    client.setRobotId(214); client.setTcpPort(server.serverPort()); client.setUdpPort(0);
    client.setRemoteHostIp("127.0.0.1");
    QObject::connect(&client, &ZenithProtocolClient::decodedMessage, &client,
                     [&client](int kind, int, const QVariantMap &p) {
        if (kind == ZenithProtocol::TEXTINFO && p.value("Message").toString().startsWith("MODESELECTION_ACK:")) client.noteModeSelectionAck();
    });
    client.start();
    if (!check(waitFor([&] { return server.hasPendingConnections(); }), "production TCP client connects")) return false;
    QTcpSocket *peer = server.nextPendingConnection();
    if (!check(waitFor([&] { return peer->bytesAvailable() > 0; }), "client immediately establishes GCS source identity")) return false;
    zenith::mavlink::Parser initialParser;
    int initialMessageId = -1;
    for (unsigned char ch : peer->readAll()) {
        if (auto message = initialParser.consume(ch); message && initialMessageId < 0)
            initialMessageId = int(message->msgid);
    }
    if (!check(initialMessageId == MAVLINK_MSG_ID_HEARTBEAT,
               "fresh TCP session sends common HEARTBEAT before the ModeSelection extension")) return false;
    zenith::mavlink::Encoder onboard(214, 191), fcu(7, 1);
    peer->write("noise\x61\x6dgarbage"); peer->flush();
    waitFor([&] { return peer->bytesToWrite() == 0; });
    QCoreApplication::processEvents();
    if (!check(!client.telemetryFresh() && !client.canSendControlCommands(), "TCP noise never enables control")) return false;
    peer->write(heartbeat(onboard, false)); peer->flush();
    if (!check(waitFor([&] { return client.heartbeatFresh(); }) && !client.telemetryFresh(),
               "companion heartbeat establishes liveness without flight telemetry")) return false;
    peer->write(extension(onboard, 1, {{"fcu_system_id", 7}, {"uav_id", 214}}, 1));
    peer->write(extension(onboard, ZenithProtocol::PROTOCOL_ACK,
        {{"request_kind", 202}, {"transaction_id", 1}, {"status", "RECEIVED"}}, 2));
    peer->write(heartbeat(fcu, true) + position(fcu) + battery(fcu)); peer->flush();
    if (!check(waitFor([&] { return client.canSendControlCommands(); }), "one full-duplex TCP session reaches ready with real FCU data")) return false;
    peer->readAll();
    const auto payload = ManualMoveCommand::build("XYZ_VEL", 1, 2, 3, 0, 123);
    client.sendTcpMessage(ZenithProtocol::UAVCOMMAND, payload, 214);
    if (!check(waitFor([&] { return peer->bytesAvailable() > 0; }), "production client transmits command")) return false;
    zenith::mavlink::Parser parser; zenith::mavlink::Reassembler assembler;
    std::optional<zenith::mavlink::Extension> command;
    for (unsigned char ch : peer->readAll()) if (auto m = parser.consume(ch)) {
        if (auto e = assembler.accept(*m, 1); e && e->kind == ZenithProtocol::UAVCOMMAND) command = e;
    }
    if (!check(command && QJsonDocument::fromJson(bytes(command->payload)).object().value("velocity_ref").toArray().size() == 3,
               "production command decodes with the shared aircraft codec")) return false;
    TelemetryStore store; store.setVehicleName("UAV214");
    CommandDispatcher dispatcher(&store, &client);
    dispatcher.sendManagedTaskRequest("custom_probe", "STATUS", false, "/home/jetson/task_ws/src/user_tasks/probe.py");
    if (!check(waitFor([&] { return peer->bytesAvailable() > 0; }), "production managed-task dispatcher transmits")) return false;
    std::optional<zenith::mavlink::Extension> task;
    for (unsigned char ch : peer->readAll()) if (auto m = parser.consume(ch))
        if (auto e = assembler.accept(*m, 2); e && e->kind == 113) task = e;
    if (!check(task && QJsonDocument::fromJson(bytes(task->payload)).object().value("datas").toArray().size() == 7,
        "production task dispatcher reaches canonical bridge schema with seven task fields")) return false;
    peer->disconnectFromHost();
    if (!check(waitFor([&] { return !client.canSendControlCommands(); }), "disconnect immediately invalidates control readiness")) return false;
    client.stop(); peer->deleteLater();
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (!commands() || !telemetry() || !sourceIsolation() || !maps() || !businessSchemas() || !tcpLoopback()) return 1;
    qInfo("MAVLink2 command, telemetry, mapping, map and full-duplex TCP tests passed");
    return 0;
}
