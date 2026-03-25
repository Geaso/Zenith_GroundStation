/****************************************************************************
** Meta object code from reading C++ file 'ZenithProtocolProfile.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.6.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ZenithProtocolProfile.h"
#include <QtCore/qmetatype.h>

#if __has_include(<QtCore/qtmochelpers.h>)
#include <QtCore/qtmochelpers.h>
#else
QT_BEGIN_MOC_NAMESPACE
#endif


#include <memory>

#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ZenithProtocolProfile.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS = QtMocHelpers::stringData(
    "ZenithProtocolProfile",
    "stateTopic",
    "",
    "vehicleName",
    "commandTopic",
    "controlStateTopic",
    "textInfoTopic",
    "protocolName",
    "namespacePrefix",
    "multicastIp",
    "groundStationIp",
    "udpPort",
    "tcpPort",
    "tcpHeartbeatPort",
    "servicePort",
    "messageTypes"
);
#else  // !QT_MOC_HAS_STRING_DATA
struct qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS_t {
    uint offsetsAndSizes[32];
    char stringdata0[22];
    char stringdata1[11];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[13];
    char stringdata5[18];
    char stringdata6[14];
    char stringdata7[13];
    char stringdata8[16];
    char stringdata9[12];
    char stringdata10[16];
    char stringdata11[8];
    char stringdata12[8];
    char stringdata13[17];
    char stringdata14[12];
    char stringdata15[13];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS_t qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS = {
    {
        QT_MOC_LITERAL(0, 21),  // "ZenithProtocolProfile"
        QT_MOC_LITERAL(22, 10),  // "stateTopic"
        QT_MOC_LITERAL(33, 0),  // ""
        QT_MOC_LITERAL(34, 11),  // "vehicleName"
        QT_MOC_LITERAL(46, 12),  // "commandTopic"
        QT_MOC_LITERAL(59, 17),  // "controlStateTopic"
        QT_MOC_LITERAL(77, 13),  // "textInfoTopic"
        QT_MOC_LITERAL(91, 12),  // "protocolName"
        QT_MOC_LITERAL(104, 15),  // "namespacePrefix"
        QT_MOC_LITERAL(120, 11),  // "multicastIp"
        QT_MOC_LITERAL(132, 15),  // "groundStationIp"
        QT_MOC_LITERAL(148, 7),  // "udpPort"
        QT_MOC_LITERAL(156, 7),  // "tcpPort"
        QT_MOC_LITERAL(164, 16),  // "tcpHeartbeatPort"
        QT_MOC_LITERAL(181, 11),  // "servicePort"
        QT_MOC_LITERAL(193, 12)   // "messageTypes"
    },
    "ZenithProtocolProfile",
    "stateTopic",
    "",
    "vehicleName",
    "commandTopic",
    "controlStateTopic",
    "textInfoTopic",
    "protocolName",
    "namespacePrefix",
    "multicastIp",
    "groundStationIp",
    "udpPort",
    "tcpPort",
    "tcpHeartbeatPort",
    "servicePort",
    "messageTypes"
};
#undef QT_MOC_LITERAL
#endif // !QT_MOC_HAS_STRING_DATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSZenithProtocolProfileENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       9,   50, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   38,    2, 0x102,   10 /* Public | MethodIsConst  */,
       4,    1,   41,    2, 0x102,   12 /* Public | MethodIsConst  */,
       5,    1,   44,    2, 0x102,   14 /* Public | MethodIsConst  */,
       6,    1,   47,    2, 0x102,   16 /* Public | MethodIsConst  */,

 // methods: parameters
    QMetaType::QString, QMetaType::QString,    3,
    QMetaType::QString, QMetaType::QString,    3,
    QMetaType::QString, QMetaType::QString,    3,
    QMetaType::QString, QMetaType::QString,    3,

 // properties: name, type, flags
       7, QMetaType::QString, 0x00015401, uint(-1), 0,
       8, QMetaType::QString, 0x00015401, uint(-1), 0,
       9, QMetaType::QString, 0x00015401, uint(-1), 0,
      10, QMetaType::QString, 0x00015401, uint(-1), 0,
      11, QMetaType::Int, 0x00015401, uint(-1), 0,
      12, QMetaType::Int, 0x00015401, uint(-1), 0,
      13, QMetaType::Int, 0x00015401, uint(-1), 0,
      14, QMetaType::Int, 0x00015401, uint(-1), 0,
      15, QMetaType::QVariantList, 0x00015401, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject ZenithProtocolProfile::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSZenithProtocolProfileENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS_t,
        // property 'protocolName'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'namespacePrefix'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'multicastIp'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'groundStationIp'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'udpPort'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'tcpPort'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'tcpHeartbeatPort'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'servicePort'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'messageTypes'
        QtPrivate::TypeAndForceComplete<QVariantList, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ZenithProtocolProfile, std::true_type>,
        // method 'stateTopic'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'commandTopic'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'controlStateTopic'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'textInfoTopic'
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ZenithProtocolProfile::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ZenithProtocolProfile *>(_o);
        (void)_t;
        switch (_id) {
        case 0: { QString _r = _t->stateTopic((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 1: { QString _r = _t->commandTopic((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 2: { QString _r = _t->controlStateTopic((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        case 3: { QString _r = _t->textInfoTopic((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QString*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::ReadProperty) {
        auto *_t = static_cast<ZenithProtocolProfile *>(_o);
        (void)_t;
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->protocolName(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->namespacePrefix(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->multicastIp(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->groundStationIp(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->udpPort(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->tcpPort(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->tcpHeartbeatPort(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->servicePort(); break;
        case 8: *reinterpret_cast< QVariantList*>(_v) = _t->messageTypes(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    } else if (_c == QMetaObject::BindableProperty) {
    }
}

const QMetaObject *ZenithProtocolProfile::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ZenithProtocolProfile::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSZenithProtocolProfileENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ZenithProtocolProfile::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    return _id;
}
QT_WARNING_POP
