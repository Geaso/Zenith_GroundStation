#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

struct ParamEntry {
    QString name;
    QString value;
    int type = 0; // 1=INT,2=LONG,3=FLOAT,4=DOUBLE,5=STRING,6=BOOLEAN
    int module = 0;
};

class ParamStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY paramsChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, ValueRole, TypeRole, ModuleRole };

    explicit ParamStore(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void applyParamSettings(const QVariantMap &payload);

    Q_INVOKABLE void setValue(int row, const QString &value);
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString valueAt(int row) const;
    Q_INVOKABLE int typeAt(int row) const;

    QList<ParamEntry> dirtyEntries() const;
    void clearDirty();

signals:
    void paramsChanged();

private:
    QList<ParamEntry> m_params;
    QSet<int> m_dirty;
};
