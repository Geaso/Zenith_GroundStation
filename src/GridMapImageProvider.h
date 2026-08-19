#pragma once

#include <QQuickImageProvider>
#include <QImage>
#include "TelemetryStore.h"

class GridMapImageProvider : public QQuickImageProvider
{
public:
    GridMapImageProvider(TelemetryStore *store)
        : QQuickImageProvider(QQuickImageProvider::Image), m_store(store) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        Q_UNUSED(id)
        if (!m_store->gridMapValid()) {
            QImage empty(1, 1, QImage::Format_ARGB32);
            empty.fill(Qt::transparent);
            if (size) *size = empty.size();
            return empty;
        }

        const int w = m_store->gridMapWidth();
        const int h = m_store->gridMapHeight();
        const auto &cells = m_store->gridMapCells();

        QImage img(w, h, QImage::Format_ARGB32);
        img.fill(qRgba(0, 0, 0, 0));

        for (int y = 0; y < h; ++y) {
            QRgb *line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                int idx = y * w + x;
                if (idx < cells.size() && cells[idx] != 0) {
                    float t = (cells[idx] - 1) / 254.0f;
                    int r, g, b;
                    if (t < 0.25f) {
                        float s = t / 0.25f;
                        r = 0; g = static_cast<int>(s * 255); b = 255;
                    } else if (t < 0.5f) {
                        float s = (t - 0.25f) / 0.25f;
                        r = 0; g = 255; b = static_cast<int>((1.0f - s) * 255);
                    } else if (t < 0.75f) {
                        float s = (t - 0.5f) / 0.25f;
                        r = static_cast<int>(s * 255); g = 255; b = 0;
                    } else {
                        float s = (t - 0.75f) / 0.25f;
                        r = 255; g = static_cast<int>((1.0f - s) * 255); b = 0;
                    }
                    line[x] = qRgba(r, g, b, 200);
                }
            }
        }

        if (size) *size = img.size();
        return img;
    }

private:
    TelemetryStore *m_store;
};
