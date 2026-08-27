#include "RadioPairingModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr int kMinimumAssignableId = 1;
constexpr int kMaximumAssignableId = 254;

QString builtInRadioPairingsPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("config/radio_pairings.json"));
}

QString userRadioPairingsPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("radio_pairings.json"));
}

RadioPairingItem itemFromJson(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("name")).toString().trimmed(),
        object.value(QStringLiteral("radio_address")).toInt(),
        object.value(QStringLiteral("uav_id")).toInt(),
        object.value(QStringLiteral("note")).toString().trimmed(),
        false,
        false
    };
}

QJsonObject itemToJson(const RadioPairingItem &item)
{
    return {
        {QStringLiteral("name"), item.name},
        {QStringLiteral("radio_address"), item.radioAddress},
        {QStringLiteral("uav_id"), item.uavId},
        {QStringLiteral("note"), item.note}
    };
}

bool hasValidShape(const QJsonObject &object)
{
    const QJsonValue radioAddress = object.value(QStringLiteral("radio_address"));
    const QJsonValue uavId = object.value(QStringLiteral("uav_id"));
    return object.value(QStringLiteral("name")).isString()
        && radioAddress.isDouble()
        && radioAddress.toDouble() == radioAddress.toInt()
        && uavId.isDouble()
        && uavId.toDouble() == uavId.toInt()
        && (!object.contains(QStringLiteral("note"))
            || object.value(QStringLiteral("note")).isString());
}

bool isValidStoredItem(const RadioPairingItem &item)
{
    return !item.name.isEmpty()
        && item.name.size() <= 64
        && item.radioAddress >= kMinimumAssignableId
        && item.radioAddress <= kMaximumAssignableId
        && item.uavId >= kMinimumAssignableId
        && item.uavId <= kMaximumAssignableId
        && item.radioAddress == item.uavId
        && item.note.size() <= 200;
}

QVariantMap toVariantMap(const RadioPairingItem &item)
{
    return {
        {QStringLiteral("name"), item.name},
        {QStringLiteral("radio_address"), item.radioAddress},
        {QStringLiteral("uav_id"), item.uavId},
        {QStringLiteral("note"), item.note},
        {QStringLiteral("builtIn"), item.builtIn},
        {QStringLiteral("overridden"), item.overridden},
        {QStringLiteral("canDelete"), !item.builtIn}
    };
}

} // namespace

RadioPairingModel::RadioPairingModel(QObject *parent)
    : RadioPairingModel(builtInRadioPairingsPath(), userRadioPairingsPath(), parent)
{
}

RadioPairingModel::RadioPairingModel(const QString &builtInConfigPath,
                                     const QString &userConfigPath,
                                     QObject *parent)
    : QAbstractListModel(parent),
      m_userConfigPath(userConfigPath)
{
    loadBuiltIns(builtInConfigPath);
    loadUserPairings(userConfigPath);
    rebuildMergedItems();
}

bool RadioPairingModel::loadBuiltIns(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("未找到内置数传配对表：%1").arg(path));
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        setLastError(QStringLiteral("内置数传配对表解析失败：%1").arg(error.errorString()));
        return false;
    }

    for (const QJsonValue &value : document.array()) {
        if (!value.isObject() || !hasValidShape(value.toObject())) {
            continue;
        }
        RadioPairingItem item = itemFromJson(value.toObject());
        if (!isValidStoredItem(item) || builtInEntryIndex(item.name) >= 0) {
            continue;
        }
        item.builtIn = true;
        m_builtInItems.append(item);
    }
    return true;
}

bool RadioPairingModel::loadUserPairings(const QString &path)
{
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("无法读取用户数传配对表：%1").arg(path));
        return false;
    }

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        setLastError(QStringLiteral("用户数传配对表解析失败：%1").arg(error.errorString()));
        return false;
    }

    const QJsonArray pairings = document.object().value(QStringLiteral("pairings")).toArray();
    for (const QJsonValue &value : pairings) {
        if (!value.isObject() || !hasValidShape(value.toObject())) {
            continue;
        }
        RadioPairingItem item = itemFromJson(value.toObject());
        if (!isValidStoredItem(item) || userEntryIndex(item.name) >= 0) {
            continue;
        }
        m_userItems.append(item);
    }
    return true;
}

int RadioPairingModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

int RadioPairingModel::builtInCount() const
{
    return m_builtInItems.size();
}

