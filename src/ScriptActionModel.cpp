#include "ScriptActionModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

namespace {

// 任务卡片清单外置在 exe 同级的 config/script_actions.json。文件只保存
// 稳定 task_id；真正的 roslaunch argv 在机载白名单中，地面站不下发 shell。
QString scriptConfigPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("config/script_actions.json");
}

} // namespace

ScriptActionModel::ScriptActionModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadFromFile(scriptConfigPath());
}

bool ScriptActionModel::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("未找到任务清单 %s，任务页将为空", qUtf8Printable(path));
        return false;
    }

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning("任务清单 %s 解析失败: %s", qUtf8Printable(path), qUtf8Printable(err.errorString()));
        return false;
    }

    QList<ScriptActionItem> parsed;
    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject o = v.toObject();
        const QString name = o.value("name").toString();
        const QString command = o.value("task_id").toString();
        if (name.isEmpty() || command.isEmpty()) {
            continue;
        }
        parsed.append({
            name,
            command,
            QStringLiteral("Managed Task API"),
            o.value("note").toString(),
            o.value("category").toString(QStringLiteral("Custom")),
            QStringLiteral("Ready"),
            QString()
        });
    }

    beginResetModel();
    m_items = parsed;
    endResetModel();
    emit countChanged();
    return true;
}

int ScriptActionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ScriptActionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const ScriptActionItem &item = m_items.at(index.row());
    switch (role) {
    case NameRole:
        return item.name;
    case CommandRole:
        return item.command;
    case TargetRole:
        return item.target;
    case NoteRole:
        return item.note;
    case CategoryRole:
        return item.category;
    case StatusRole:
        return item.status;
    case LastRunRole:
        return item.lastRun;
    default:
        return {};
    }
}

QHash<int, QByteArray> ScriptActionModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {CommandRole, "command"},
        {TargetRole, "target"},
        {NoteRole, "note"},
        {CategoryRole, "category"},
        {StatusRole, "status"},
        {LastRunRole, "lastRun"}
    };
}

void ScriptActionModel::addAction(const QString &name, const QString &command, const QString &target)
{
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.append({name, command, target, "User defined action", "Custom", "Ready", "Just Created"});
    endInsertRows();
    emit countChanged();
}

void ScriptActionModel::removeAction(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void ScriptActionModel::updateCommand(int row, const QString &newCommand)
{
    if (row < 0 || row >= m_items.size()) return;
    m_items[row].command = newCommand;
    emit dataChanged(index(row), index(row), {CommandRole});
}
