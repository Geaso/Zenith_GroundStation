#pragma once

#include <QAbstractListModel>

struct ScriptActionItem
{
    QString name;
    QString command;
    QString target;
    QString note;
    QString category;
    QString status;
    QString lastRun;
};

class ScriptActionModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        CommandRole,
        TargetRole,
        NoteRole,
        CategoryRole,
        StatusRole,
        LastRunRole
    };

    explicit ScriptActionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const { return m_items.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addAction(const QString &name, const QString &command, const QString &target);
    Q_INVOKABLE void removeAction(int row);
    Q_INVOKABLE void updateCommand(int row, const QString &newCommand);

    // 从外置 JSON 载入任务清单，成功返回 true
    bool loadFromFile(const QString &path);

signals:
    void countChanged();

private:
    QList<ScriptActionItem> m_items;
};
