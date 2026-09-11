#include "PacedSerialWriter.h"

PacedSerialWriter::PacedSerialWriter(QIODevice *device, QObject *parent)
    : QObject(parent), m_device(device)
{
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &PacedSerialWriter::pump);
}

qint64 PacedSerialWriter::intervalUs(qint64 bytes, int baud)
{
    // 8N1: ten bits per byte. Round up both here and when arming QTimer.
    return (bytes * 10000000LL + baud - 1) / baud + 8000;
}

qint64 PacedSerialWriter::messageTimeUs(qsizetype bytes) const
{
    return (bytes / ChunkBytes) * intervalUs(ChunkBytes, m_baud)
        + (bytes % ChunkBytes ? intervalUs(bytes % ChunkBytes, m_baud) : 0);
}

void PacedSerialWriter::start(int baud)
{
    stop();
    if (baud <= 0) return;
    m_baud = baud;
    m_active = true;
    m_clock.start();
}

void PacedSerialWriter::stop()
{
    ++m_generation;
    m_active = false;
    m_timer.stop();
    m_messages.clear();
    m_queuedBytes = 0;
    m_queuedTimeUs = 0;
    m_offset = 0;
    m_nextWriteUs = 0;
    m_stalledSinceUs = -1;
}

bool PacedSerialWriter::enqueue(const QByteArray &message)
{
    if (!m_active || message.isEmpty()
        || message.size() > MaxQueuedBytes - m_queuedBytes
        || m_messages.size() >= MaxQueuedMessages
        || messageTimeUs(message.size()) > MaxQueuedTimeUs - m_queuedTimeUs) return false;
    // Keep a whole encoded message at the head until all its MAVLink fragments
    // have been accepted; later heartbeats/commands cannot split that message.
    m_messages.enqueue(message);
    m_queuedBytes += message.size();
    // Bound scheduled wire time as well as memory, so a low baud connection
    // cannot accumulate seconds of commands ahead of the next heartbeat.
    m_queuedTimeUs += messageTimeUs(message.size());
    if (!m_timer.isActive()) m_timer.start(0);
    return true;
}

void PacedSerialWriter::fail(const QString &reason)
{
    stop();
    emit failed(reason);
}

void PacedSerialWriter::pump()
{
    if (!m_active) return;
    if (!m_device->isOpen() || !m_device->isWritable()) {
        fail(QStringLiteral("Serial transmit device is not writable"));
        return;
    }
    const qint64 now = m_clock.nsecsElapsed() / 1000;
    if (now < m_nextWriteUs) {
        m_timer.start(static_cast<int>((m_nextWriteUs - now + 999) / 1000));
        return;
    }
    if (m_messages.isEmpty() && m_device->bytesToWrite() == 0) {
        m_stalledSinceUs = -1;
        return;
    }
    if (m_stalledSinceUs < 0) m_stalledSinceUs = now;
    if (now - m_stalledSinceUs >= 5000000) {
        fail(QStringLiteral("Serial transmit stalled for 5 seconds"));
        return;
    }
    // Do not let the Qt/driver output buffer collapse separately paced chunks
    // into a larger burst while the device is backpressured.
    if (m_device->bytesToWrite() > 0) {
        m_timer.start(2);
        return;
    }
    const QByteArray chunk = m_messages.head().mid(m_offset, ChunkBytes);
    const quint64 generation = m_generation;
    const qint64 written = m_device->write(chunk);
    // QSerialPort can synchronously emit errorOccurred from write(). Its owner
    // may stop/reconnect us, invalidating the head and this transmission session.
    if (generation != m_generation || !m_active) return;
    if (written < 0 || written > chunk.size()) {
        fail(QStringLiteral("Serial write failed: %1").arg(m_device->errorString()));
        return;
    }
    if (written == 0) {
        m_timer.start(2);
        return;
    }
    m_stalledSinceUs = -1;
    m_offset += written;
    if (m_offset == m_messages.head().size()) {
        m_queuedBytes -= m_messages.head().size();
        m_queuedTimeUs -= messageTimeUs(m_messages.head().size());
        m_messages.dequeue();
        m_offset = 0;
    }
    m_nextWriteUs = m_clock.nsecsElapsed() / 1000 + intervalUs(written, m_baud);
    // Check the final chunk too: an empty FIFO does not mean the driver has
    // drained its output, and a stuck final write must still fail the session.
    m_timer.start(static_cast<int>((intervalUs(written, m_baud) + 999) / 1000));
    emit bytesAccepted(written);
}
