#include "RadioPairingModel.h"

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

bool writeJson(const QString &path, const QJsonDocument &document)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(document.toJson()) > 0;
}

QJsonObject pairing(const QString &name, int id, const QString &note)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("radio_address"), id},
        {QStringLiteral("uav_id"), id},
        {QStringLiteral("note"), note}
    };
}

bool writeBuiltIns(const QString &path)
{
    return writeJson(path, QJsonDocument(QJsonArray{
                               pairing(QStringLiteral("yukong214"), 214,
                                       QStringLiteral("built-in 214")),
                               pairing(QStringLiteral("新F.1"), 215,
                                       QStringLiteral("built-in 215"))
                           }));
}

bool writeUserLayer(const QString &path)
{
    const QJsonObject root{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("pairings"),
         QJsonArray{
             pairing(QStringLiteral("yukong214"), 216, QStringLiteral("override")),
             pairing(QStringLiteral("测试机"), 7, QStringLiteral("custom"))
         }}
    };
    return writeJson(path, QJsonDocument(root));
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
    const QString userPath = temporaryDirectory.filePath(QStringLiteral("radio_pairings.json"));
    if (!require(writeBuiltIns(builtInPath), "write built-in fixture")
        || !require(writeUserLayer(userPath), "write user fixture")) {
        return 1;
    }

    RadioPairingModel model(builtInPath, userPath);
    if (!require(model.count() == 3 && model.builtInCount() == 2
                     && model.customCount() == 1 && model.overrideCount() == 1,
                 "merge built-in, override and custom layers")) {
        return 1;
    }

    const int overriddenRow = model.indexOfName(QStringLiteral("yukong214"));
    const QVariantMap overridden = model.pairingAt(overriddenRow);
    if (!require(overriddenRow == 0
                     && overridden.value(QStringLiteral("radio_address")).toInt() == 216
                     && overridden.value(QStringLiteral("uav_id")).toInt() == 216
                     && overridden.value(QStringLiteral("builtIn")).toBool()
                     && overridden.value(QStringLiteral("overridden")).toBool()
                     && !overridden.value(QStringLiteral("canDelete")).toBool(),
                 "user layer overrides matching built-in by name")) {
        return 1;
    }
    if (!require(model.pairingByName(QStringLiteral("测试机"))
                         .value(QStringLiteral("radio_address")).toInt() == 7,
                 "lookup pairing by persisted name")) {
        return 1;
    }

    if (!require(!model.removePairing(overriddenRow),
                 "built-in remains protected while overridden")
        || !require(model.lastError().contains(QStringLiteral("恢复内置值")),
                    "protected override has actionable error")) {
        return 1;
    }
    if (!require(model.restoreBuiltIn(overriddenRow), "remove override explicitly")) {
        return 1;
    }
    const QVariantMap restored = model.pairingByName(QStringLiteral("yukong214"));
    if (!require(restored.value(QStringLiteral("radio_address")).toInt() == 214
                     && !restored.value(QStringLiteral("overridden")).toBool(),
                 "restoring override reveals immutable built-in")) {
        return 1;
    }

    const int builtInRow = model.indexOfName(QStringLiteral("新F.1"));
    if (!require(model.updatePairing(builtInRow, QStringLiteral("新F.1"), 217, 217,
                                     QStringLiteral("user override")),
                 "built-in values may be overridden")) {
        return 1;
    }
    if (!require(model.pairingAt(builtInRow).value(QStringLiteral("overridden")).toBool(),
                 "override is explicit in model roles")) {
        return 1;
    }
    if (!require(!model.updatePairing(builtInRow, QStringLiteral("renamed"), 217, 217,
                                      QString()),
                 "built-in name cannot be renamed")) {
        return 1;
    }

    const QList<int> rejectedIds{0, 255, 1000, -1, 256};
    for (int id : rejectedIds) {
        if (!require(!model.addPairing(QStringLiteral("invalid-%1").arg(id), id, id,
                                      QString()),
                     "reserved or out-of-range ID rejected")) {
            return 1;
        }
    }
    if (!require(!model.addPairing(QStringLiteral(""), 8, 8, QString()),
                 "empty name rejected")
        || !require(!model.addPairing(QStringLiteral("mismatch"), 8, 9, QString()),
                    "radio address and UAV ID mismatch rejected")
        || !require(model.lastError().contains(QStringLiteral("一机一号")),
                    "mismatch has a Chinese error")) {
        return 1;
    }
    if (!require(model.addPairing(QStringLiteral("下边界"), 1, 1, QString()),
                 "lower assignable boundary accepted")
        || !require(model.addPairing(QStringLiteral("上边界"), 254, 254, QString()),
                    "upper assignable boundary accepted")) {
        return 1;
    }

    if (!require(model.addPairing(QStringLiteral("持久化测试"), 8, 8,
                                  QStringLiteral("saved")),
                 "valid custom pairing accepted")) {
        return 1;
    }
    RadioPairingModel reloaded(builtInPath, userPath);
    const QVariantMap persisted = reloaded.pairingByName(QStringLiteral("持久化测试"));
    if (!require(persisted.value(QStringLiteral("radio_address")).toInt() == 8
                     && persisted.value(QStringLiteral("uav_id")).toInt() == 8
                     && QFile::exists(userPath),
                 "user add and built-in override persist")) {
        return 1;
    }

    const int customRow = reloaded.indexOfName(QStringLiteral("持久化测试"));
    if (!require(reloaded.updatePairing(customRow, QStringLiteral("持久化测试2"), 9, 9,
                                        QStringLiteral("updated")),
                 "custom pairing can be edited")) {
        return 1;
    }
    const int updatedRow = reloaded.indexOfName(QStringLiteral("持久化测试2"));
    if (!require(updatedRow >= 0 && reloaded.removePairing(updatedRow),
                 "custom pairing can be deleted")) {
        return 1;
    }
    RadioPairingModel afterDelete(builtInPath, userPath);
    if (!require(afterDelete.indexOfName(QStringLiteral("持久化测试2")) == -1
                     && afterDelete.indexOfName(QStringLiteral("新F.1")) >= 0,
                 "custom deletion persists and built-in remains")) {
        return 1;
    }

    qInfo("RadioPairingModel merge, validation and persistence tests passed");
    return 0;
}
