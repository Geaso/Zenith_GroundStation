#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariantMap>

struct RadioPairingItem
{
    QString name;
    int radioAddress = 0;
    int uavId = 0;
    QString note;
    bool builtIn = false;
    bool overridden = false;
};

class RadioPairingModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int builtInCount READ builtInCount NOTIFY countChanged)
    Q_PROPERTY(int customCount READ customCount NOTIFY countChanged)
    Q_PROPERTY(int overrideCount READ overrideCount NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString userConfigPath READ userConfigPath CONSTANT)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        RadioAddressRole,
        UavIdRole,
        NoteRole,
        BuiltInRole,
        OverriddenRole,
        CanDeleteRole
    };

    explicit RadioPairingModel(QObject *parent = nullptr);
    RadioPairingModel(const QString &builtInConfigPath,
                      const QString &userConfigPath,
                      QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const { return m_items.size(); }
    int builtInCount() const;
    int customCount() const;
    int overrideCount() const;
    QString lastError() const { return m_lastError; }
    QString userConfigPath() const { return m_userConfigPath; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool addPairing(const QString &name,
                                int radioAddress,
                                int uavId,
                                const QString &note = QString());
    Q_INVOKABLE bool updatePairing(int row,
                                   const QString &name,
                                   int radioAddress,
                                   int uavId,
                                   const QString &note = QString());
    Q_INVOKABLE bool removePairing(int row);

    // Removing an override never removes the read-only built-in item. It reveals
    // the values from config/radio_pairings.json again.
    Q_INVOKABLE bool restoreBuiltIn(int row);

    // AppState can persist the selected name and resolve it after model loading.
    Q_INVOKABLE int indexOfName(const QString &name) const;
    Q_INVOKABLE QVariantMap pairingAt(int row) const;
    Q_INVOKABLE QVariantMap pairingByName(const QString &name) const;
    Q_INVOKABLE bool isNameAvailable(const QString &name, int exceptRow = -1) const;

signals:
    void countChanged();
    void lastErrorChanged();

private:
    bool loadBuiltIns(const QString &path);
    bool loadUserPairings(const QString &path);
    bool saveUserPairings(const QList<RadioPairingItem> &items);
    bool validatePairing(const QString &name,
                         int radioAddress,
                         int uavId,
                         const QString &note,
                         int exceptRow);
    void rebuildMergedItems();
    int userEntryIndex(const QString &name) const;
    int builtInEntryIndex(const QString &name) const;
    void setLastError(const QString &error);

    QList<RadioPairingItem> m_builtInItems;
    QList<RadioPairingItem> m_userItems;
    QList<RadioPairingItem> m_items;
    QString m_userConfigPath;
    QString m_lastError;
};
