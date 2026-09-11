#include "PacedSerialWriter.h"

#include <QCoreApplication>
#include <QThread>
#include <functional>

namespace {
bool check(bool condition, const char *message)
{
    if (!condition) qCritical("FAIL: %s", message);
    return condition;
}

bool waitFor(const std::function<bool()> &condition, int timeoutMs = 2000)
{
    QElapsedTimer clock;
    clock.start();
    while (!condition() && clock.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    return condition();
}

class FakeSerial : public QIODevice
{
public:
    struct Write { qint64 atUs; QByteArray requested; qint64 accepted; };
    FakeSerial() { open(QIODevice::WriteOnly | QIODevice::Unbuffered); clock.start(); }
    qint64 bytesToWrite() const override { return pending; }
    QList<Write> writes;
    QByteArray output;
    QElapsedTimer clock;
    qint64 pending = 0;
    std::function<qint64(const QByteArray &)> onWrite;
protected:
    qint64 readData(char *, qint64) override { return -1; }
    qint64 writeData(const char *data, qint64 size) override
    {
        const QByteArray bytes(data, size);
        const qint64 at = clock.nsecsElapsed() / 1000;
        const qint64 accepted = onWrite ? onWrite(bytes) : size;
        writes.append({at, bytes, accepted});
        if (accepted > 0) output.append(bytes.first(accepted));
        return accepted;
    }
};

bool burstAndShortWrites(int baud, int shortWriteLimit)
{
    FakeSerial device;
    PacedSerialWriter writer(&device);
    device.onWrite = [=](const QByteArray &bytes) { return qMin(bytes.size(), qsizetype(shortWriteLimit)); };
    writer.start(baud);
    // Reproduces the failing two-frame 399-byte task request, with a heartbeat
    // and a later command arriving before its fragments finish transmitting.
    const QByteArray task = QByteArray(255, 'T') + QByteArray(144, 'S');
    const QByteArray heartbeat(21, 'H');
    const QByteArray next(233, 'N');
    if (!check(writer.enqueue(task) && writer.enqueue(heartbeat) && writer.enqueue(next), "burst accepted atomically")) return false;
    if (!check(device.writes.isEmpty(), "enqueue does not write synchronously on the UI thread")) return false;
    int uiTicks = 0;
    QTimer uiTimer;
    QObject::connect(&uiTimer, &QTimer::timeout, [&] { ++uiTicks; });
    uiTimer.start(1);
    if (!check(waitFor([&] { return writer.queuedMessages() == 0; }), "paced burst drains")) return false;
    if (!check(device.output == task + heartbeat + next, "short writes preserve exact bytes and whole-message FIFO order")) return false;
    if (!check(uiTicks > 5, "event loop remains responsive while transmitting")) return false;
    for (qsizetype i = 0; i < device.writes.size(); ++i) {
        if (!check(device.writes[i].requested.size() <= 200, "no UART write exceeds 200 bytes")) return false;
        if (i && !check(device.writes[i].atUs - device.writes[i-1].atUs
            >= PacedSerialWriter::intervalUs(device.writes[i-1].accepted, baud),
            "chunks wait for the accepted bytes' 8N1 wire time plus 8ms")) return false;
    }
    return true;
}

bool backpressureAndFailure()
{
    FakeSerial device;
    PacedSerialWriter writer(&device);
    writer.start(921600);
    device.pending = 1;
    writer.enqueue(QByteArray(399, 'A'));
    QTimer::singleShot(35, [&] { device.pending = 0; });
    int calls = 0;
    device.onWrite = [&](const QByteArray &b) { return calls++ == 0 ? qint64(0) : qint64(b.size()); };
    if (!check(waitFor([&] { return writer.queuedBytes() == 0; }) && device.output == QByteArray(399, 'A'),
        "driver backpressure and zero-byte writes retain the unsent message")) return false;
    if (!check(device.writes.first().atUs >= 30000, "no writes while the driver still has pending output")) return false;
    QString error;
    QObject::connect(&writer, &PacedSerialWriter::failed, [&](const QString &reason) { error = reason; });
    device.onWrite = [](const QByteArray &) { return qint64(-1); };
    writer.enqueue("failure");
    if (!check(waitFor([&] { return !error.isEmpty(); }) && writer.queuedBytes() == 0
        && !writer.enqueue("blocked"), "write error cancels the session and its remaining queue")) return false;

    writer.start(921600);
    error.clear();
    device.onWrite = [&](const QByteArray &b) { device.pending = b.size(); return b.size(); };
    writer.enqueue("last chunk");
    return check(waitFor([&] { return !error.isEmpty(); }, 6500)
        && error.contains("stalled"), "a stuck final driver chunk triggers the stall watchdog even with an empty FIFO");
}

bool cancellationAndBounds()
{
    FakeSerial device;
    PacedSerialWriter writer(&device);
    writer.start(921600);
    writer.enqueue(QByteArray(399, 'O'));
    writer.stop();
    if (!check(!writer.enqueue("closed") && writer.queuedBytes() == 0, "stop rejects new traffic and clears pending messages")) return false;
    writer.start(921600);
    if (!check(!writer.enqueue(QByteArray(PacedSerialWriter::MaxQueuedBytes + 1, 'X'))
        && writer.queuedBytes() == 0, "oversized message rejected as a whole")) return false;
    writer.start(9600);
    if (!check(!writer.enqueue(QByteArray(1200, 'X')), "low baud bounds queued wire time, not just memory")) return false;
    int accepted = 0;
    while (writer.enqueue(QByteArray(200, 'B'))) ++accepted;
    if (!check(accepted > 0 && accepted < 10, "FIFO has a finite time budget")) return false;
    writer.stop();
    writer.start(921600);
    bool restarted = false;
    device.onWrite = [&](const QByteArray &bytes) {
        if (!restarted) {
            restarted = true;
            // Simulate QSerialPort::write synchronously causing a reconnect.
            writer.stop();
            writer.start(921600);
            writer.enqueue("new session");
        }
        return bytes.size();
    };
    writer.enqueue(QByteArray(399, 'R'));
    if (!check(waitFor([&] { return restarted && writer.queuedMessages() == 0; }), "reentrant stop/start does not corrupt the new session")) return false;
    return check(device.output == QByteArray(200, 'R') + "new session",
        "stop/restart cancels old queued tails without leaking them into the new session");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (!burstAndShortWrites(921600, 200) || !burstAndShortWrites(115200, 73)
        || !cancellationAndBounds() || !backpressureAndFailure()) return 1;
    qInfo("Paced serial FIFO, timing, partial write, backpressure and cancellation tests passed");
    return 0;
}
