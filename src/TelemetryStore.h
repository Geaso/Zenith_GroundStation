#pragma once

#include <QObject>
#include <QPointF>
#include <QVariantMap>
#include <QByteArray>
#include <QVector>

class TelemetryStore : public QObject
{
    Q_OBJECT
public:
    explicit TelemetryStore(QObject *parent = nullptr);

    QString vehicleName() const;
    QString flightStatus() const;
    QString flightMode() const;
    QString controllerMode() const;
    QString controlState() const;
    QString execState() const;
    QString missionMode() const;
    QString activeCommandSource() const;
    QString pendingRequest() const;
    bool requestActive() const;
    quint32 lastReachedWaypointId() const;
    QString locationSource() const;
    QString gpsStatus() const;
    int gpsFixType() const;
    int locationSourceId() const;
    QString heartbeatLink() const;
    QString videoLink() const;
    QString rcLink() const;
    bool armed() const;
    bool connected() const;
    bool failsafe() const;
    double batteryVoltage() const;
    double batteryPercent() const;
    double altitude() const;
    double relativeAltitude() const;
    double range() const;
    double speed() const;
    double positionX() const;
    double positionY() const;
    double positionZ() const;
    double vinsPositionX() const;
    double vinsPositionY() const;
    double vinsPositionZ() const;
    bool odomValid() const;
    double velocityX() const;
    double velocityY() const;
    double velocityZ() const;
    double heading() const;
    double roll() const;
    double pitch() const;
    double yaw() const;
    double desiredPositionX() const;
    double desiredPositionY() const;
    double desiredPositionZ() const;
    double desiredVelocityX() const;
    double desiredVelocityY() const;
    double desiredVelocityZ() const;
    double homeDistance() const;
    QString missionStage() const;
    QString videoStatus() const;
    QString alertLevel() const;
    QString currentTime() const;
    QString lastCommand() const;
    QString commandAck() const;
    QString managedTaskName() const;
    QString managedTaskPath() const;
    QString managedTaskState() const;
    QString managedTaskReason() const;
    QString managedTaskAck() const;
    QString managedTaskRequestId() const;
    bool managedTaskActive() const;
    QList<QPointF> pathPoints() const;
    QList<QPointF> waypointPoints() const;
    QString currentVehicleTopicRoot() const;
    int currentVehicleId() const;
    QStringList runningNodes() const;
    QString moduleExecFeedback() const;

    void setVehicleName(const QString &name);
    void applyUavState(const QVariantMap &payload, int senderId);
    void applyTextInfo(const QVariantMap &payload);
    void applyUavControlState(const QVariantMap &payload);
    void applyHeartbeat(const QVariantMap &payload);
    void setTransportHealth(bool telemetryFresh, bool heartbeatFresh, const QString &summary);
    void setCommandFeedback(const QString &commandName, const QString &ackText);
    void setMissionStage(const QString &stage);
    void setWaypointPoints(const QList<QPointF> &points);
    void setDesiredReference(double posX, double posY, double posZ, double velX, double velY, double velZ);

    void applyGridMap(const QVariantMap &payload);
    float gridMapOriginX() const { return m_gmOriginX; }
    float gridMapOriginY() const { return m_gmOriginY; }
    float gridMapResolution() const { return m_gmResolution; }
    int gridMapWidth() const { return m_gmWidth; }
    int gridMapHeight() const { return m_gmHeight; }
    float gridMapSliceZ() const { return m_gmSliceZ; }
    const QVector<uint8_t>& gridMapCells() const { return m_gmCells; }
    bool gridMapValid() const { return m_gmWidth > 0 && m_gmHeight > 0; }

    struct TrailPt { float x, y, z; };
    const QVector<TrailPt>& trail3D() const { return m_trail3D; }
    int trailSize() const { return m_trail3D.size(); }

    void applyPlannedPath(const QVariantMap &payload);
    const QVector<TrailPt>& plannedPath() const { return m_plannedPath; }
    int plannedPathSize() const { return m_plannedPath.size(); }

    // ---- 解锁前检查（PX4 SYS_STATUS 传感器健康位，经 CustomDataSegment/113 上报）----
    void applyCustomDataSegment(const QVariantMap &payload);
    bool preflightValid() const;          // 是否收到过上报
    bool preflightArmOk() const;          // 子系统健康汇总（prearmBit=false 时不等于飞控允许解锁）
    QString preflightFail() const;        // 故障子系统名，逗号分隔
    QVariantList preflightChecks() const; // [{name, enabled, healthy}] 供面板逐项显示
    QString missionLogText() const;       // 任务日志滚动缓冲（最新在上）
    // ---- 链路/就绪状态（DJI 式分层提示）----
    void noteFrameReceived();             // 收到任一 CRC 通过的帧时调用
    bool linkEstablished() const;         // 已对频：收到过有效帧
    // 飞行遥测是否已稳定。connected 只代表"收到过任一帧"（心跳就能置真），而 UAVSTATE
    // 要等机载控制状态机起来才有，中间那十几秒 UI 会把默认值 0.00 当成真实读数显示。
    // 这里要求连续收到若干帧 UAVSTATE 且持续一小段时间，期间前端只显示"已建立连接"。
    // 刻意不依赖 rd_mask/batteryValid——preflight_reporter 是可选节点，没跑时不能把
    // 整个仪表盘永久卡在等待态。
    bool telemetryStable() const;
    bool batteryValid() const;            // 电池首值是否已到（未到时 UI 显示 "--" 而非 0.0V）
    bool fcuReady() const;
    bool locReady() const;
    bool ctrlReady() const;
    int  aircraftUptime() const;
    QVariantList readinessSteps() const;  // [{name, ready, atSec}] 供就绪时间线
    Q_INVOKABLE void clearMissionLog();
    bool preflightPrearmBit() const;      // 飞控是否上报了权威的 PREARM_CHECK 位
    int preflightArmAck() const;          // 上次解锁尝试: -1未试 0接受 其余为拒绝码
    QString preflightArmAckText() const;  // 拒绝原因中文

signals:
    void telemetryChanged();
    void pathChanged();
    void gridMapChanged();
    void plannedPathChanged();

private:
    // 链路断开后作废车辆遥测缓存，避免把上一次的数据当成当前值显示
    void invalidateVehicleData();

