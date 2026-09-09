#pragma once

#include <QQuick3DInstancing>
#include <QColor>
#include <QVector3D>
#include <QVector>
#include <cmath>
#include "TelemetryStore.h"

class VoxelInstanceTable : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TelemetryStore* store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(int voxelCount READ voxelCount NOTIFY voxelCountChanged)

public:
    // 体素高度量化区间，必须与机载 uav_basic_topic.cpp gridMapCb() 的 z_min/z_max 一致
    static constexpr float kHeightMin = -0.5f;
    static constexpr float kHeightMax = 3.0f;
    // 图二验收基线：体素之间保留 10% 缝隙，避免相邻格黏成整块平面。
    static constexpr float kCubeFillRatio = 0.9f;

    // 按用户新选择改为蓝→青→绿→黄，取代旧红端；2.125m 起固定黄色。
    // 黄色仅表示高度，不表示安全或可通行；只钳颜色，不改变真实几何。
    static QColor heightColor(float worldZ) {
        // 非有限颜色输入回退蓝色，几何有效性仍由原数据链路判定。
        if (!std::isfinite(worldZ)) worldZ = kHeightMin;
        const float t = qBound(0.0f, (worldZ - kHeightMin) / (kHeightMax - kHeightMin), 0.75f);
        if (t < 0.25f)
            return QColor::fromRgbF(0.0f, t / 0.25f, 1.0f, 0.9f);
        if (t < 0.5f)
            return QColor::fromRgbF(0.0f, 1.0f, 1.0f - (t - 0.25f) / 0.25f, 0.9f);
        return QColor::fromRgbF((t - 0.5f) / 0.25f, 1.0f, 0.0f, 0.9f);
    }

    explicit VoxelInstanceTable(QQuick3DObject *parent = nullptr) : QQuick3DInstancing(parent) {}

    TelemetryStore* store() const { return m_store; }
    int voxelCount() const { return m_count; }

    void setStore(TelemetryStore *s) {
        if (m_store == s) return;
        if (m_store) disconnect(m_store, nullptr, this, nullptr);
        m_store = s;
        if (m_store) connect(m_store, &TelemetryStore::gridMapChanged, this, &VoxelInstanceTable::rebuild);
        emit storeChanged();
        rebuild();
    }

signals:
    void storeChanged();
    void voxelCountChanged();

public slots:
    void rebuild() { markDirty(); }

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override {
        if (m_store && m_store->voxelMapValid())
            return getVoxelMapBuffer(instanceCount);
        if (!m_store || !m_store->gridMapValid()) {
            setVoxelCount(0);
            *instanceCount = 0;
            return {};
        }

        const int w = m_store->gridMapWidth();
        const int h = m_store->gridMapHeight();
        const float res = m_store->gridMapResolution();
        const float ox = m_store->gridMapOriginX();
        const float oy = m_store->gridMapOriginY();
        const auto &cells = m_store->gridMapCells();

        // #Cube is 100x100x100 units. Scale factor to get 1-meter cube = 0.01.
        // Voxel side = resolution meters → scale = res * 0.01
        const float cubeScale = res * 0.01f * kCubeFillRatio;

        QByteArray buf;
        int count = 0;

        // Pre-count for reserve
        for (int i = 0; i < cells.size(); ++i)
            if (cells[i] != 0) ++count;

        buf.resize(count * int(sizeof(InstanceTableEntry)));
        char *dst = buf.data();
        int written = 0;

        for (int cy = 0; cy < h; ++cy) {
            for (int cx = 0; cx < w; ++cx) {
                int idx = cy * w + cx;
                if (idx >= cells.size() || cells[idx] == 0) continue;

                float t = (cells[idx] - 1) / 254.0f;
                float worldX = ox + (cx + 0.5f) * res;
                float worldY = oy + (cy + 0.5f) * res;
                // 必须和机载 gridMapCb() 的量化区间一致：
                //   norm = (pz - kHeightMin) / (kHeightMax - kHeightMin)
                // 之前这里写的是 t * 3.0f，漏掉了 -0.5 的下界，导致低处体素整体上浮
                // 0.5m、整根柱子被压到 3.0/3.5 高。
                float worldZ = kHeightMin + t * (kHeightMax - kHeightMin);

                // ENU→Qt3D: x=East, y=Up(Z), z=-North(-Y)
                auto entry = calculateTableEntry(
                    QVector3D(worldX, worldZ, -worldY),
                    QVector3D(cubeScale, cubeScale, cubeScale),
                    QVector3D(0, 0, 0),
                    heightColor(worldZ),
                    {}
                );
                memcpy(dst, &entry, sizeof(InstanceTableEntry));
                dst += sizeof(InstanceTableEntry);
                ++written;
            }
        }

        setVoxelCount(written);
        *instanceCount = written;
        buf.resize(written * int(sizeof(InstanceTableEntry)));
        return buf;
    }

private:
    QByteArray getVoxelMapBuffer(int *instanceCount) {
        const auto &map = m_store->voxelMap();
        int count = 0;
        for (quint32 mask : map.columns)
            for (; mask; mask &= mask - 1) ++count;
        QByteArray buffer(count * int(sizeof(InstanceTableEntry)), Qt::Uninitialized);
        char *dst = buffer.data();
        const QVector3D scale(map.resolution * 0.01f * kCubeFillRatio,
                              map.zResolution * 0.01f * kCubeFillRatio,
                              map.resolution * 0.01f * kCubeFillRatio);
        for (qsizetype index = 0; index < map.columns.size(); ++index) {
            const quint32 mask = map.columns[index];
            if (!mask) continue;
            const float x = map.originX + (float(index % map.width) + 0.5f) * map.resolution;
            const float y = map.originY + (float(index / map.width) + 0.5f) * map.resolution;
            for (int layer = 0; layer < map.layers; ++layer) {
                if (!(mask & (quint32(1) << layer))) continue;
                const float z = map.zMin + (float(layer) + 0.5f) * map.zResolution;
                const auto entry = calculateTableEntry(
                    QVector3D(x, z, -y), scale, QVector3D(), heightColor(z), {});
                memcpy(dst, &entry, sizeof(entry));
                dst += sizeof(entry);
            }
        }
        setVoxelCount(count);
        *instanceCount = count;
        return buffer;
    }

    // getInstanceBuffer() 跑在渲染同步阶段，不能在这里同步改 QML 属性，
    // 否则 HUD 的 binding 会在错误的线程/时机被求值。排队到事件循环里发。
    void setVoxelCount(int n) {
        if (m_count == n) return;
        m_count = n;
        QMetaObject::invokeMethod(this, [this] { emit voxelCountChanged(); },
                                  Qt::QueuedConnection);
    }

    TelemetryStore *m_store = nullptr;
    int m_count = 0;
};
