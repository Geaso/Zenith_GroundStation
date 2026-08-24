#pragma once

#include <QQuick3DInstancing>
#include <QColor>
#include <QVector3D>
#include <QVector>
#include "TelemetryStore.h"

class VoxelInstanceTable : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TelemetryStore* store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(int voxelCount READ voxelCount NOTIFY voxelCountChanged)
    // 配色高度带：色带在 [colorMinZ, colorMaxZ] 之间铺满，超出则钳到两端。
    // 这个区间是**显示**用的，和下面的量化区间是两回事，不要混。
    Q_PROPERTY(float colorMinZ READ colorMinZ WRITE setColorMinZ NOTIFY colorBandChanged)
    Q_PROPERTY(float colorMaxZ READ colorMaxZ WRITE setColorMaxZ NOTIFY colorBandChanged)

public:
    // 体素高度**量化**区间，必须与机载 uav_basic_topic.cpp gridMapCb() 的
    // z_min/z_max 一致，用于把 1 字节高度码还原成世界系 z。不是显示参数。
    static constexpr float kHeightMin = -0.5f;
    static constexpr float kHeightMax = 3.0f;

    explicit VoxelInstanceTable(QQuick3DObject *parent = nullptr) : QQuick3DInstancing(parent) {}

    TelemetryStore* store() const { return m_store; }
    int voxelCount() const { return m_count; }
    float colorMinZ() const { return m_colorMinZ; }
    float colorMaxZ() const { return m_colorMaxZ; }

    void setColorMinZ(float v) {
        if (qFuzzyCompare(m_colorMinZ, v)) return;
        m_colorMinZ = v;
        emit colorBandChanged();
        rebuild();
    }
    void setColorMaxZ(float v) {
        if (qFuzzyCompare(m_colorMaxZ, v)) return;
        m_colorMaxZ = v;
        emit colorBandChanged();
        rebuild();
    }

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
    void colorBandChanged();

public slots:
    void rebuild() { markDirty(); }

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override {
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
        //
        // 系数取 1.02 而不是原来的 0.9：留 10% 缝隙会让连续的墙渲染成一串独立小方块，
        // 观感是"撒了一地confetti"而不是墙体。略微过盈可以让相邻格子接成面，
        // 也顺带盖掉浮点误差造成的发丝缝。
        const float cubeScale = res * 0.01f * 1.02f;

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

                // 高度 → 颜色：蓝 → 青 → 绿 → 黄 → 红
                //
                // 归一化用的是**显示带** [colorMinZ, colorMaxZ]，不是量化区间。
                // 之前直接拿量化出来的 t 上色，而量化区间是 [-0.5, 3.0]、实际内容
                // 只到 1.5m，结果整张图挤在 t∈[0.14,0.57] 也就是蓝→绿这 40% 色带里，
                // 黄和红永远不出现，高度差几乎看不出来。
                const float span = (m_colorMaxZ - m_colorMinZ) > 1e-6f
                                       ? (m_colorMaxZ - m_colorMinZ) : 1.0f;
                float ct = (worldZ - m_colorMinZ) / span;
                ct = ct < 0.0f ? 0.0f : (ct > 1.0f ? 1.0f : ct);

                float cr, cg, cb;
                if (ct < 0.25f)      { float s = ct/0.25f;          cr=0;   cg=s;   cb=1;   }
                else if (ct < 0.5f)  { float s=(ct-0.25f)/0.25f;    cr=0;   cg=1;   cb=1-s; }
                else if (ct < 0.75f) { float s=(ct-0.5f)/0.25f;     cr=s;   cg=1;   cb=0;   }
                else                 { float s=(ct-0.75f)/0.25f;    cr=1;   cg=1-s; cb=0;   }

                // ENU→Qt3D: x=East, y=Up(Z), z=-North(-Y)
                auto entry = calculateTableEntry(
                    QVector3D(worldX, worldZ, -worldY),
                    QVector3D(cubeScale, cubeScale, cubeScale),
                    QVector3D(0, 0, 0),
                    QColor::fromRgbF(cr, cg, cb, 0.9f),
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
    // getInstanceBuffer() 跑在渲染同步阶段，不能在这里同步改 QML 属性，
    // 否则 HUD 的 binding 会在错误的线程/时机被求值。排队到事件循环里发。
    void setVoxelCount(int n) {
        if (m_count == n) return;
        m_count = n;
        QMetaObject::invokeMethod(this, [this] { emit voxelCountChanged(); },
                                  Qt::QueuedConnection);
    }

    TelemetryStore *m_store = nullptr;
    // 默认铺满 0~1.5m，即机载 max_height 限定的飞行包线。
    // 改机载限高时这里要跟着改，否则色带又会用不满。
    float m_colorMinZ = 0.0f;
    float m_colorMaxZ = 1.5f;
    int m_count = 0;
};
