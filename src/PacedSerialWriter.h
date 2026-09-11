#pragma once

#include <QElapsedTimer>
#include <QIODevice>
#include <QObject>
#include <QQueue>
#include <QTimer>

// One FIFO for normal MAVLink traffic. Local LR24 configuration owns the port
// before start(); the owner must stop this writer before closing/reconfiguring it.
class PacedSerialWriter : public QObject
{
    Q_OBJECT
public:
    explicit PacedSerialWriter(QIODevice *device, QObject *parent = nullptr);
    void start(int baud);
    void stop();
    bool enqueue(const QByteArray &message);
    qsizetype queuedBytes() const { return m_queuedBytes; }
    qsizetype queuedMessages() const { return m_messages.size(); }
    static constexpr qsizetype ChunkBytes = 200;
    static constexpr qsizetype MaxQueuedBytes = 64 * 1024;
    static constexpr qsizetype MaxQueuedMessages = 128;
    static constexpr qint64 MaxQueuedTimeUs = 1000000;
    static qint64 intervalUs(qint64 bytes, int baud);

signals:
    void bytesAccepted(qint64 bytes);
    void failed(const QString &reason);

private:
    void pump();
    void fail(const QString &reason);
    qint64 messageTimeUs(qsizetype bytes) const;
    QIODevice *m_device;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QQueue<QByteArray> m_messages;
    qsizetype m_offset = 0;
    qsizetype m_queuedBytes = 0;
    qint64 m_queuedTimeUs = 0;
    qint64 m_nextWriteUs = 0;
    qint64 m_stalledSinceUs = -1;
    quint64 m_generation = 0;
    int m_baud = 0;
    bool m_active = false;
};
