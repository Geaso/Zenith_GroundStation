#pragma once
#include "ZenithProtocol.h"
#include <functional>
namespace ZenithProtocol {
// Pure codec entry point: offline tests never need to open a transport.
struct FrameDecodeResult {
    int msgId = -1;
    int robotId = 0;
    QVariantMap payload;
    int totalBytes = 0;
    bool valid = false;
};
FrameDecodeResult decodeFrame(const QByteArray &buffer);
int consumeFrames(QByteArray &buffer, const std::function<void(const FrameDecodeResult &)> &onFrame);

}
