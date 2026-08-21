#include "ScriptActionModel.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

bool require(bool condition, const char *message)
{
    if (!condition) {
        qCritical("FAILED: %s", message);
    }
    return condition;
}

bool writeBuiltIns(const QString &path)
{
    QJsonArray tasks{
        QJsonObject{
            {QStringLiteral("name"), QStringLiteral("内置任务")},
            {QStringLiteral("task_id"), QStringLiteral("built_in")},
            {QStringLiteral("note"), QStringLiteral("protected")},
            {QStringLiteral("category"), QStringLiteral("测试")}
        }
    };
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(QJsonDocument(tasks).toJson()) > 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!require(temporaryDirectory.isValid(), "temporary directory")) {
        return 1;
    }

    const QString builtInPath = temporaryDirectory.filePath(QStringLiteral("built_in.json"));
    const QString customPath = temporaryDirectory.filePath(QStringLiteral("custom_tasks.json"));
    if (!require(writeBuiltIns(builtInPath), "write built-in fixture")) {
        return 1;
    }

    ScriptActionModel model(builtInPath, customPath);
    if (!require(model.builtInCount() == 1 && model.customCount() == 0,
                 "load protected built-in")) {
        return 1;
    }
    if (!require(!model.removeCustomAction(0), "built-in cannot be deleted")) {
        return 1;
    }
    if (!require(!model.updateCustomAction(
                     0, QStringLiteral("changed"), QStringLiteral("changed"),
                     QStringLiteral("/home/jetson/task_ws/src/user_tasks/changed.py"), QString()),
                 "built-in cannot be edited")) {
        return 1;
    }

    if (!require(model.addCustomAction(
                     QStringLiteral("巡检"), QStringLiteral("inspection"),
                     QStringLiteral("/home/jetson/task_ws/src/user_tasks/inspection.py"),
                     QStringLiteral("custom task")),
                 "add custom task")) {
        return 1;
    }
    if (!require(QFile::exists(customPath) && model.customCount() == 1,
                 "custom task is persisted")) {
        return 1;
    }
    if (!require(!model.addCustomAction(
                     QStringLiteral("duplicate"), QStringLiteral("inspection"),
                     QStringLiteral("/home/jetson/task_ws/src/user_tasks/duplicate.py"), QString()),
                 "duplicate task ID rejected")) {
        return 1;
    }
    if (!require(!model.addCustomAction(
                     QStringLiteral("outside"), QStringLiteral("outside_root"),
                     QStringLiteral("/opt/zenith/bin/zenith_autostart.sh"), QString()),
                 "path outside dedicated user root rejected")) {
        return 1;
    }

    if (!require(model.updateCustomAction(
                     1, QStringLiteral("巡检新版"), QStringLiteral("inspection_v2"),
                     QStringLiteral("/home/jetson/task_ws/src/user_tasks/inspection_v2.launch"),
                     QStringLiteral("updated")),
                 "update custom task")) {
        return 1;
    }

    ScriptActionModel reloaded(builtInPath, customPath);
    const QModelIndex customIndex = reloaded.index(1, 0);
    if (!require(reloaded.customCount() == 1
                 && reloaded.data(customIndex, ScriptActionModel::CommandRole).toString()
                    == QStringLiteral("inspection_v2")
                 && reloaded.data(customIndex, ScriptActionModel::TaskPathRole).toString()
                    == QStringLiteral("/home/jetson/task_ws/src/user_tasks/inspection_v2.launch"),
                 "reload saved edit")) {
        return 1;
    }
    if (!require(reloaded.removeCustomAction(1), "delete custom task")) {
        return 1;
    }

    ScriptActionModel afterDelete(builtInPath, customPath);
    if (!require(afterDelete.builtInCount() == 1 && afterDelete.customCount() == 0,
                 "deletion persists while built-in remains")) {
        return 1;
    }

    qInfo("ScriptActionModel CRUD persistence tests passed");
    return 0;
}
