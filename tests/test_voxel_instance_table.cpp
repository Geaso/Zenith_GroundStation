// 锁定 20260825 栅格显示：方块保留 10% 间隙，颜色使用完整量化高度。
// 直接检查实例缓冲，不创建窗口、连接数传或发送飞行命令。
#include "VoxelInstanceTable.h"
#include "ZenithProtocol.h"

#include <QCoreApplication>
#include <QtMath>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>
#include <limits>
#include <functional>

namespace {

class TestVoxelInstanceTable : public VoxelInstanceTable
{
public:
    using VoxelInstanceTable::getInstanceBuffer;

    // 让 Qt 处理颜色的存储格式；测试只约束地面站的高度到颜色映射。
    static QVector4D packedColor(const QColor &color)
    {
        return calculateTableEntry({}, {1, 1, 1}, {}, color).color;
    }
};

bool require(bool condition, const char *message, int code = -1, float resolution = 0)
{
    if (!condition) {
        qCritical("FAILED: %s (height code=%d, resolution=%.3f)", message, code, resolution);
    }
    return condition;
}

bool close(float actual, float expected)
{
    return qAbs(actual - expected) < 0.00002f;
}

bool close(const QVector3D &actual, const QVector3D &expected)
{
    return close(actual.x(), expected.x()) && close(actual.y(), expected.y())
        && close(actual.z(), expected.z());
}

bool close(const QVector4D &actual, const QVector4D &expected)
{
    return close(actual.x(), expected.x()) && close(actual.y(), expected.y())
        && close(actual.z(), expected.z()) && close(actual.w(), expected.w());
}

QVariantMap gridPayload(float resolution)
{
    // 16x16 中逐一放入 0..255：0 留空，其余覆盖所有有效高度码。
    QByteArray rle;
    for (int code = 0; code <= 255; ++code) {
        rle.append(static_cast<char>(code));
        rle.append(char(1));
    }
    return {
        {QStringLiteral("gm_origin_x"), 2.0f},
        {QStringLiteral("gm_origin_y"), -3.0f},
        {QStringLiteral("gm_resolution"), resolution},
        {QStringLiteral("gm_width"), 16},
        {QStringLiteral("gm_height"), 16},
        {QStringLiteral("gm_slice_z"), 99.0f},
        {QStringLiteral("gm_data"), rle},
    };
}

QVector4D referenceColor(int code)
{
    // 固定五个参考色，而不引用被测类中的常量或配色实现。
    const QVector3D stops[] = {
        {0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}
    };
    const float offset = float(code - 1) * 4.0f / 254.0f;
    const int segment = qMin(int(offset), 3);
    const float fraction = offset - segment;
    const QVector3D rgb = stops[segment] * (1.0f - fraction)
        + stops[segment + 1] * fraction;
    return TestVoxelInstanceTable::packedColor(
        QColor::fromRgbF(rgb.x(), rgb.y(), rgb.z(), 0.9f));
}

bool verifyGrid(float resolution)
{
    TelemetryStore store;
    TestVoxelInstanceTable table;
    table.setStore(&store);

    int count = -1;
    if (!require(table.getInstanceBuffer(&count).isEmpty() && count == 0,
                 "store without a grid produces no instances")) return false;

    store.applyGridMap(gridPayload(resolution));
    const QByteArray buffer = table.getInstanceBuffer(&count);
    using Entry = QQuick3DInstancing::InstanceTableEntry;
    if (!require(count == 255 && table.voxelCount() == 255
                 && buffer.size() == 255 * int(sizeof(Entry)),
                 "zero is omitted and every occupied cell produces one instance",
                 -1, resolution)) return false;

    for (int code = 1; code <= 255; ++code) {
        Entry entry;
        std::memcpy(&entry, buffer.constData() + (code - 1) * sizeof(Entry), sizeof(Entry));

        // 量化码 1/128/255 对应 -0.5/1.25/3.0m；ENU 的北向映射到 -Z。
        const QVector3D position(2.0f + (code % 16 + 0.5f) * resolution,
                                 -0.5f + float(code - 1) * 3.5f / 254.0f,
                                 3.0f - (code / 16 + 0.5f) * resolution);
        if (!require(close(entry.getPosition(), position),
                     "height decoding and ENU cell-center coordinates match the reference",
                     code, resolution)) return false;

        // #Cube 的边长为 100；实例缩放须得到 resolution * 0.9 的实际边长。
        const float scale = resolution * 0.009f;
        if (!require(close(entry.getScale(), {scale, scale, scale}),
                     "cube side remains 90 percent of the map resolution",
                     code, resolution)) return false;
        if (!require(close(entry.color, referenceColor(code)),
                     "blue-cyan-green-yellow-red spans the complete encoded height range",
                     code, resolution)) return false;

        // 两个高于 1.5m 的中间高度仍须保留不同颜色，不能都被截成顶端红色。
        if (code == 160 || code == 224) {
            if (!require(!close(entry.color, referenceColor(255)),
                         "SUPER obstacles above 1.5m do not all saturate to red",
                         code, resolution)) return false;
        }
    }

    table.setStore(nullptr);
    count = -1;
    return require(table.getInstanceBuffer(&count).isEmpty() && count == 0
                   && table.voxelCount() == 0,
                   "detaching the store clears previous instances", -1, resolution);
}

QByteArray columnRun(quint32 mask, quint8 run)
{
    QByteArray bytes;
    for (int shift = 0; shift < 32; shift += 8) bytes.append(char(mask >> shift));
    bytes.append(char(run));
    return bytes;
}

QVariantMap voxelPayload(quint32 frame, const QByteArray &data, int width = 2, int height = 1,
                         int part = 0, int parts = 1)
{
    return {{"vm_origin_x", 1.95f}, {"vm_origin_y", -3.0f}, {"vm_xy_resolution", 0.15f},
            {"vm_width", width}, {"vm_height", height}, {"vm_z_min", -0.6f},
            {"vm_z_resolution", 0.15f}, {"vm_layers", 24}, {"vm_frame_id", frame},
            {"vm_part_index", part}, {"vm_part_count", parts}, {"vm_data", data}, {"vm_encoding", 1}};
}

QVector4D referenceHeightColor(float z)
{
    const QVector3D stops[] = {{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}};
    const float offset = qBound(0.0f, (z + 0.5f) / 3.5f, 1.0f) * 4;
    const int segment = qMin(int(offset), 3);
    const float fraction = offset - segment;
    const auto rgb = stops[segment] * (1 - fraction) + stops[segment + 1] * fraction;
    return TestVoxelInstanceTable::packedColor(QColor::fromRgbF(rgb.x(), rgb.y(), rgb.z(), 0.9f));
}

bool verifyVoxelGeometry()
{
    TelemetryStore store;
    TestVoxelInstanceTable table;
    table.setStore(&store);
    const quint32 mask = (1u << 1) | (1u << 7) | (1u << 17);
    if (!require(store.applyVoxelMap(voxelPayload(10, columnRun(mask, 1) + columnRun(1u << 23, 1)),
                                    214, 100), "multi-Z snapshot accepted")) return false;
    int count = -1;
    const auto bytes = table.getInstanceBuffer(&count);
    if (!require(count == 4, "only four observed bits create four voxels, no filled columns")) return false;
    const int layers[] = {1, 7, 17, 23};
    using Entry = QQuick3DInstancing::InstanceTableEntry;
    for (int i = 0; i < 4; ++i) {
        Entry entry;
        std::memcpy(&entry, bytes.constData() + i * sizeof(Entry), sizeof(entry));
        const float z = -0.6f + (layers[i] + 0.5f) * 0.15f;
        if (!require(close(entry.getPosition(), {i == 3 ? 2.175f : 2.025f, z, 2.925f}),
                     "actual layer center and ENU axes, including low obstacles")) return false;
        if (!require(close(entry.getScale(), {0.00135f, 0.00135f, 0.00135f}),
                     "multi-Z voxels keep 90 percent fill")) return false;
        if (!require(close(entry.color, referenceHeightColor(z)), "physical height uses full-range palette")) return false;
    }
    auto high = voxelPayload(11, columnRun(1, 2));
    high["vm_z_min"] = 4.0f;
    if (!require(store.applyVoxelMap(high, 214, 101), "height above palette range remains valid")) return false;
    const auto highBytes = table.getInstanceBuffer(&count);
    Entry highEntry;
    std::memcpy(&highEntry, highBytes.constData(), sizeof(highEntry));
    if (!require(close(highEntry.getPosition().y(), 4.075f), "palette clamp does not clamp geometry")) return false;
    if (!require(store.applyVoxelMap(voxelPayload(12, columnRun(0, 2)), 214, 102),
                 "complete empty occupancy accepted")) return false;
    return require(table.getInstanceBuffer(&count).isEmpty() && count == 0 && store.voxelMapValid(),
                   "all-zero complete map clears obstacles without legacy fallback");
}

bool verifyReassembly()
{
    TelemetryStore store;
    int updates = 0;
    QObject::connect(&store, &TelemetryStore::gridMapChanged, [&] { ++updates; });
    if (!require(store.applyVoxelMap(voxelPayload(100, columnRun(1, 2)), 214, 100), "initial map")) return false;
    const auto part = [](quint32 frame, int index, quint32 mask) {
        return voxelPayload(frame, columnRun(mask, 1), 3, 1, index, 3);
    };
    if (!require(store.applyVoxelMap(part(101, 2, 4), 214, 200), "last part may arrive first")) return false;
    if (!require(store.applyVoxelMap(part(101, 2, 4), 214, 201), "identical duplicate harmless")) return false;
    store.applyGridMap(gridPayload(0.15f), 214, 202);
    if (!require(updates == 1 && store.voxelMap().frameId == 100, "partial and legacy cannot replace complete 3D map")) return false;
    if (!require(store.applyVoxelMap(part(101, 0, 1), 214, 203)
                 && store.applyVoxelMap(part(101, 1, 2), 214, 204), "out-of-order map completes")) return false;
    if (!require(updates == 2 && store.voxelMap().columns == QVector<quint32>({1, 2, 4}),
                 "single atomic update preserves part-index order")) return false;
    if (!require(!store.applyVoxelMap(part(100, 0, 1), 214, 205), "stale frame rejected")) return false;
    if (!require(store.applyVoxelMap(part(102, 0, 8), 214, 206)
                 && store.applyVoxelMap(voxelPayload(103, columnRun(16, 2)), 214, 207), "new frame preempts incomplete frame")) return false;
    if (!require(!store.applyVoxelMap(part(102, 1, 8), 214, 208) && updates == 3,
                 "late preempted part cannot mix into newer frame")) return false;
    if (!require(store.applyVoxelMap(part(104, 0, 1), 214, 209)
                 && !store.applyVoxelMap(part(104, 0, 2), 214, 210)
                 && !store.applyVoxelMap(part(104, 1, 4), 214, 211), "conflicting duplicate invalidates that frame")) return false;
    if (!require(store.applyVoxelMap(part(105, 0, 1), 214, 212), "begin header consistency test")) return false;
    auto changed = part(105, 1, 2);
    changed["vm_origin_x"] = 10.0f;
    if (!require(!store.applyVoxelMap(changed, 214, 213) && updates == 3, "inconsistent metadata never commits")) return false;
    if (!require(store.applyVoxelMap(part(106, 0, 1), 214, 214), "vehicle A starts")) return false;
    if (!require(store.applyVoxelMap(voxelPayload(106, columnRun(8, 1), 2, 1, 1, 2), 75, 215), "vehicle B has separate watermark")) return false;
    if (!require(!store.applyVoxelMap(part(106, 1, 2), 214, 216), "vehicle A cannot supply vehicle B parts")) return false;
    if (!require(store.applyVoxelMap(voxelPayload(106, columnRun(16, 1), 2, 1, 0, 2), 75, 217)
                 && store.voxelMap().vehicleId == 75 && store.voxelMap().columns == QVector<quint32>({16, 8}),
                 "complete map belongs to exactly one vehicle")) return false;

    TelemetryStore wrap;
    for (quint32 frame : {0xfffffffeu, 0xffffffffu, 0u, 1u})
        if (!require(wrap.applyVoxelMap(voxelPayload(frame, columnRun(1, 2)), 214, 100), "uint32 frame wrap is ordered")) return false;
    if (!require(!wrap.applyVoxelMap(voxelPayload(0xffffffffu, columnRun(2, 2)), 214, 101), "pre-wrap frame stays old")) return false;

    TelemetryStore timeout;
    const qint64 deadline = ZenithProtocol::VoxelMap::kAssemblyTimeoutMs;
    timeout.applyVoxelMap(voxelPayload(50, columnRun(1, 2)), 214, 0);
    timeout.applyVoxelMap(part(51, 0, 2), 214, 1);
    if (!require(timeout.applyVoxelMap(voxelPayload(1, columnRun(4, 2)), 214, deadline + 2)
                 && timeout.voxelMap().frameId == 1, "silent timeout permits restarted counter without old parts")) return false;
    timeout.applyGridMap(gridPayload(0.15f), 214, deadline + 3);
    if (!require(timeout.voxelMapValid(), "fresh 3D wins over old GRIDMAP")) return false;
    timeout.applyGridMap(gridPayload(0.15f), 214, 2 * deadline + 3);
    if (!require(!timeout.voxelMapValid() && timeout.gridMapCells().size() == 256,
                 "quiet 3D stream permits explicit legacy fallback")) return false;
    timeout.applyVoxelMap(voxelPayload(10, columnRun(8, 2)), 214, 2 * deadline + 4);
    timeout.noteFrameReceived(214);
    timeout.setTransportHealth(false, false, "link lost");
    return require(timeout.voxelMapValid()
                   && timeout.applyVoxelMap(voxelPayload(0, columnRun(16, 2)), 214, 2 * deadline + 5),
                   "link loss preserves complete image but resets counter and staging");
}

bool verifyInvalidMaps()
{
    const auto good = voxelPayload(11, columnRun(1, 2));
    const QVector<std::function<void(QVariantMap &)>> mutations = {
        [](auto &p) { p.remove("vm_frame_id"); },
        [](auto &p) { p["vm_width"] = 0; },
        [](auto &p) { p["vm_width"] = 65536; },
        [](auto &p) { p["vm_width"] = 257; p["vm_height"] = 256; },
        [](auto &p) { p["vm_width"] = QString("2"); },
        [](auto &p) { p["vm_layers"] = 0; },
        [](auto &p) { p["vm_layers"] = 33; },
        [](auto &p) { p["vm_part_count"] = 0; },
        [](auto &p) { p["vm_part_count"] = 65; },
        [](auto &p) { p["vm_part_index"] = 1; },
        [](auto &p) { p["vm_part_index"] = -1; },
        [](auto &p) { p["vm_encoding"] = 0; },
        [](auto &p) { p["vm_encoding"] = 2; },
        [](auto &p) { p["vm_frame_id"] = QVariant::fromValue(quint64(0x100000000ull)); },
        [](auto &p) { p["vm_xy_resolution"] = 0.0; },
        [](auto &p) { p["vm_z_resolution"] = -0.15; },
        [](auto &p) { p["vm_origin_x"] = std::numeric_limits<double>::quiet_NaN(); },
        [](auto &p) { p["vm_z_min"] = std::numeric_limits<double>::infinity(); },
        [](auto &p) { p["vm_xy_resolution"] = std::numeric_limits<float>::max(); },
        [](auto &p) { p["vm_data"] = columnRun(1u << 24, 2); },
        [](auto &p) { p["vm_data"] = columnRun(1, 0); },
        [](auto &p) { p["vm_data"] = columnRun(1, 1); },
        [](auto &p) { p["vm_data"] = columnRun(1, 3); },
        [](auto &p) { p["vm_data"] = QByteArray(4, '\0'); },
        [](auto &p) { p["vm_data"] = QByteArray(6005, '\0'); },
        [](auto &p) { p["vm_data"] = QString("binary is not a JSON string"); }
    };
    for (int i = 0; i < mutations.size(); ++i) {
        TelemetryStore store;
        store.applyVoxelMap(voxelPayload(10, columnRun(2, 2)), 214, 0);
        auto invalid = good;
        mutations[i](invalid);
        if (!require(!store.applyVoxelMap(invalid, 214, 1)
                     && store.voxelMap().frameId == 10
                     && store.voxelMap().columns == QVector<quint32>({2, 2}),
                     "invalid map preserves complete snapshot", i)) return false;
    }
    TelemetryStore partial;
    partial.applyVoxelMap(voxelPayload(1, columnRun(1, 2)), 214, 0);
    partial.applyVoxelMap(voxelPayload(2, columnRun(2, 1), 3, 1, 0, 2), 214, 1);
    if (!require(!partial.applyVoxelMap(voxelPayload(2, columnRun(4, 1), 3, 1, 1, 2), 214, 2)
                 && partial.voxelMap().frameId == 1, "aggregate run count must exactly match width times height")) return false;
    return true;
}

void appendBE(QByteArray &out, quint64 value, int bytes)
{
    for (int i = bytes - 1; i >= 0; --i) out.append(char(value >> (i * 8)));
}

void appendFloat(QByteArray &out, float value)
{
    quint32 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    out.append(char(0xca));
    appendBE(out, bits, 4);
}

QByteArray voxelMsgPack(const QVariantMap &p)
{
    QByteArray out(1, char(0x8d));
    const char *names[] = {"vm_origin_x", "vm_origin_y", "vm_xy_resolution", "vm_width", "vm_height",
                          "vm_z_min", "vm_z_resolution", "vm_layers", "vm_frame_id",
                          "vm_part_index", "vm_part_count", "vm_data", "vm_encoding"};
    for (int key = 80; key <= 92; ++key) {
        out.append(char(key));
        const auto value = p.value(names[key - 80]);
        if (key == 80 || key == 81 || key == 82 || key == 85 || key == 86) appendFloat(out, value.toFloat());
        else if (key == 91) {
            const auto data = value.toByteArray();
            out.append(char(0xc5));
            appendBE(out, data.size(), 2);
            out.append(data);
        } else {
            out.append(char(0xce));
            appendBE(out, value.toUInt(), 4);
        }
    }
    return out;
}

QByteArray wireFrame(const QByteArray &payload, int id = 13, int vehicle = 214)
{
    QByteArray out = QByteArray::fromHex("616d");
    for (int i = 0; i < 4; ++i) out.append(char(quint32(payload.size()) >> (8 * i)));
    out.append(char(id));
    out.append(char(vehicle));
    out.append(payload);
    quint16 crc = 0;
    for (quint8 byte : out) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) crc = (crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1;
    }
    out.append(char(crc));
    out.append(char(crc >> 8));
    return out;
}

