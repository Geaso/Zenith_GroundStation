/****************************************************************************
** Meta object code from reading C++ file 'AppState.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.6.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/AppState.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AppState.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.6.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSAppStateENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSAppStateENDCLASS = QtMocHelpers::stringData(
    "AppState",
    "telemetryChanged",
    "",
    "pathChanged",
    "commandTriggered",
    "commandName",
    "linkStateChanged",
    "linkSettingsChanged",
    "selectVehicle",
    "name",
    "issueCommand",
    "sendManualMove",
    "mode",
    "x",
    "y",
    "z",
    "yawDeg",
    "runScriptAction",
    "command",
    "target",
    "applyConnectionSettings",
    "hostIp",
    "udpPort",
    "tcpPort",
    "heartbeatPort",
    "connectProtocol",
    "disconnectProtocol",
    "testProtocol",
    "vehicleName",
    "flightStatus",
    "flightMode",
    "controllerMode",
    "controlState",
    "locationSource",
    "gpsStatus",
    "heartbeatLink",
    "videoLink",
    "rcLink",
    "armed",
    "connected",
    "failsafe",
    "batteryVoltage",
    "batteryPercent",
    "altitude",
    "speed",
    "positionX",
    "positionY",
    "positionZ",
    "velocityX",
    "velocityY",
    "velocityZ",
    "heading",
    "roll",
    "pitch",
    "yaw",
    "desiredPositionX",
    "desiredPositionY",
    "desiredPositionZ",
    "desiredVelocityX",
    "desiredVelocityY",
    "desiredVelocityZ",
    "homeDistance",
    "missionStage",
    "videoStatus",
    "alertLevel",
    "currentTime",
    "lastCommand",
    "commandAck",
    "remoteHostIp",
    "udpLinkState",
    "tcpLinkState",
    "heartbeatLinkState",
    "connectionSummary",
    "protocolLogText",
    "protocolConnected",
    "pathPoints",
    "waypointPoints"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSAppStateENDCLASS_t {
    uint offsetsAndSizes[154];
    char stringdata0[9];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[17];
    char stringdata5[12];
    char stringdata6[17];
    char stringdata7[20];
    char stringdata8[14];
    char stringdata9[5];
    char stringdata10[13];
    char stringdata11[15];
    char stringdata12[5];
    char stringdata13[2];
    char stringdata14[2];
    char stringdata15[2];
    char stringdata16[7];
    char stringdata17[16];
    char stringdata18[8];
    char stringdata19[7];
    char stringdata20[24];
    char stringdata21[7];
    char stringdata22[8];
    char stringdata23[8];
    char stringdata24[14];
    char stringdata25[16];
    char stringdata26[19];
    char stringdata27[13];
    char stringdata28[12];
    char stringdata29[13];
    char stringdata30[11];
    char stringdata31[15];
    char stringdata32[13];
    char stringdata33[15];
    char stringdata34[10];
    char stringdata35[14];
    char stringdata36[10];
    char stringdata37[7];
    char stringdata38[6];
    char stringdata39[10];
    char stringdata40[9];
    char stringdata41[15];
    char stringdata42[15];
    char stringdata43[9];
    char stringdata44[6];
    char stringdata45[10];
    char stringdata46[10];
    char stringdata47[10];
    char stringdata48[10];
    char stringdata49[10];
    char stringdata50[10];
    char stringdata51[8];
    char stringdata52[5];
    char stringdata53[6];
    char stringdata54[4];
    char stringdata55[17];
    char stringdata56[17];
    char stringdata57[17];
    char stringdata58[17];
    char stringdata59[17];
    char stringdata60[17];
    char stringdata61[13];
    char stringdata62[13];
    char stringdata63[12];
    char stringdata64[11];
    char stringdata65[12];
    char stringdata66[12];
    char stringdata67[11];
    char stringdata68[13];
    char stringdata69[13];
    char stringdata70[13];
    char stringdata71[19];
    char stringdata72[18];
    char stringdata73[16];
    char stringdata74[18];
    char stringdata75[11];
    char stringdata76[15];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSAppStateENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSAppStateENDCLASS_t qt_meta_stringdata_CLASSAppStateENDCLASS = {
    {
        QT_MOC_LITERAL(0, 8),  // "AppState"
        QT_MOC_LITERAL(9, 16),  // "telemetryChanged"
        QT_MOC_LITERAL(26, 0),  // ""
        QT_MOC_LITERAL(27, 11),  // "pathChanged"
        QT_MOC_LITERAL(39, 16),  // "commandTriggered"
        QT_MOC_LITERAL(56, 11),  // "commandName"
        QT_MOC_LITERAL(68, 16),  // "linkStateChanged"
        QT_MOC_LITERAL(85, 19),  // "linkSettingsChanged"
        QT_MOC_LITERAL(105, 13),  // "selectVehicle"
        QT_MOC_LITERAL(119, 4),  // "name"
        QT_MOC_LITERAL(124, 12),  // "issueCommand"
        QT_MOC_LITERAL(137, 14),  // "sendManualMove"
        QT_MOC_LITERAL(152, 4),  // "mode"
        QT_MOC_LITERAL(157, 1),  // "x"
        QT_MOC_LITERAL(159, 1),  // "y"
        QT_MOC_LITERAL(161, 1),  // "z"
        QT_MOC_LITERAL(163, 6),  // "yawDeg"
        QT_MOC_LITERAL(170, 15),  // "runScriptAction"
        QT_MOC_LITERAL(186, 7),  // "command"
        QT_MOC_LITERAL(194, 6),  // "target"
        QT_MOC_LITERAL(201, 23),  // "applyConnectionSettings"
        QT_MOC_LITERAL(225, 6),  // "hostIp"
        QT_MOC_LITERAL(232, 7),  // "udpPort"
        QT_MOC_LITERAL(240, 7),  // "tcpPort"
        QT_MOC_LITERAL(248, 13),  // "heartbeatPort"
        QT_MOC_LITERAL(262, 15),  // "connectProtocol"
        QT_MOC_LITERAL(278, 18),  // "disconnectProtocol"
        QT_MOC_LITERAL(297, 12),  // "testProtocol"
        QT_MOC_LITERAL(310, 11),  // "vehicleName"
        QT_MOC_LITERAL(322, 12),  // "flightStatus"
        QT_MOC_LITERAL(335, 10),  // "flightMode"
        QT_MOC_LITERAL(346, 14),  // "controllerMode"
        QT_MOC_LITERAL(361, 12),  // "controlState"
        QT_MOC_LITERAL(374, 14),  // "locationSource"
        QT_MOC_LITERAL(389, 9),  // "gpsStatus"
        QT_MOC_LITERAL(399, 13),  // "heartbeatLink"
        QT_MOC_LITERAL(413, 9),  // "videoLink"
        QT_MOC_LITERAL(423, 6),  // "rcLink"
        QT_MOC_LITERAL(430, 5),  // "armed"
        QT_MOC_LITERAL(436, 9),  // "connected"
        QT_MOC_LITERAL(446, 8),  // "failsafe"
        QT_MOC_LITERAL(455, 14),  // "batteryVoltage"
        QT_MOC_LITERAL(470, 14),  // "batteryPercent"
        QT_MOC_LITERAL(485, 8),  // "altitude"
        QT_MOC_LITERAL(494, 5),  // "speed"
        QT_MOC_LITERAL(500, 9),  // "positionX"
        QT_MOC_LITERAL(510, 9),  // "positionY"
        QT_MOC_LITERAL(520, 9),  // "positionZ"
        QT_MOC_LITERAL(530, 9),  // "velocityX"
        QT_MOC_LITERAL(540, 9),  // "velocityY"
        QT_MOC_LITERAL(550, 9),  // "velocityZ"
        QT_MOC_LITERAL(560, 7),  // "heading"
        QT_MOC_LITERAL(568, 4),  // "roll"
        QT_MOC_LITERAL(573, 5),  // "pitch"
        QT_MOC_LITERAL(579, 3),  // "yaw"
        QT_MOC_LITERAL(583, 16),  // "desiredPositionX"
        QT_MOC_LITERAL(600, 16),  // "desiredPositionY"
        QT_MOC_LITERAL(617, 16),  // "desiredPositionZ"
        QT_MOC_LITERAL(634, 16),  // "desiredVelocityX"
        QT_MOC_LITERAL(651, 16),  // "desiredVelocityY"
        QT_MOC_LITERAL(668, 16),  // "desiredVelocityZ"
        QT_MOC_LITERAL(685, 12),  // "homeDistance"
        QT_MOC_LITERAL(698, 12),  // "missionStage"
        QT_MOC_LITERAL(711, 11),  // "videoStatus"
        QT_MOC_LITERAL(723, 10),  // "alertLevel"
        QT_MOC_LITERAL(734, 11),  // "currentTime"
        QT_MOC_LITERAL(746, 11),  // "lastCommand"
        QT_MOC_LITERAL(758, 10),  // "commandAck"
        QT_MOC_LITERAL(769, 12),  // "remoteHostIp"
        QT_MOC_LITERAL(782, 12),  // "udpLinkState"
        QT_MOC_LITERAL(795, 12),  // "tcpLinkState"
        QT_MOC_LITERAL(808, 18),  // "heartbeatLinkState"
        QT_MOC_LITERAL(827, 17),  // "connectionSummary"
        QT_MOC_LITERAL(845, 15),  // "protocolLogText"
        QT_MOC_LITERAL(861, 17),  // "protocolConnected"
        QT_MOC_LITERAL(879, 10),  // "pathPoints"
        QT_MOC_LITERAL(890, 14)   // "waypointPoints"
    },
    "AppState",
    "telemetryChanged",
    "",
    "pathChanged",
    "commandTriggered",
    "commandName",
    "linkStateChanged",
    "linkSettingsChanged",
    "selectVehicle",
    "name",
    "issueCommand",
    "sendManualMove",
    "mode",
    "x",
    "y",
    "z",
    "yawDeg",
    "runScriptAction",
    "command",
    "target",
    "applyConnectionSettings",
    "hostIp",
    "udpPort",
    "tcpPort",
    "heartbeatPort",
    "connectProtocol",
    "disconnectProtocol",
    "testProtocol",
    "vehicleName",
    "flightStatus",
    "flightMode",
    "controllerMode",
    "controlState",
    "locationSource",
    "gpsStatus",
    "heartbeatLink",
    "videoLink",
    "rcLink",
    "armed",
    "connected",
    "failsafe",
    "batteryVoltage",
    "batteryPercent",
    "altitude",
    "speed",
    "positionX",
    "positionY",
    "positionZ",
    "velocityX",
    "velocityY",
    "velocityZ",
    "heading",
    "roll",
    "pitch",
    "yaw",
    "desiredPositionX",
    "desiredPositionY",
    "desiredPositionZ",
    "desiredVelocityX",
    "desiredVelocityY",
    "desiredVelocityZ",
    "homeDistance",
    "missionStage",
    "videoStatus",
    "alertLevel",
    "currentTime",
    "lastCommand",
    "commandAck",
    "remoteHostIp",
    "udpLinkState",
    "tcpLinkState",
    "heartbeatLinkState",
    "connectionSummary",
    "protocolLogText",
    "protocolConnected",
    "pathPoints",
    "waypointPoints"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSAppStateENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
      52,  135, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   92,    2, 0x06,   53 /* Public */,
       3,    0,   93,    2, 0x06,   54 /* Public */,
       4,    1,   94,    2, 0x06,   55 /* Public */,
       6,    0,   97,    2, 0x06,   57 /* Public */,
       7,    0,   98,    2, 0x06,   58 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       8,    1,   99,    2, 0x02,   59 /* Public */,
      10,    1,  102,    2, 0x02,   61 /* Public */,
      11,    5,  105,    2, 0x02,   63 /* Public */,
      17,    3,  116,    2, 0x02,   69 /* Public */,
      20,    4,  123,    2, 0x02,   73 /* Public */,
      25,    0,  132,    2, 0x02,   78 /* Public */,
      26,    0,  133,    2, 0x02,   79 /* Public */,
      27,    0,  134,    2, 0x02,   80 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,    9,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::QString, QMetaType::Double, QMetaType::Double, QMetaType::Double, QMetaType::Double,   12,   13,   14,   15,   16,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString,    9,   18,   19,
    QMetaType::Void, QMetaType::QString, QMetaType::Int, QMetaType::Int, QMetaType::Int,   21,   22,   23,   24,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,

 // properties: name, type, flags
      28, QMetaType::QString, 0x00015001, uint(0), 0,
      29, QMetaType::QString, 0x00015001, uint(0), 0,
      30, QMetaType::QString, 0x00015001, uint(0), 0,
      31, QMetaType::QString, 0x00015001, uint(0), 0,
      32, QMetaType::QString, 0x00015001, uint(0), 0,
      33, QMetaType::QString, 0x00015001, uint(0), 0,
      34, QMetaType::QString, 0x00015001, uint(0), 0,
      35, QMetaType::QString, 0x00015001, uint(0), 0,
      36, QMetaType::QString, 0x00015001, uint(0), 0,
      37, QMetaType::QString, 0x00015001, uint(0), 0,
      38, QMetaType::Bool, 0x00015001, uint(0), 0,
      39, QMetaType::Bool, 0x00015001, uint(0), 0,
      40, QMetaType::Bool, 0x00015001, uint(0), 0,
      41, QMetaType::Double, 0x00015001, uint(0), 0,
      42, QMetaType::Double, 0x00015001, uint(0), 0,
      43, QMetaType::Double, 0x00015001, uint(0), 0,
      44, QMetaType::Double, 0x00015001, uint(0), 0,
      45, QMetaType::Double, 0x00015001, uint(0), 0,
      46, QMetaType::Double, 0x00015001, uint(0), 0,
      47, QMetaType::Double, 0x00015001, uint(0), 0,
      48, QMetaType::Double, 0x00015001, uint(0), 0,
      49, QMetaType::Double, 0x00015001, uint(0), 0,
      50, QMetaType::Double, 0x00015001, uint(0), 0,
      51, QMetaType::Double, 0x00015001, uint(0), 0,
      52, QMetaType::Double, 0x00015001, uint(0), 0,
      53, QMetaType::Double, 0x00015001, uint(0), 0,
      54, QMetaType::Double, 0x00015001, uint(0), 0,
      55, QMetaType::Double, 0x00015001, uint(0), 0,
      56, QMetaType::Double, 0x00015001, uint(0), 0,
      57, QMetaType::Double, 0x00015001, uint(0), 0,
      58, QMetaType::Double, 0x00015001, uint(0), 0,
      59, QMetaType::Double, 0x00015001, uint(0), 0,
      60, QMetaType::Double, 0x00015001, uint(0), 0,
      61, QMetaType::Double, 0x00015001, uint(0), 0,
      62, QMetaType::QString, 0x00015001, uint(0), 0,
      63, QMetaType::QString, 0x00015001, uint(0), 0,
      64, QMetaType::QString, 0x00015001, uint(0), 0,
      65, QMetaType::QString, 0x00015001, uint(0), 0,
      66, QMetaType::QString, 0x00015001, uint(0), 0,
      67, QMetaType::QString, 0x00015001, uint(0), 0,
      68, QMetaType::QString, 0x00015001, uint(4), 0,
      22, QMetaType::Int, 0x00015001, uint(4), 0,
      23, QMetaType::Int, 0x00015001, uint(4), 0,
      24, QMetaType::Int, 0x00015001, uint(4), 0,
      69, QMetaType::QString, 0x00015001, uint(3), 0,
      70, QMetaType::QString, 0x00015001, uint(3), 0,
      71, QMetaType::QString, 0x00015001, uint(3), 0,
      72, QMetaType::QString, 0x00015001, uint(3), 0,
      73, QMetaType::QString, 0x00015001, uint(3), 0,
      74, QMetaType::Bool, 0x00015001, uint(3), 0,
      75, QMetaType::QVariantList, 0x00015001, uint(1), 0,
      76, QMetaType::QVariantList, 0x00015001, uint(1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject AppState::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSAppStateENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSAppStateENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSAppStateENDCLASS_t,
        // property 'vehicleName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'flightStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'flightMode'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'controllerMode'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'controlState'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'locationSource'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'gpsStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'heartbeatLink'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'videoLink'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'rcLink'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'armed'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'connected'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'failsafe'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'batteryVoltage'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'batteryPercent'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'altitude'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'speed'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'positionX'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'positionY'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'positionZ'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'velocityX'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'velocityY'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'velocityZ'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'heading'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'roll'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'pitch'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'yaw'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'desiredPositionX'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'desiredPositionY'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'desiredPositionZ'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'desiredVelocityX'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'desiredVelocityY'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'desiredVelocityZ'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'homeDistance'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'missionStage'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'videoStatus'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'alertLevel'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'currentTime'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'lastCommand'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'commandAck'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'remoteHostIp'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'udpPort'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'tcpPort'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'heartbeatPort'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'udpLinkState'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'tcpLinkState'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'heartbeatLinkState'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'connectionSummary'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'protocolLogText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'protocolConnected'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'pathPoints'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // property 'waypointPoints'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AppState, std::true_type>,
        // method 'telemetryChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'pathChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'commandTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'linkStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'linkSettingsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectVehicle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'issueCommand'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'sendManualMove'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'runScriptAction'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'applyConnectionSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'connectProtocol'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'disconnectProtocol'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'testProtocol'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void AppState::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AppState *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->telemetryChanged(); break;
        case 1: _t->pathChanged(); break;
        case 2: _t->commandTriggered((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->linkStateChanged(); break;
        case 4: _t->linkSettingsChanged(); break;
        case 5: _t->selectVehicle((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->issueCommand((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->sendManualMove((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[5]))); break;
        case 8: _t->runScriptAction((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3]))); break;
        case 9: _t->applyConnectionSettings((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 10: _t->connectProtocol(); break;
        case 11: _t->disconnectProtocol(); break;
        case 12: { bool _r = _t->testProtocol();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AppState::*)();
            if (_t _q_method = &AppState::telemetryChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AppState::*)();
            if (_t _q_method = &AppState::pathChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AppState::*)(const QString & );
            if (_t _q_method = &AppState::commandTriggered; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AppState::*)();
            if (_t _q_method = &AppState::linkStateChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (AppState::*)();
            if (_t _q_method = &AppState::linkSettingsChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    } else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<AppState *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->vehicleName(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->flightStatus(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->flightMode(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->controllerMode(); break;
        case 4: *reinterpret_cast< QString*>(_v) = _t->controlState(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->locationSource(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->gpsStatus(); break;
        case 7: *reinterpret_cast< QString*>(_v) = _t->heartbeatLink(); break;
        case 8: *reinterpret_cast< QString*>(_v) = _t->videoLink(); break;
        case 9: *reinterpret_cast< QString*>(_v) = _t->rcLink(); break;
        case 10: *reinterpret_cast< bool*>(_v) = _t->armed(); break;
        case 11: *reinterpret_cast< bool*>(_v) = _t->connected(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->failsafe(); break;
        case 13: *reinterpret_cast< double*>(_v) = _t->batteryVoltage(); break;
        case 14: *reinterpret_cast< double*>(_v) = _t->batteryPercent(); break;
        case 15: *reinterpret_cast< double*>(_v) = _t->altitude(); break;
        case 16: *reinterpret_cast< double*>(_v) = _t->speed(); break;
        case 17: *reinterpret_cast< double*>(_v) = _t->positionX(); break;
        case 18: *reinterpret_cast< double*>(_v) = _t->positionY(); break;
        case 19: *reinterpret_cast< double*>(_v) = _t->positionZ(); break;
        case 20: *reinterpret_cast< double*>(_v) = _t->velocityX(); break;
        case 21: *reinterpret_cast< double*>(_v) = _t->velocityY(); break;
        case 22: *reinterpret_cast< double*>(_v) = _t->velocityZ(); break;
        case 23: *reinterpret_cast< double*>(_v) = _t->heading(); break;
        case 24: *reinterpret_cast< double*>(_v) = _t->roll(); break;
        case 25: *reinterpret_cast< double*>(_v) = _t->pitch(); break;
        case 26: *reinterpret_cast< double*>(_v) = _t->yaw(); break;
        case 27: *reinterpret_cast< double*>(_v) = _t->desiredPositionX(); break;
        case 28: *reinterpret_cast< double*>(_v) = _t->desiredPositionY(); break;
        case 29: *reinterpret_cast< double*>(_v) = _t->desiredPositionZ(); break;
        case 30: *reinterpret_cast< double*>(_v) = _t->desiredVelocityX(); break;
        case 31: *reinterpret_cast< double*>(_v) = _t->desiredVelocityY(); break;
        case 32: *reinterpret_cast< double*>(_v) = _t->desiredVelocityZ(); break;
        case 33: *reinterpret_cast< double*>(_v) = _t->homeDistance(); break;
        case 34: *reinterpret_cast< QString*>(_v) = _t->missionStage(); break;
        case 35: *reinterpret_cast< QString*>(_v) = _t->videoStatus(); break;
        case 36: *reinterpret_cast< QString*>(_v) = _t->alertLevel(); break;
        case 37: *reinterpret_cast< QString*>(_v) = _t->currentTime(); break;
        case 38: *reinterpret_cast< QString*>(_v) = _t->lastCommand(); break;
        case 39: *reinterpret_cast< QString*>(_v) = _t->commandAck(); break;
        case 40: *reinterpret_cast< QString*>(_v) = _t->remoteHostIp(); break;
        case 41: *reinterpret_cast< int*>(_v) = _t->udpPort(); break;
        case 42: *reinterpret_cast< int*>(_v) = _t->tcpPort(); break;
        case 43: *reinterpret_cast< int*>(_v) = _t->heartbeatPort(); break;
        case 44: *reinterpret_cast< QString*>(_v) = _t->udpLinkState(); break;
        case 45: *reinterpret_cast< QString*>(_v) = _t->tcpLinkState(); break;
        case 46: *reinterpret_cast< QString*>(_v) = _t->heartbeatLinkState(); break;
        case 47: *reinterpret_cast< QString*>(_v) = _t->connectionSummary(); break;
        case 48: *reinterpret_cast< QString*>(_v) = _t->protocolLogText(); break;
        case 49: *reinterpret_cast< bool*>(_v) = _t->protocolConnected(); break;
        case 50: *reinterpret_cast< QVariantList*>(_v) = _t->pathPoints(); break;
        case 51: *reinterpret_cast< QVariantList*>(_v) = _t->waypointPoints(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *AppState::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AppState::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSAppStateENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AppState::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 52;
    }
    return _id;
}

// SIGNAL 0
void AppState::telemetryChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void AppState::pathChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void AppState::commandTriggered(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void AppState::linkStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void AppState::linkSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
QT_WARNING_POP