int RadioPairingModel::customCount() const
{
    int result = 0;
    for (const RadioPairingItem &item : m_items) {
        result += item.builtIn ? 0 : 1;
    }
    return result;
}

int RadioPairingModel::overrideCount() const
{
    int result = 0;
    for (const RadioPairingItem &item : m_items) {
        result += item.overridden ? 1 : 0;
    }
    return result;
}

QVariant RadioPairingModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const RadioPairingItem &item = m_items.at(index.row());
    switch (role) {
    case NameRole:
        return item.name;
    case RadioAddressRole:
        return item.radioAddress;
    case UavIdRole:
        return item.uavId;
    case NoteRole:
        return item.note;
    case BuiltInRole:
        return item.builtIn;
    case OverriddenRole:
        return item.overridden;
    case CanDeleteRole:
        return !item.builtIn;
    default:
        return {};
    }
}

QHash<int, QByteArray> RadioPairingModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {RadioAddressRole, "radioAddress"},
        {UavIdRole, "uavId"},
        {NoteRole, "note"},
        {BuiltInRole, "builtIn"},
        {OverriddenRole, "overridden"},
        {CanDeleteRole, "canDelete"}
    };
}

bool RadioPairingModel::addPairing(const QString &name,
                                   int radioAddress,
                                   int uavId,
                                   const QString &note)
{
    if (!validatePairing(name, radioAddress, uavId, note, -1)) {
        return false;
    }

    RadioPairingItem item{name.trimmed(), radioAddress, uavId, note.trimmed(), false, false};
    QList<RadioPairingItem> updated = m_userItems;
    updated.append(item);
    if (!saveUserPairings(updated)) {
        return false;
    }

    beginResetModel();
    m_userItems = updated;
    rebuildMergedItems();
    endResetModel();
    emit countChanged();
    return true;
}

bool RadioPairingModel::updatePairing(int row,
                                      const QString &name,
                                      int radioAddress,
                                      int uavId,
                                      const QString &note)
{
    if (row < 0 || row >= m_items.size()) {
        setLastError(QStringLiteral("无效的数传配对项"));
        return false;
    }

    const RadioPairingItem current = m_items.at(row);
    const QString normalizedName = name.trimmed();
    if (current.builtIn && normalizedName != current.name) {
        setLastError(QStringLiteral("内置配对项名称不可修改；可以覆盖地址、飞机 ID 和备注"));
        return false;
    }
    if (!validatePairing(normalizedName, radioAddress, uavId, note, row)) {
        return false;
    }

    RadioPairingItem replacement{
        normalizedName, radioAddress, uavId, note.trimmed(), false, false
    };
    QList<RadioPairingItem> updated = m_userItems;
    const int userRow = userEntryIndex(current.name);
    if (userRow >= 0) {
        updated[userRow] = replacement;
    } else {
        updated.append(replacement);
    }
    if (!saveUserPairings(updated)) {
        return false;
    }

    beginResetModel();
    m_userItems = updated;
    rebuildMergedItems();
    endResetModel();
    emit countChanged();
    return true;
}

bool RadioPairingModel::removePairing(int row)
{
    if (row < 0 || row >= m_items.size()) {
        setLastError(QStringLiteral("无效的数传配对项"));
        return false;
    }

    const RadioPairingItem current = m_items.at(row);
    if (current.builtIn) {
        setLastError(current.overridden
                         ? QStringLiteral("内置配对项不能删除；请使用“恢复内置值”取消用户覆盖")
                         : QStringLiteral("内置配对项受保护，不能删除"));
        return false;
    }

    const int userRow = userEntryIndex(current.name);
    if (userRow < 0) {
        setLastError(QStringLiteral("未找到可删除的用户配对项"));
        return false;
    }

    QList<RadioPairingItem> updated = m_userItems;
    updated.removeAt(userRow);
    if (!saveUserPairings(updated)) {
        return false;
    }

    beginResetModel();
    m_userItems = updated;
    rebuildMergedItems();
    endResetModel();
    emit countChanged();
    return true;
}

bool RadioPairingModel::restoreBuiltIn(int row)
{
    if (row < 0 || row >= m_items.size() || !m_items.at(row).builtIn) {
        setLastError(QStringLiteral("只能恢复内置配对项"));
        return false;
    }
    if (!m_items.at(row).overridden) {
        setLastError(QStringLiteral("该内置配对项没有用户覆盖"));
        return false;
    }

    QList<RadioPairingItem> updated = m_userItems;
    const int userRow = userEntryIndex(m_items.at(row).name);
    if (userRow < 0) {
        setLastError(QStringLiteral("未找到该内置配对项的用户覆盖"));
        return false;
    }
    updated.removeAt(userRow);
    if (!saveUserPairings(updated)) {
        return false;
    }

    beginResetModel();
    m_userItems = updated;
    rebuildMergedItems();
    endResetModel();
    emit countChanged();
    return true;
}

