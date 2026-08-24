#include "ScriptActionModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QString builtInTaskConfigPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("config/script_actions.json"));
}

QString userTaskConfigPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("custom_tasks.json"));
}

const QString &customTaskRoot()
{
    static const QString root =
        QStringLiteral("/home/jetson/task_ws/src/user_tasks/");
    return root;
}

bool isAllowedCustomTaskPath(const QString &path)
{
    return path.startsWith(customTaskRoot());
}

ScriptActionItem customItemFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("name")).toString().trimmed(),
        object.value(QStringLiteral("task_id")).toString().trimmed(),
        object.value(QStringLiteral("task_path")).toString().trimmed(),
        object.value(QStringLiteral("note")).toString().trimmed(),
        QStringLiteral("自定义"),
        QStringLiteral("Ready"),
        QString(),
        false
    };
}

QJsonObject customItemToJson(const ScriptActionItem &item)
{
    return {
        {QStringLiteral("name"), item.name},
        {QStringLiteral("task_id"), item.command},
        {QStringLiteral("task_path"), item.taskPath},
        {QStringLiteral("note"), item.note}
    };
}

} // namespace

ScriptActionModel::ScriptActionModel(QObject *parent)
    : ScriptActionModel(builtInTaskConfigPath(), userTaskConfigPath(), parent)
{
}

ScriptActionModel::ScriptActionModel(const QString &builtInConfigPath,
                                     const QString &customConfigPath,
                                     QObject *parent)
    : QAbstractListModel(parent),
      m_customConfigPath(customConfigPath)
{
    loadBuiltIns(builtInConfigPath);
    loadCustomActions(customConfigPath);
}

bool ScriptActionModel::loadBuiltIns(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("未找到内置任务清单：%1").arg(path));
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        setLastError(QStringLiteral("内置任务清单解析失败：%1").arg(error.errorString()));
        return false;
    }

    for (const QJsonValue &value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const QString name = object.value(QStringLiteral("name")).toString().trimmed();
        const QString taskId = object.value(QStringLiteral("task_id")).toString().trimmed();
        if (name.isEmpty() || taskId.isEmpty()) {
            continue;
        }
        // 内置任务可以带 task_path: 机载任务管理器的注册表里没有的任务（比如 EGO
        // 那条 launch）必须靠绝对路径启动。留空则按注册表里的 task_id 启动。
        m_items.append({
            name,
            taskId,
            object.value(QStringLiteral("task_path")).toString().trimmed(),
            object.value(QStringLiteral("note")).toString(),
            object.value(QStringLiteral("category")).toString(QStringLiteral("任务")),
            QStringLiteral("Ready"),
            QString(),
            true
        });
    }
    return true;
}

bool ScriptActionModel::loadCustomActions(const QString &path)
{
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("无法读取自定义任务库：%1").arg(path));
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        setLastError(QStringLiteral("自定义任务库解析失败：%1").arg(error.errorString()));
        return false;
    }

    const QJsonArray tasks = document.object().value(QStringLiteral("tasks")).toArray();
    for (const QJsonValue &value : tasks) {
        if (!value.isObject()) {
            continue;
        }
        ScriptActionItem item = customItemFromJson(value.toObject());
        const bool valid = !item.name.isEmpty()
            && !item.command.isEmpty()
            && isAllowedCustomTaskPath(item.taskPath)
            && isTaskIdAvailable(item.command);
        if (valid) {
            m_items.append(item);
        }
    }
    return true;
}

int ScriptActionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int ScriptActionModel::builtInCount() const
{
    int result = 0;
    for (const ScriptActionItem &item : m_items) {
        result += item.builtIn ? 1 : 0;
    }
    return result;
}

int ScriptActionModel::customCount() const
{
    return count() - builtInCount();
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
    case TaskPathRole:
        return item.taskPath;
    case NoteRole:
        return item.note;
    case CategoryRole:
        return item.category;
    case StatusRole:
        return item.status;
    case LastRunRole:
        return item.lastRun;
    case BuiltInRole:
        return item.builtIn;
    default:
        return {};
    }
}

QHash<int, QByteArray> ScriptActionModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {CommandRole, "command"},
        {TaskPathRole, "taskPath"},
        {NoteRole, "note"},
        {CategoryRole, "category"},
        {StatusRole, "status"},
        {LastRunRole, "lastRun"},
        {BuiltInRole, "builtIn"}
    };
}

bool ScriptActionModel::isTaskIdAvailable(const QString &taskId, int exceptRow) const
{
    const QString normalizedId = taskId.trimmed();
    for (int row = 0; row < m_items.size(); ++row) {
        if (row != exceptRow && m_items.at(row).command == normalizedId) {
            return false;
        }
    }
    return true;
}