    QString locationSourceName(int locationSource) const;
    QString gpsStatusName(int gpsStatus) const;
    QList<QPointF> m_pathPoints;
    QList<QPointF> m_waypointPoints;
    QString m_vehicleName;
    QString m_flightStatus;
    QString m_flightMode;
    QString m_controllerMode;
    QString m_controlState;
    QString m_execState;
    QString m_missionMode;
    QString m_activeCommandSource;
    QString m_pendingRequest;
    bool m_requestActive = false;
    quint32 m_lastReachedWaypointId = 0;
    QString m_locationSource;
    QString m_gpsStatus;
    int m_gpsFixType = 0;       // raw gps_status integer (0=NO_GPS .. 6=RTK_FIXED)
    int m_locationSourceId = 0; // raw location_source integer，取值见 zenith_msgs/UAVState.msg
    // 解锁前检查
    bool m_preflightValid = false;
    bool m_preflightArmOk = false;
    QString m_preflightFail;
    quint32 m_preflightPresent = 0;
    quint32 m_preflightEnabled = 0;
    quint32 m_preflightHealth = 0;
    QStringList m_missionLog;   // 任务日志，最新在前，上限 MissionLogMax 条
    static const int MissionLogMax = 300;
    bool m_linkEstablished = false;
    int  m_uavStateFrames = 0;      // 自上次链路作废以来收到的 UAVSTATE 帧数
    qint64 m_firstUavStateMs = 0;   // 首帧 UAVSTATE 的单调时刻，0 表示尚未收到
    static const int StableFrames = 5;      // 10Hz 下约 0.5s
    static const int StableDwellMs = 1000;  // 再叠加 1s 静默期，跨过重启抖动
    int  m_readyMask = 0;
    QString m_readyTimes;
    int  m_aircraftUptime = 0;
    bool m_preflightPrearmBit = false;
    int m_preflightArmAck = -1;
    QString m_preflightArmAckText;
    QString m_heartbeatLink;
    QString m_videoLink;
    QString m_rcLink;
    QString m_missionStage;
    QString m_videoStatus;
    QString m_alertLevel;
    QString m_currentTime;
    QString m_lastCommand;
    QString m_commandAck;
    QString m_managedTaskName;
    QString m_managedTaskPath;
    QString m_managedTaskState = "UNKNOWN";
    QString m_managedTaskReason = "尚未收到机载任务管理器状态";
    QString m_managedTaskAck;
    QString m_managedTaskRequestId;
    bool m_managedTaskActive = false;
    bool m_armed = false;
    bool m_connected = false;
    bool m_failsafe = false;
    double m_batteryVoltage = 0.0;
    double m_batteryPercent = 0.0;
    double m_altitude = 0.0;
    double m_relativeAltitude = 0.0;
    double m_range = 0.0;
    double m_speed = 0.0;
    double m_positionX = 0.0;
    double m_positionY = 0.0;
    double m_positionZ = 0.0;
    double m_vinsPositionX = 0.0;
    double m_vinsPositionY = 0.0;
    double m_vinsPositionZ = 0.0;
    bool m_odomValid = false;
    double m_velocityX = 0.0;
    double m_velocityY = 0.0;
    double m_velocityZ = 0.0;
    double m_heading = 0.0;
    double m_roll = 0.0;
    double m_pitch = 0.0;
    double m_yaw = 0.0;
    double m_desiredPositionX = 0.0;
    double m_desiredPositionY = 0.0;
    double m_desiredPositionZ = 0.0;
    double m_desiredVelocityX = 0.0;
    double m_desiredVelocityY = 0.0;
    double m_desiredVelocityZ = 0.0;
    double m_homeDistance = 0.0;
    int m_currentVehicleId = 1;
    QStringList m_runningNodes;
    QString m_moduleExecFeedback;

    float m_gmOriginX = 0, m_gmOriginY = 0, m_gmResolution = 0.15f, m_gmSliceZ = 0;
    int m_gmWidth = 0, m_gmHeight = 0;
    QVector<uint8_t> m_gmCells;

    QVector<TrailPt> m_trail3D;
    QVector<TrailPt> m_plannedPath;
    static constexpr int kMaxTrail = 500;
};
