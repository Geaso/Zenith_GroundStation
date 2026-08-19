#pragma once

#include <QQuick3DInstancing>
#include <QVector3D>
#include <QColor>
#include "TelemetryStore.h"

class TrailInstanceTable : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TelemetryStore* store READ store WRITE setStore NOTIFY storeChanged)

public:
    explicit TrailInstanceTable(QQuick3DObject *parent = nullptr) : QQuick3DInstancing(parent) {}

    TelemetryStore* store() const { return m_store; }
    void setStore(TelemetryStore *s) {
        if (m_store == s) return;
        if (m_store) disconnect(m_store, nullptr, this, nullptr);
        m_store = s;
        if (m_store) connect(m_store, &TelemetryStore::telemetryChanged, this, &TrailInstanceTable::rebuild);
        emit storeChanged();
    }

signals:
    void storeChanged();

public slots:
    void rebuild() { markDirty(); }

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override {
        if (!m_store || m_store->trailSize() < 1) {
            *instanceCount = 0;
            return {};
        }

        const auto &trail = m_store->trail3D();
        const int n = trail.size();
        const int entrySz = static_cast<int>(sizeof(InstanceTableEntry));

        QByteArray buf;
        buf.resize(n * entrySz);
        char *dst = buf.data();

        const float dotScale = 0.0006f;

        for (int i = 0; i < n; ++i) {
            const auto &pt = trail[i];
            float age = static_cast<float>(i) / n;

            // newer = brighter white-blue, older = dim
            float r = 0.2f + 0.15f * age;
            float g = 0.4f + 0.25f * age;
            float b = 0.8f + 0.2f * age;
            float a = 0.3f + 0.7f * age;

            auto entry = calculateTableEntry(
                QVector3D(pt.x, pt.z, -pt.y),
                QVector3D(dotScale, dotScale, dotScale),
                QVector3D(0, 0, 0),
                QColor::fromRgbF(r, g, b, a),
                {}
            );
            memcpy(dst, &entry, entrySz);
            dst += entrySz;
        }

        *instanceCount = n;
        return buf;
    }

private:
    TelemetryStore *m_store = nullptr;
};
