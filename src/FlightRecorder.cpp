#include "FlightRecorder.h"
#include "TelemetryStore.h"

#include <QDateTime>
#include <QDir>

FlightRecorder::FlightRecorder(TelemetryStore *store, QObject *parent)
    : QObject(parent), m_store(store)
{
    connect(m_store, &TelemetryStore::telemetryChanged, this, &FlightRecorder::onTelemetryChanged);
}

bool FlightRecorder::isRecording() const { return m_recording; }
QString FlightRecorder::filePath() const { return m_file.fileName(); }
int FlightRecorder::sampleCount() const { return m_sampleCount; }

double FlightRecorder::elapsedSeconds() const
{
    if (!m_recording) return 0.0;
    return m_elapsed.elapsed() / 1000.0;
}

void FlightRecorder::start(const QString &directory)
{
    if (m_recording) return;

    QDir dir(directory);
    if (!dir.exists()) dir.mkpath(".");

    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString path = dir.filePath(QString("flight_%1.csv").arg(timestamp));

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    m_stream.setDevice(&m_file);
    m_stream << "time_s,pos_x,pos_y,pos_z,vel_x,vel_y,vel_z,"
                "roll,pitch,yaw,speed,altitude,"
                "battery_v,battery_pct,"
                "exec_state,mission_mode,command_source,"
                "control_state,controller,failsafe,armed\n";

    m_elapsed.start();
    m_sampleCount = 0;
    m_lastWriteMs = 0;
    m_recording = true;
}

void FlightRecorder::stop()
{
    if (!m_recording) return;
    m_recording = false;
    m_stream.flush();
    m_file.close();
}

void FlightRecorder::onTelemetryChanged()
{
    if (!m_recording) return;

    const qint64 now = m_elapsed.elapsed();
    if (now - m_lastWriteMs < kMinIntervalMs) return;
    m_lastWriteMs = now;

    m_stream << QString::number(now / 1000.0, 'f', 3) << ','
             << QString::number(m_store->positionX(), 'f', 4) << ','
             << QString::number(m_store->positionY(), 'f', 4) << ','
             << QString::number(m_store->positionZ(), 'f', 4) << ','
             << QString::number(m_store->velocityX(), 'f', 4) << ','
             << QString::number(m_store->velocityY(), 'f', 4) << ','
             << QString::number(m_store->velocityZ(), 'f', 4) << ','
             << QString::number(m_store->roll(), 'f', 2) << ','
             << QString::number(m_store->pitch(), 'f', 2) << ','
             << QString::number(m_store->yaw(), 'f', 2) << ','
             << QString::number(m_store->speed(), 'f', 3) << ','
             << QString::number(m_store->altitude(), 'f', 3) << ','
             << QString::number(m_store->batteryVoltage(), 'f', 2) << ','
             << QString::number(m_store->batteryPercent(), 'f', 3) << ','
             << m_store->execState() << ','
             << m_store->missionMode() << ','
             << m_store->activeCommandSource() << ','
             << m_store->controlState() << ','
             << m_store->controllerMode() << ','
             << (m_store->failsafe() ? "1" : "0") << ','
             << (m_store->armed() ? "1" : "0") << '\n';

    ++m_sampleCount;
}