bool verifyWireCodec()
{
    const auto payload = voxelPayload(0xffffffffu, QByteArray::fromHex("8100800002"));
    const auto packed = voxelMsgPack(payload);
    const auto frame = wireFrame(packed);
    const auto decoded = ZenithProtocol::decodeFrame(frame);
    TelemetryStore store;
    if (!require(decoded.valid && decoded.msgId == 13 && decoded.robotId == 214
                 && decoded.payload.value("vm_frame_id").toUInt() == 0xffffffffu
                 && store.applyVoxelMap(decoded.payload, decoded.robotId, 0)
                 && store.voxelMap().columns == QVector<quint32>({0x800081u, 0x800081u}),
                 "real C++ frame and MsgPack decoder matches little-endian bitmask contract")) return false;
    auto map16 = packed;
    map16.replace(0, 1, QByteArray::fromHex("de000d"));
    if (!require(ZenithProtocol::decodeFrame(wireFrame(map16)).valid, "map16 header supported")) return false;
    for (qsizetype n = 0; n < frame.size(); ++n)
        if (!require(!ZenithProtocol::decodeFrame(frame.left(n)).valid, "split frame waits for all bytes")) return false;
    auto badCrc = frame;
    badCrc[badCrc.size() - 1] ^= 1;
    if (!require(!ZenithProtocol::decodeFrame(badCrc).valid, "bad CRC rejected")) return false;
    auto missing = packed;
    missing.chop(2);
    if (!require(!ZenithProtocol::decodeFrame(wireFrame(missing)).valid, "CRC-valid truncated MsgPack rejected")) return false;
    auto duplicate = packed;
    duplicate[7] = char(80); // key 81 becomes a duplicate key 80
    if (!require(!ZenithProtocol::decodeFrame(wireFrame(duplicate)).valid, "duplicate schema keys rejected")) return false;
    auto trailing = packed + QByteArray(1, char(0));
    if (!require(!ZenithProtocol::decodeFrame(wireFrame(trailing)).valid, "trailing MsgPack data rejected")) return false;
    const auto legacy = wireFrame("{\"count\":7}", ZenithProtocol::HEARTBEAT);
    if (!require(ZenithProtocol::decodeFrame(legacy).payload.value("count").toInt() == 7,
                 "existing JSON messages remain compatible")) return false;
    const auto legacyGrid = ZenithProtocol::decodeFrame(wireFrame(
        R"({"gm_origin_x":2,"gm_origin_y":-3,"gm_resolution":0.15,"gm_width":2,"gm_height":2,"gm_data":"A\u0004"})",
        ZenithProtocol::GRIDMAP));
    TelemetryStore legacyStore;
    legacyStore.applyGridMap(legacyGrid.payload, 214, 0);
    if (!require(legacyGrid.valid && legacyStore.gridMapValid()
                 && legacyStore.gridMapCells() == QVector<uint8_t>({65, 65, 65, 65}),
                 "legacy JSON grid accepts integral coordinates and dimensions")) return false;
    auto legacyNumbers = legacyGrid.payload;
    legacyNumbers["gm_width"] = 2.0;
    legacyNumbers["gm_data"] = QByteArray::fromHex("4204");
    legacyStore.applyGridMap(legacyNumbers, 214, 1);
    if (!require(legacyStore.gridMapCells() == QVector<uint8_t>({66, 66, 66, 66}),
                 "legacy whole floating dimensions remain compatible")) return false;
    legacyNumbers["gm_width"] = 2.5;
    legacyStore.applyGridMap(legacyNumbers, 214, 2);
    if (!require(legacyStore.gridMapWidth() == 2, "fractional grid dimensions never truncate")) return false;

    int frames = 0;
    QByteArray stream;
    for (char byte : frame) {
        stream.append(byte);
        ZenithProtocol::consumeFrames(stream, [&](const auto &) { ++frames; });
    }
    if (!require(frames == 1 && stream.isEmpty(), "serial byte-by-byte receive uses production stream parser")) return false;
    stream = QByteArray("noise") + badCrc;
    constexpr int burstFrames = 2000;
    for (int i = 0; i < burstFrames; ++i) stream += i % 2 ? legacy : frame;
    stream += frame.left(9);
    frames = 0;
    if (!require(stream.size() > 65536, "fixture exercises burst beyond former buffer limit")) return false;
    ZenithProtocol::consumeFrames(stream, [&](const auto &) { ++frames; });
    if (!require(frames == burstFrames && stream == frame.left(9), "large valid burst drains and preserves final partial frame")) return false;
    stream += frame.mid(9);
    ZenithProtocol::consumeFrames(stream, [&](const auto &) { ++frames; });
    return require(frames == burstFrames + 1 && stream.isEmpty(), "partial frame completes after burst");
}

