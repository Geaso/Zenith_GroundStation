// telemetryStable 决定仪表盘在换电池/机载重启后何时恢复显示数值。
// 判据写松了，过渡期的默认值 0.00 会被当成真实读数；写紧了（比如依赖
// preflight_reporter 的 rd_mask），没跑那个可选节点的机型会永久停在"等待数据"。
// 这里把两侧边界都钉住。
#include "TelemetryStore.h"

#include <QCoreApplication>
#include <QThread>
#include <QVariantMap>

namespace {

bool require(bool condition, const char *message)
{
    if (!condition) {
        qCritical("FAILED: %s", message);
    }
    return condition;
}

QVariantMap uavStatePayload(double voltage)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("connected"), true);
    payload.insert(QStringLiteral("armed"), false);
    payload.insert(QStringLiteral("battery_state"), voltage);
    payload.insert(QStringLiteral("position"), QVariantList{1.0, 2.0, 3.0});
    return payload;
}

// 连续灌入 UAVSTATE，模拟 10Hz 遥测
void feed(TelemetryStore &store, int frames, double voltage = 20.0)
{
    for (int i = 0; i < frames; ++i) {
        store.applyUavState(uavStatePayload(voltage), 1);
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    {
        TelemetryStore store;
        if (!require(!store.telemetryStable(), "刚构造时不应判定为稳定")) {
            return 1;
        }
    }

    {
        // 选中的应用层目标必须与实际收到的 senderId 分开保存；否则误连遥测会
        // 悄悄改写后续指令目标，界面也无法给出“期望/实际”告警。
        TelemetryStore store;
        store.setVehicleName(QStringLiteral("UAV215"));
        store.noteFrameReceived(214);
        store.applyUavState(uavStatePayload(20.0), 214);
        if (!require(store.currentVehicleId() == 215,
                     "遥测 senderId 不得覆盖操作员选中的 vehicleId")) {
            return 1;
        }
        if (!require(store.lastTelemetrySenderId() == 214,
                     "应单独记录最近实际遥测 senderId")) {
            return 1;
        }
        store.setTransportHealth(false, false, QStringLiteral("link lost"));
        if (!require(store.lastTelemetrySenderId() == -1,
                     "链路丢失后实际 senderId 应失效")) {
            return 1;
        }
    }

    {
        // 即使只有一帧有效协议消息、尚未形成稳定 UAVSTATE，链路失效也
        // 必须清掉实际 sender，避免自动重连期间显示上一台飞机的告警。
        TelemetryStore store;
        store.noteFrameReceived(214);
        store.setTransportHealth(false, false, QStringLiteral("link lost"));
        if (!require(store.lastTelemetrySenderId() == -1,
                     "无稳定遥测时链路丢失也应作废实际 senderId")) {
            return 1;
        }
    }

    {
        // 帧数够但静默期没到 —— 这正是重启后数值跳变的窗口
        TelemetryStore store;
        feed(store, 8);
        if (!require(!store.telemetryStable(), "帧数已够但未过静默期，不应判稳定")) {
            return 1;
        }
    }

    {
        // 只有一两帧，即使等足时间也不算稳定
        TelemetryStore store;
        feed(store, 1);
        QThread::msleep(1200);
        if (!require(!store.telemetryStable(), "帧数不足时不应判稳定")) {
            return 1;
        }
    }

    {
        // 帧数 + 静默期都满足
        TelemetryStore store;
        feed(store, 5);
        QThread::msleep(1200);
        feed(store, 1);
        if (!require(store.telemetryStable(), "帧数与静默期均满足时应判稳定")) {
            return 1;
        }

        // 掉链路（拔电池）后必须立刻退回不稳定，否则会拿旧电池的读数糊弄用户
        store.setTransportHealth(false, false, QStringLiteral("link lost"));
        if (!require(!store.telemetryStable(), "链路丢失后应立即退回不稳定")) {
            return 1;
        }
        if (!require(store.batteryVoltage() == 0.0, "链路丢失后应作废缓存电压")) {
            return 1;
        }

        // 重新上电：心跳先到，UAVSTATE 还没来，此时仍不能显示数值
        QVariantMap heartbeat;
        heartbeat.insert(QStringLiteral("count"), 1);
        store.applyHeartbeat(heartbeat);
        if (!require(!store.telemetryStable(),
                     "仅有心跳、尚无 UAVSTATE 时不应判稳定")) {
            return 1;
        }

        // UAVSTATE 恢复并稳定后才重新显示
        feed(store, 5, 16.8);
        QThread::msleep(1200);
        feed(store, 1, 16.8);
        if (!require(store.telemetryStable(), "遥测恢复并稳定后应重新判稳定")) {
            return 1;
        }
    }

    return 0;
}
