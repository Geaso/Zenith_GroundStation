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
    "selectVehicle",
    "name",
    "issueCommand",
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
    "heading",
    "roll",
    "pitch",
    "yaw",
    "homeDistance",
    "missionStage",
    "videoStatus",
    "alertLevel",
    "currentTime",
    "lastCommand",
    "commandAck",
    "pathPoints",
    "waypointPoints"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSAppStateENDCLASS_t {
    uint offsetsAndSizes[78];
    char stringdata0[9];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[17];
    char stringdata5[12];
    char stringdata6[14];
    char stringdata7[5];
    char stringdata8[13];
    char stringdata9[12];
    char stringdata10[13];
    char stringdata11[11];
    char stringdata12[15];
    char stringdata13[13];
    char stringdata14[15];
    char stringdata15[10];
    char stringdata16[14];
    char stringdata17[10];
    char stringdata18[7];
    char stringdata19[6];
    char stringdata20[10];
    char stringdata21[9];
    char stringdata22[15];
    char stringdata23[15];
    char stringdata24[9];
    char stringdata25[6];
    char stringdata26[8];
    char stringdata27[5];
    char stringdata28[6];
    char stringdata29[4];
    char stringdata30[13];
    char stringdata31[13];
    char stringdata32[12];
    char stringdata33[11];
    char stringdata34[12];
    char stringdata35[12];
    char stringdata36[11];
    char stringdata37[11];
    char stringdata38[15];
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
        QT_MOC_LITERAL(68, 13),  // "selectVehicle"
        QT_MOC_LITERAL(82, 4),  // "name"
        QT_MOC_LITERAL(87, 12),  // "issueCommand"
        QT_MOC_LITERAL(100, 11),  // "vehicleName"
        QT_MOC_LITERAL(112, 12),  // "flightStatus"
        QT_MOC_LITERAL(125, 10),  // "flightMode"
        QT_MOC_LITERAL(136, 14),  // "controllerMode"
        QT_MOC_LITERAL(151, 12),  // "controlState"
        QT_MOC_LITERAL(164, 14),  // "locationSource"
        QT_MOC_LITERAL(179, 9),  // "gpsStatus"
        QT_MOC_LITERAL(189, 13),  // "heartbeatLink"
        QT_MOC_LITERAL(203, 9),  // "videoLink"
        QT_MOC_LITERAL(213, 6),  // "rcLink"
        QT_MOC_LITERAL(220, 5),  // "armed"
        QT_MOC_LITERAL(226, 9),  // "connected"
        QT_MOC_LITERAL(236, 8),  // "failsafe"
        QT_MOC_LITERAL(245, 14),  // "batteryVoltage"
        QT_MOC_LITERAL(260, 14),  // "batteryPercent"
        QT_MOC_LITERAL(275, 8),  // "altitude"
        QT_MOC_LITERAL(284, 5),  // "speed"
        QT_MOC_LITERAL(290, 7),  // "heading"
        QT_MOC_LITERAL(298, 4),  // "roll"
        QT_MOC_LITERAL(303, 5),  // "pitch"
        QT_MOC_LITERAL(309, 3),  // "yaw"
        QT_MOC_LITERAL(313, 12),  // "homeDistance"
        QT_MOC_LITERAL(326, 12),  // "missionStage"
        QT_MOC_LITERAL(339, 11),  // "videoStatus"
        QT_MOC_LITERAL(351, 10),  // "alertLevel"
        QT_MOC_LITERAL(362, 11),  // "currentTime"
        QT_MOC_LITERAL(374, 11),  // "lastCommand"
        QT_MOC_LITERAL(386, 10),  // "commandAck"
        QT_MOC_LITERAL(397, 10),  // "pathPoints"
        QT_MOC_LITERAL(408, 14)   // "waypointPoints"
    },
    "AppState",
    "telemetryChanged",
    "",
    "pathChanged",
    "commandTriggered",
    "commandName",
    "selectVehicle",
    "name",
    "issueCommand",
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
    "heading",
    "roll",
    "pitch",
    "yaw",
    "homeDistance",
    "missionStage",
    "videoStatus",
    "alertLevel",
    "currentTime",
    "lastCommand",
    "commandAck",
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
       5,   14, // methods
      30,   55, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   44,    2, 0x06,   31 /* Public */,
       3,    0,   45,    2, 0x06,   32 /* Public */,
       4,    1,   46,    2, 0x06,   33 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       6,    1,   49,    2, 0x02,   35 /* Public */,
       8,    1,   52,    2, 0x02,   37 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,

 // methods: parameters
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::QString,    5,

 // properties: name, type, flags
       9, QMetaType::QString, 0x00015001, uint(0), 0,
      10, QMetaType::QString, 0x00015001, uint(0), 0,
      11, QMetaType::QString, 0x00015001, uint(0), 0,
      12, QMetaType::QString, 0x00015001, uint(0), 0,
      13, QMetaType::QString, 0x00015001, uint(0), 0,
      14, QMetaType::QString, 0x00015001, uint(0), 0,
      15, QMetaType::QString, 0x00015001, uint(0), 0,
      16, QMetaType::QString, 0x00015001, uint(0), 0,
      17, QMetaType::QString, 0x00015001, uint(0), 0,
      18, QMetaType::QString, 0x00015001, uint(0), 0,
      19, QMetaType::Bool, 0x00015001, uint(0), 0,
      20, QMetaType::Bool, 0x00015001, uint(0), 0,
      21, QMetaType::Bool, 0x00015001, uint(0), 0,
      22, QMetaType::Double, 0x00015001, uint(0), 0,
      23, QMetaType::Double, 0x00015001, uint(0), 0,
      24, QMetaType::Double, 0x00015001, uint(0), 0,
      25, QMetaType::Double, 0x00015001, uint(0), 0,
      26, QMetaType::Double, 0x00015001, uint(0), 0,
      27, QMetaType::Double, 0x00015001, uint(0), 0,
      28, QMetaType::Double, 0x00015001, uint(0), 0,
      29, QMetaType::Double, 0x00015001, uint(0), 0,
      30, QMetaType::Double, 0x00015001, uint(0), 0,
      31, QMetaType::QString, 0x00015001, uint(0), 0,
      32, QMetaType::QString, 0x00015001, uint(0), 0,
      33, QMetaType::QString, 0x00015001, uint(0), 0,
      34, QMetaType::QString, 0x00015001, uint(0), 0,
      35, QMetaType::QString, 0x00015001, uint(0), 0,
      36, QMetaType::QString, 0x00015001, uint(0), 0,
      37, QMetaType::QVariantList, 0x00015001, uint(1), 0,
      38, QMetaType::QVariantList, 0x00015001, uint(1), 0,

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
        // property 'heading'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'roll'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'pitch'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'yaw'
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
        // method 'selectVehicle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'issueCommand'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
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
        case 3: _t->selectVehicle((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->issueCommand((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
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
        case 17: *reinterpret_cast< double*>(_v) = _t->heading(); break;
        case 18: *reinterpret_cast< double*>(_v) = _t->roll(); break;
        case 19: *reinterpret_cast< double*>(_v) = _t->pitch(); break;
        case 20: *reinterpret_cast< double*>(_v) = _t->yaw(); break;
        case 21: *reinterpret_cast< double*>(_v) = _t->homeDistance(); break;
        case 22: *reinterpret_cast< QString*>(_v) = _t->missionStage(); break;
        case 23: *reinterpret_cast< QString*>(_v) = _t->videoStatus(); break;
        case 24: *reinterpret_cast< QString*>(_v) = _t->alertLevel(); break;
        case 25: *reinterpret_cast< QString*>(_v) = _t->currentTime(); break;
        case 26: *reinterpret_cast< QString*>(_v) = _t->lastCommand(); break;
        case 27: *reinterpret_cast< QString*>(_v) = _t->commandAck(); break;
        case 28: *reinterpret_cast< QVariantList*>(_v) = _t->pathPoints(); break;
        case 29: *reinterpret_cast< QVariantList*>(_v) = _t->waypointPoints(); break;
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
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 30;
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
QT_WARNING_POP