bool verifyLimits()
{
    TelemetryStore store;
    QByteArray maxPart;
    for (int i = 0; i < 1200; ++i) maxPart += columnRun(i & 1, 1);
    const auto maxWire = ZenithProtocol::decodeFrame(wireFrame(voxelMsgPack(voxelPayload(1, maxPart, 1200))));
    if (!require(maxWire.valid && store.applyVoxelMap(maxWire.payload, 214, 0), "6000-byte part accepted")) return false;
    maxPart += columnRun(1, 1);
    if (!require(!ZenithProtocol::decodeFrame(wireFrame(voxelMsgPack(voxelPayload(2, maxPart, 1201)))).valid,
                 "6005-byte part rejected before reassembly")) return false;
    QByteArray maxColumns;
    int remaining = 65536;
    while (remaining) {
        const int run = qMin(remaining, 255);
        maxColumns += columnRun(0, quint8(run));
        remaining -= run;
    }
    if (!require(store.applyVoxelMap(voxelPayload(3, maxColumns, 256, 256), 214, 1)
                 && store.voxelMap().columns.size() == 65536, "maximum total column count accepted")) return false;
    for (int i = 63; i >= 0; --i)
        if (!require(store.applyVoxelMap(voxelPayload(4, columnRun(1, 1), 64, 1, i, 64), 214, 2 + (63 - i) * 100),
                     "64 parts accept reverse order inside 10-second budget")) return false;
    if (!require(store.voxelMap().frameId == 4 && store.voxelMap().columns.size() == 64, "64-part snapshot committed")) return false;
    auto bit31 = voxelPayload(5, columnRun(0x80000000u, 2));
    bit31["vm_layers"] = 32;
    return require(store.applyVoxelMap(bit31, 214, 6400), "32 layers avoid undefined full-width shift");
}

