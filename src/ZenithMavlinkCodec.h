#pragma once

#include <QByteArray>
#include <QVariantMap>
#include <QVector>
#include <memory>

// Transport-free adapter between the shared MAVLink wire protocol and the UI's
// domain messages. Each byte stream has a separate parser; no global channels.
class ZenithMavlinkCodec
{
public:
    struct Message {
        int kind = -1;
        int systemId = 0;
        QVariantMap payload;
        bool heartbeat = false;
        bool telemetry = false;
    };

    ZenithMavlinkCodec();
    ~ZenithMavlinkCodec();
    ZenithMavlinkCodec(const ZenithMavlinkCodec &) = delete;
    ZenithMavlinkCodec &operator=(const ZenithMavlinkCodec &) = delete;

    QByteArray encode(int kind, int targetSystem, const QVariantMap &payload);
    QVector<Message> append(const QByteArray &bytes, int channel = 0);
    void resetReceive();
    void setTargetSystem(int systemId);
    bool fcuHeartbeatFresh() const;

private:
    class Impl;
    std::unique_ptr<Impl> d;
};
