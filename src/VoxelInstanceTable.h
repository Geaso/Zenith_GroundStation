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
    Q_PROPERTY(int voxelCount READ voxelCount NOTIFY storeChanged)

public:
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

public slots:
    void rebuild() { markDirty(); }

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override {
        if (!m_store || !m_store->gridMapValid()) {
            m_count = 0;
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
        const float cubeScale = res * 0.01f * 0.9f;

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
                float worldZ = t * 3.0f;

                // height → color: blue → cyan → green → yellow → red
                float cr, cg, cb;
                if (t < 0.25f)      { float s = t/0.25f;          cr=0;   cg=s;   cb=1;   }
                else if (t < 0.5f)  { float s=(t-0.25f)/0.25f;    cr=0;   cg=1;   cb=1-s; }
                else if (t < 0.75f) { float s=(t-0.5f)/0.25f;     cr=s;   cg=1;   cb=0;   }
                else                { float s=(t-0.75f)/0.25f;     cr=1;   cg=1-s; cb=0;   }

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

        m_count = written;
        *instanceCount = written;
        buf.resize(written * int(sizeof(InstanceTableEntry)));
        return buf;
    }

private:
    TelemetryStore *m_store = nullptr;
    int m_count = 0;
};
