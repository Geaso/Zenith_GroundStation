#pragma once

#include <QQuick3DInstancing>
#include <QVector3D>
#include <QColor>
#include "TelemetryStore.h"

class PlannedPathInstanceTable : public QQuick3DInstancing
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TelemetryStore* store READ store WRITE setStore NOTIFY storeChanged)

public:
    explicit PlannedPathInstanceTable(QQuick3DObject *parent = nullptr) : QQuick3DInstancing(parent) {}

    TelemetryStore* store() const { return m_store; }
    void setStore(TelemetryStore *s) {
        if (m_store == s) return;
        if (m_store) disconnect(m_store, nullptr, this, nullptr);
        m_store = s;
        if (m_store) connect(m_store, &TelemetryStore::plannedPathChanged, this, &PlannedPathInstanceTable::rebuild);
        emit storeChanged();
    }

signals:
    void storeChanged();

public slots:
    void rebuild() { markDirty(); }

protected:
    QByteArray getInstanceBuffer(int *instanceCount) override {
        if (!m_store || m_store->plannedPathSize() < 1) {
            *instanceCount = 0;
            return {};
        }

        const auto &path = m_store->plannedPath();
        const int n = path.size();
        const int entrySz = static_cast<int>(sizeof(InstanceTableEntry));
        const float dotScale = 0.0008f;

        QByteArray buf;
        buf.resize(n * entrySz);
        char *dst = buf.data();

        for (int i = 0; i < n; ++i) {
            const auto &pt = path[i];
            float progress = static_cast<float>(i) / n;

            auto entry = calculateTableEntry(
                QVector3D(pt.x, pt.z, -pt.y),
                QVector3D(dotScale, dotScale, dotScale),
                QVector3D(0, 0, 0),
                QColor::fromRgbF(0.2f + 0.8f * progress, 1.0f, 0.2f, 0.9f),
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