bool ScriptActionModel::validateCustomAction(const QString &name,
                                             const QString &taskId,
                                             const QString &taskPath,
                                             const QString &note,
                                             int exceptRow)
{
    static const QRegularExpression taskIdPattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$"));
    const QString normalizedName = name.trimmed();
    const QString normalizedId = taskId.trimmed();
    const QString normalizedPath = taskPath.trimmed();

    if (normalizedName.isEmpty() || normalizedName.size() > 64) {
        setLastError(QStringLiteral("任务名称不能为空且最多 64 个字符"));
        return false;
    }
    if (!taskIdPattern.match(normalizedId).hasMatch()) {
        setLastError(QStringLiteral("任务 ID 只能包含字母、数字、点、下划线和短横线（最多 64 字符）"));
        return false;
    }
    if (!isTaskIdAvailable(normalizedId, exceptRow)) {
        setLastError(QStringLiteral("任务 ID 已存在，请使用唯一 ID"));
        return false;
    }
    if (!isAllowedCustomTaskPath(normalizedPath)
        || normalizedPath.size() > 512
        || normalizedPath.contains(QLatin1Char('\n'))
        || normalizedPath.contains(QLatin1Char('\r'))) {
        setLastError(QStringLiteral(
            "任务路径必须位于 /home/jetson/task_ws/src/user_tasks/ 且不超过 512 字符"));
        return false;
    }
    if (note.size() > 200) {
        setLastError(QStringLiteral("任务说明最多 200 个字符"));
        return false;
    }
    setLastError(QString());
    return true;
}

bool ScriptActionModel::saveCustomActions(const QList<ScriptActionItem> &items)
{
    const QFileInfo fileInfo(m_customConfigPath);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        setLastError(QStringLiteral("无法创建任务库目录：%1").arg(fileInfo.absolutePath()));
        return false;
    }

    QJsonArray tasks;
    for (const ScriptActionItem &item : items) {
        if (!item.builtIn) {
            tasks.append(customItemToJson(item));
        }
    }
    QJsonObject root{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("tasks"), tasks}
    };

    QSaveFile file(m_customConfigPath);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(QStringLiteral("无法写入自定义任务库：%1").arg(m_customConfigPath));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setLastError(QStringLiteral("保存自定义任务库失败：%1").arg(m_customConfigPath));
        return false;
    }
    setLastError(QString());
    return true;
}

bool ScriptActionModel::addCustomAction(const QString &name,
                                        const QString &taskId,
                                        const QString &taskPath,
                                        const QString &note)
{
    if (!validateCustomAction(name, taskId, taskPath, note, -1)) {
        return false;
    }

    ScriptActionItem item{
        name.trimmed(),
        taskId.trimmed(),
        taskPath.trimmed(),
        note.trimmed(),
        QStringLiteral("自定义"),
        QStringLiteral("Ready"),
        QString(),
        false
    };
    QList<ScriptActionItem> updated = m_items;
    updated.append(item);
    if (!saveCustomActions(updated)) {
        return false;
    }

    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.append(item);
    endInsertRows();
    emit countChanged();
    return true;
}

bool ScriptActionModel::updateCustomAction(int row,
                                           const QString &name,
                                           const QString &taskId,
                                           const QString &taskPath,
                                           const QString &note)
{
    if (row < 0 || row >= m_items.size() || m_items.at(row).builtIn) {
        setLastError(QStringLiteral("内置任务受保护，不能修改"));
        return false;
    }
    if (!validateCustomAction(name, taskId, taskPath, note, row)) {
        return false;
    }

    ScriptActionItem item = m_items.at(row);
    item.name = name.trimmed();
    item.command = taskId.trimmed();
    item.taskPath = taskPath.trimmed();
    item.note = note.trimmed();
    QList<ScriptActionItem> updated = m_items;
    updated[row] = item;
    if (!saveCustomActions(updated)) {
        return false;
    }

    m_items[row] = item;
    emit dataChanged(index(row), index(row),
                     {NameRole, CommandRole, TaskPathRole, NoteRole});
    return true;
}

bool ScriptActionModel::removeCustomAction(int row)
{
    if (row < 0 || row >= m_items.size() || m_items.at(row).builtIn) {
        setLastError(QStringLiteral("内置任务受保护，不能删除"));
        return false;
    }

    QList<ScriptActionItem> updated = m_items;
    updated.removeAt(row);
    if (!saveCustomActions(updated)) {
        return false;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
    emit countChanged();
    return true;
}

void ScriptActionModel::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}
