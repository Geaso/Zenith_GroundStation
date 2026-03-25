#include "ScriptActionModel.h"

ScriptActionModel::ScriptActionModel(QObject *parent)
    : QAbstractListModel(parent),
      m_items({
          {"Indoor D435i Avoidance", "experiment/scripts/d435i_ego_indoor.sh", "Send To Current UAV", "Indoor visual avoidance workflow", "Avoidance", "Ready", "10:22"},
          {"Lidar-EGO Avoidance", "rc/p450_experiment/scripts/lidar_ego.sh", "Send To Current UAV", "Lidar navigation stack", "Navigation", "Last Success", "09:47"},
          {"Aruco Tracking", "tt/scripts/aruco_detection_with_d435i.sh", "Send To Current UAV", "Vision target tracking", "Perception", "Running", "In Progress"},
          {"Vision Detection Link", "ots/car_detection_with_tracking_d435i.sh", "Send To Current UAV", "Detection and tracking link", "Diagnostics", "Failed", "08:15"}
      })
{
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
}

void ScriptActionModel::removeAction(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
}
