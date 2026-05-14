#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>

class TelemetryStore;

class FlightRecorder : public QObject
{
    Q_OBJECT
public:
    explicit FlightRecorder(TelemetryStore *store, QObject *parent = nullptr);

    bool isRecording() const;
    QString filePath() const;
    int sampleCount() const;
    double elapsedSeconds() const;

    void start(const QString &directory);
    void stop();

private slots:
    void onTelemetryChanged();

private:
    TelemetryStore *m_store = nullptr;
    QFile m_file;
    QTextStream m_stream;
    QElapsedTimer m_elapsed;
    bool m_recording = false;
    int m_sampleCount = 0;
    qint64 m_lastWriteMs = 0;
    static constexpr int kMinIntervalMs = 100; // 10 Hz max
};
