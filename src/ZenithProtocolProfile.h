#pragma once

#include <QObject>
#include <QVariantList>

class ZenithProtocolProfile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString protocolName READ protocolName CONSTANT)
    Q_PROPERTY(QString namespacePrefix READ namespacePrefix CONSTANT)
    Q_PROPERTY(QString multicastIp READ multicastIp CONSTANT)
    Q_PROPERTY(QString groundStationIp READ groundStationIp CONSTANT)
    Q_PROPERTY(int udpPort READ udpPort CONSTANT)
    Q_PROPERTY(int tcpPort READ tcpPort CONSTANT)
    Q_PROPERTY(int tcpHeartbeatPort READ tcpHeartbeatPort CONSTANT)
    Q_PROPERTY(int servicePort READ servicePort CONSTANT)
    Q_PROPERTY(QVariantList messageTypes READ messageTypes CONSTANT)
public:
    explicit ZenithProtocolProfile(QObject *parent = nullptr);

    QString protocolName() const;
    QString namespacePrefix() const;
    QString multicastIp() const;
    QString groundStationIp() const;
    int udpPort() const;
    int tcpPort() const;
    int tcpHeartbeatPort() const;
    int servicePort() const;
    QVariantList messageTypes() const;

    Q_INVOKABLE QString stateTopic(const QString &vehicleName) const;
    Q_INVOKABLE QString commandTopic(const QString &vehicleName) const;
    Q_INVOKABLE QString controlStateTopic(const QString &vehicleName) const;
    Q_INVOKABLE QString textInfoTopic(const QString &vehicleName) const;
};
