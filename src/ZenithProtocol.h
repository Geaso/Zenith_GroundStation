#pragma once

#include <QString>
#include <QStringList>

namespace ZenithProtocol {

constexpr int kUdpPort = 8889;
constexpr int kTcpPort = 55555;
constexpr int kTcpHeartbeatPort = 55556;

inline const QString kTopicPrefix = "/zenith";
inline const QString kGroundStationNode = "zenith_communication_bridge";
inline const QString kControlPackage = "zenith_uav_control";
inline const QString kCommunicationPackage = "zenith_communication_bridge";

// Zenith ROS topics are expected to use the /zenith namespace only.
// This project should not emit or consume /prometheus topic paths.

enum MessageId {
    UAVSTATE = 1,
    TEXTINFO = 3,
    GIMBALSTATE = 4,
    VISIONDIFF = 5,
    HEARTBEAT = 6,
    UGVSTATE = 7,
    MULTIDETECTIONINFO = 8,
    UAVCONTROLSTATE = 9,
    POSESTAMPED = 10,

    SWARMCOMMAND = 101,
    GIMBALCONTROL = 102,
    GIMBALSERVICE = 103,
    WINDOWPOSITION = 104,
    UGVCOMMAND = 105,
    GIMBALPARAMSET = 106,
    IMAGEDATA = 107,
    UAVCOMMAND = 108,
    UAVSETUP = 109,
    PARAMSETTINGS = 110,
    BSPLINE = 111,
    MULTIBSPLINES = 112,
    CUSTOMDATASEGMENT_1 = 113,

    CONNECTSTATE = 201,
    MODESELECTION = 202,
    UGVLASERSCAN = 230,
    UGVPOINTCLOUND2 = 231,
    UGVTFMESSAGE = 232,
    UGVTFSTATIC = 233,
    UGVMARKERARRAY = 234,
    UGVMARKERARRAYLANDMARK = 235,
    UGVMARKERARRAYTRAJECTORY = 236,
    GOAL = 255
};

enum LocationSource {
    MOCAP = 0,
    T265 = 1,
    GAZEBO = 2,
    FAKE_ODOM = 3,
    GPS = 4,
    RTK = 5,
    UWB = 6,
    VINS = 7,
    OPTICAL_FLOW = 8,
    VIOBOT = 9,
    MID360 = 10,
    BSA_SLAM = 11,
    ODIN = 12,
    PROSIM = 13
};

enum ModeSelectionMode {
    UAVBASIC_MODE = 1,
    UGVBASIC_MODE = 2,
    SWARMCONTROL_MODE = 3,
    AUTONOMOUSLANDING_MODE = 4,
    OBJECTTRACKING_MODE = 5,
    EGOPLANNER_MODE = 6,
    TRAJECTORYCONTROL_MODE = 7,
    CUSTOMMODE_MODE = 8,
    REBOOTNX_MODE = 9,
    EXITNX_MODE = 10
};

enum ModeSelectionUseMode {
    UM_CREATE = 0,
    UM_DELETE = 1
};

struct TelemetryFieldSet
{
    static inline const QStringList kRequiredFields = {
        "uav_id",
        "connected",
        "armed",
        "mode",
        "location_source",
        "odom_valid",
        "gps_status",
        "gps_num",
        "position[3]",
        "velocity[3]",
        "attitude[3]",
        "attitude_q",
        "attitude_rate[3]",
        "battery_state",
        "battery_percetage",
        "latitude",
        "longitude",
        "altitude"
    };

    static inline const QStringList kZenithExtraFields = {
        "range",
        "rel_alt"
    };
};

struct CommandFieldSet
{
    static inline const QStringList kUavCommandFields = {
        "Agent_CMD",
        "Control_Level",
        "Move_mode",
        "position_ref[3]",
        "velocity_ref[3]",
        "acceleration_ref[3]",
        "yaw_ref",
        "Yaw_Rate_Mode",
        "yaw_rate_ref",
        "att_ref[4]",
        "latitude",
        "longitude",
        "altitude",
        "Command_ID"
    };

    static inline const QStringList kStartScriptFields = {
        "cmd",
        "mode",
        "node_name",
        "detection_cmd",
        "flag",
        "cmd_level",
        "close_cmd"
    };
};

struct ParamSettingsShape
{
    // Zenith ROS message shape differs from the richer binary Struct.hpp form.
    // ROS side: string[] param_name + string[] param_value
    static inline const QStringList kRosFields = {
        "param_name[]",
        "param_value[]"
    };
};

inline QStringList defaultOverviewTopics(int uavId)
{
    const QString root = QString("/uav%1").arg(uavId) + kTopicPrefix;
    return {
        root + "/state",
        root + "/text_info",
        root + "/control_state",
        root + "/command",
        root + "/setup",
        root + "/param_settings",
        root + "/offset_pose",
        root + "/customdatasegment",
        root + "/load_cmd"
    };
}

inline QStringList defaultCommandTopics(int uavId)
{
    const QString root = QString("/uav%1").arg(uavId) + kTopicPrefix;
    return {
        root + "/command",
        root + "/setup",
        root + "/param_settings",
        root + "/load_cmd",
        root + "/switch_location_source"
    };
}

} // namespace ZenithProtocol
