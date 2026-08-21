#pragma once

#include <QAbstractListModel>

struct ScriptActionItem
{
    QString name;
    QString command;
    QString taskPath;
    QString note;
    QString category;
    QString status;
    QString lastRun;
    bool builtIn = false;
};

class ScriptActionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int builtInCount READ builtInCount NOTIFY countChanged)
    Q_PROPERTY(int customCount READ customCount NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString customConfigPath READ customConfigPath CONSTANT)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        CommandRole,
        TaskPathRole,
        NoteRole,
        CategoryRole,
        StatusRole,
        LastRunRole,
        BuiltInRole
    };

    explicit ScriptActionModel(QObject *parent = nullptr);
    ScriptActionModel(const QString &builtInConfigPath,
                      const QString &customConfigPath,
                      QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const { return m_items.size(); }
    int builtInCount() const;
    int customCount() const;
    QString lastError() const { return m_lastError; }
    QString customConfigPath() const { return m_customConfigPath; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool addCustomAction(const QString &name,
                                     const QString &taskId,
                                     const QString &taskPath,
                                     const QString &note);
    Q_INVOKABLE bool updateCustomAction(int row,
                                        const QString &name,
                                        const QString &taskId,
                                        const QString &taskPath,
                                        const QString &note);
    Q_INVOKABLE bool removeCustomAction(int row);
    Q_INVOKABLE bool isTaskIdAvailable(const QString &taskId, int exceptRow = -1) const;

signals:
    void countChanged();
    void lastErrorChanged();

private:
    bool loadBuiltIns(const QString &path);
    bool loadCustomActions(const QString &path);
    bool saveCustomActions(const QList<ScriptActionItem> &items);
    bool validateCustomAction(const QString &name,
                              const QString &taskId,
                              const QString &taskPath,
                              const QString &note,
                              int exceptRow);
    void setLastError(const QString &error);

    QList<ScriptActionItem> m_items;
    QString m_customConfigPath;
    QString m_lastError;
};
