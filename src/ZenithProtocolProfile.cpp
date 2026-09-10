#include "ZenithProtocolProfile.h"

#include "ZenithProtocol.h"

ZenithProtocolProfile::ZenithProtocolProfile(QObject *parent)
    : QObject(parent)
{
}

QString ZenithProtocolProfile::protocolName() const
{
    return "Zenith MAVLink 2";
}

QString ZenithProtocolProfile::namespacePrefix() const
{
    return ZenithProtocol::kTopicPrefix;
}

QString ZenithProtocolProfile::multicastIp() const
{
    return "224.0.0.88";
}

QString ZenithProtocolProfile::groundStationIp() const
{
    return "127.0.0.1";
}

int ZenithProtocolProfile::udpPort() const
{
    return ZenithProtocol::kUdpPort;
}

int ZenithProtocolProfile::tcpPort() const
{
    return ZenithProtocol::kTcpPort;
}

int ZenithProtocolProfile::tcpHeartbeatPort() const
{
    return ZenithProtocol::kTcpHeartbeatPort;
}

int ZenithProtocolProfile::servicePort() const
{
    return 20168;
}

QVariantList ZenithProtocolProfile::messageTypes() const
{
    QVariantList list;
    const struct Item {
        int id;
        const char *name;
        const char *direction;
    } items[] = {
        {0, "HEARTBEAT", "Bidirectional"},
        {1, "SYS_STATUS", "Vehicle -> Ground"},
        {24, "GPS_RAW_INT", "Vehicle -> Ground"},
        {30, "ATTITUDE", "Vehicle -> Ground"},
        {32, "LOCAL_POSITION_NED", "Vehicle -> Ground"},
        {33, "GLOBAL_POSITION_INT", "Vehicle -> Ground"},
        {147, "BATTERY_STATUS", "Vehicle -> Ground"},
        {248, "V2_EXTENSION (Zenith)", "Bidirectional"},
        {253, "STATUSTEXT", "Vehicle -> Ground"}
    };

    for (const Item &item : items) {
        QVariantMap map;
        map.insert("id", item.id);
        map.insert("name", QString::fromLatin1(item.name));
        map.insert("direction", QString::fromLatin1(item.direction));
        list.append(map);
    }
    return list;
}

QString ZenithProtocolProfile::stateTopic(const QString &vehicleName) const
{
    return "/" + vehicleName.toLower() + ZenithProtocol::kTopicPrefix + "/state";
}

QString ZenithProtocolProfile::commandTopic(const QString &vehicleName) const
{
    return "/" + vehicleName.toLower() + ZenithProtocol::kTopicPrefix + "/command";
}

QString ZenithProtocolProfile::controlStateTopic(const QString &vehicleName) const
{
    return "/" + vehicleName.toLower() + ZenithProtocol::kTopicPrefix + "/control_state";
}

QString ZenithProtocolProfile::textInfoTopic(const QString &vehicleName) const
{
    return "/" + vehicleName.toLower() + ZenithProtocol::kTopicPrefix + "/text_info";
}