int RadioPairingModel::indexOfName(const QString &name) const
{
    const QString normalizedName = name.trimmed();
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).name == normalizedName) {
            return row;
        }
    }
    return -1;
}

QVariantMap RadioPairingModel::pairingAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }
    return toVariantMap(m_items.at(row));
}

QVariantMap RadioPairingModel::pairingByName(const QString &name) const
{
    return pairingAt(indexOfName(name));
}

bool RadioPairingModel::isNameAvailable(const QString &name, int exceptRow) const
{
    const QString normalizedName = name.trimmed();
    for (int row = 0; row < m_items.size(); ++row) {
        if (row != exceptRow && m_items.at(row).name == normalizedName) {
            return false;
        }
    }
    return true;
}

bool RadioPairingModel::saveUserPairings(const QList<RadioPairingItem> &items)
{
    const QFileInfo fileInfo(m_userConfigPath);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        setLastError(QStringLiteral("无法创建用户数传配对目录：%1").arg(fileInfo.absolutePath()));
        return false;
    }

    QJsonArray pairings;
    for (const RadioPairingItem &item : items) {
        pairings.append(itemToJson(item));
    }
    const QJsonObject root{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("pairings"), pairings}
    };

    QSaveFile file(m_userConfigPath);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(QStringLiteral("无法写入用户数传配对表：%1").arg(m_userConfigPath));
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setLastError(QStringLiteral("保存用户数传配对表失败：%1").arg(m_userConfigPath));
        return false;
    }
    setLastError(QString());
    return true;
}

bool RadioPairingModel::validatePairing(const QString &name,
                                        int radioAddress,
                                        int uavId,
                                        const QString &note,
                                        int exceptRow)
{
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty() || normalizedName.size() > 64) {
        setLastError(QStringLiteral("配对名称不能为空且最多 64 个字符"));
        return false;
    }
    if (!isNameAvailable(normalizedName, exceptRow)) {
        setLastError(QStringLiteral("配对名称已存在；同名内置项请直接编辑以创建用户覆盖"));
        return false;
    }
    if (radioAddress < kMinimumAssignableId || radioAddress > kMaximumAssignableId) {
        setLastError(QStringLiteral(
            "数传地址必须在 1–254 之间；1000 仅用于读回显示“未配对”，不能保存为配对项"));
        return false;
    }
    if (uavId < kMinimumAssignableId || uavId > kMaximumAssignableId) {
        setLastError(QStringLiteral("飞机 ID 必须在 1–254 之间；0、255 和 1000 均不可分配"));
        return false;
    }
    if (radioAddress != uavId) {
        setLastError(QStringLiteral("同一台飞机的数传地址必须与飞机 ID 相同（一机一号）"));
        return false;
    }
    if (note.size() > 200) {
        setLastError(QStringLiteral("配对备注最多 200 个字符"));
        return false;
    }
    setLastError(QString());
    return true;
}

void RadioPairingModel::rebuildMergedItems()
{
    m_items.clear();
    for (const RadioPairingItem &builtIn : m_builtInItems) {
        const int overrideRow = userEntryIndex(builtIn.name);
        if (overrideRow >= 0) {
            RadioPairingItem merged = m_userItems.at(overrideRow);
            merged.builtIn = true;
            merged.overridden = true;
            m_items.append(merged);
        } else {
            m_items.append(builtIn);
        }
    }

    for (const RadioPairingItem &user : m_userItems) {
        if (builtInEntryIndex(user.name) < 0) {
            m_items.append(user);
        }
    }
}

int RadioPairingModel::userEntryIndex(const QString &name) const
{
    for (int row = 0; row < m_userItems.size(); ++row) {
        if (m_userItems.at(row).name == name) {
            return row;
        }
    }
    return -1;
}

int RadioPairingModel::builtInEntryIndex(const QString &name) const
{
    for (int row = 0; row < m_builtInItems.size(); ++row) {
        if (m_builtInItems.at(row).name == name) {
            return row;
        }
    }
    return -1;
}

void RadioPairingModel::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}