bool verifyWireFixture(const QString &path, int expectedVoxels)
{
    QFile file(path);
    if (!require(file.open(QIODevice::ReadOnly), "open offline production wire fixture")) return false;
    const QByteArray capture = file.readAll();
    QByteArray bytes = capture;
    const auto originalSize = bytes.size();
    TelemetryStore store;
    TestVoxelInstanceTable table;
    table.setStore(&store);
    int snapshots = 0, voxelParts = 0;
    bool accepted = true;
    QObject::connect(&store, &TelemetryStore::gridMapChanged, [&] { ++snapshots; });
    const int frames = ZenithProtocol::consumeFrames(bytes, [&](const auto &frame) {
        if (frame.msgId == ZenithProtocol::VOXELMAP) {
            ++voxelParts;
            accepted = store.applyVoxelMap(frame.payload, frame.robotId) && accepted;
        }
    });
    int voxels = 0;
    const auto instances = table.getInstanceBuffer(&voxels);
    if (!require(accepted && bytes.isEmpty() && store.voxelMapValid() && snapshots > 0,
                 "fixture fully decodes and produces complete 3D snapshot")) return false;
    if (expectedVoxels >= 0 && !require(voxels == expectedVoxels, "fixture matches expected occupied voxel count")) return false;
    const auto &map = store.voxelMap();
    int occupiedColumns = 0, multipleLayerColumns = 0;
    for (quint32 mask : map.columns) {
        occupiedColumns += mask != 0;
        multipleLayerColumns += qPopulationCount(mask) > 1;
    }

    // Replay the exact captured bytes through serial-sized fragments, without
    // an encoder, transport object, ROS master, serial port or GUI window.
    TelemetryStore splitStore;
    TestVoxelInstanceTable splitTable;
    splitTable.setStore(&splitStore);
    QByteArray pending;
    int splitSnapshots = 0, splitFrames = 0;
    bool splitAccepted = true;
    QObject::connect(&splitStore, &TelemetryStore::gridMapChanged, [&] { ++splitSnapshots; });
    for (qsizetype offset = 0; offset < capture.size(); offset += 37) {
        pending += capture.mid(offset, 37);
        splitFrames += ZenithProtocol::consumeFrames(pending, [&](const auto &frame) {
            if (frame.msgId == ZenithProtocol::VOXELMAP)
                splitAccepted = splitStore.applyVoxelMap(frame.payload, frame.robotId) && splitAccepted;
        });
        if (frames == 1 && offset + 37 < capture.size()
            && !require(splitSnapshots == 0, "captured snapshot is not visible before final CRC bytes")) return false;
    }
    int splitVoxels = 0;
    const auto splitInstances = splitTable.getInstanceBuffer(&splitVoxels);
    if (!require(splitAccepted && pending.isEmpty() && splitFrames == frames && splitSnapshots == snapshots
                 && splitStore.voxelMap().columns == map.columns && splitVoxels == voxels
                 && splitInstances == instances, "captured serial fragments preserve masks and every rendered voxel")) return false;
    const QJsonObject result{{"wire_bytes", double(originalSize)}, {"frames", frames}, {"voxel_parts", voxelParts},
                             {"snapshots", snapshots}, {"remaining_bytes", double(bytes.size())},
                             {"vehicle", map.vehicleId}, {"frame_id", double(map.frameId)},
                             {"width", map.width}, {"height", map.height}, {"layers", map.layers},
                             {"xy_resolution", map.resolution}, {"z_resolution", map.zResolution},
                             {"z_min", map.zMin}, {"voxels", voxels},
                             {"occupied_columns", occupiedColumns}, {"multiple_layer_columns", multipleLayerColumns}};
    qInfo().noquote() << QJsonDocument(result).toJson(QJsonDocument::Compact);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.size() > 1) {
        if (args.size() < 3 || args.size() > 4 || args[1] != "--wire-fixture") {
            qCritical("usage: VoxelInstanceTableTests --wire-fixture PATH [EXPECTED_VOXELS]");
            return 2;
        }
        bool ok = true;
        const int expected = args.size() == 4 ? args[3].toInt(&ok) : -1;
        return ok && verifyWireFixture(args[2], expected) ? 0 : 1;
    }

    {
        TestVoxelInstanceTable table;
        int count = -1;
        if (!require(table.getInstanceBuffer(&count).isEmpty() && count == 0
                     && table.voxelCount() == 0,
                     "unbound table produces no instances")) return 1;
    }

    if (!verifyGrid(0.1f) || !verifyGrid(0.25f)) return 1;
    if (!verifyVoxelGeometry() || !verifyReassembly() || !verifyInvalidMaps()
        || !verifyWireCodec() || !verifyLimits()) return 1;

    qInfo("Voxel instance table reference rendering tests passed");
    return 0;
}
