#include "ParamStore.h"

ParamStore::ParamStore(QObject *parent) : QAbstractListModel(parent) {}

int ParamStore::rowCount(const QModelIndex &) const { return m_params.size(); }

QVariant ParamStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_params.size()) return {};
    const auto &p = m_params[index.row()];
    switch (role) {
    case NameRole:   return p.name;
    case ValueRole:  return p.value;
    case TypeRole:   return p.type;
    case ModuleRole: return p.module;
    }
    return {};
}

QHash<int, QByteArray> ParamStore::roleNames() const
{
    return {
        {NameRole, "paramName"},
        {ValueRole, "paramValue"},
        {TypeRole, "paramType"},
        {ModuleRole, "paramModule"}
    };
}

void ParamStore::applyParamSettings(const QVariantMap &payload)
{
    const int module = payload.value("param_module").toInt();
    const QVariantList params = payload.value("params").toList();
    if (params.isEmpty()) return;

    beginResetModel();
    // Remove existing params of this module
    for (int i = m_params.size() - 1; i >= 0; --i) {
        if (m_params[i].module == module)
            m_params.removeAt(i);
    }
    // Add new params
    for (const QVariant &v : params) {
        const QVariantMap pm = v.toMap();
        ParamEntry entry;
        entry.name = pm.value("param_name").toString();
        entry.value = pm.value("param_value").toString();
        entry.type = pm.value("type").toInt();
        entry.module = module;
        m_params.append(entry);
    }
    m_dirty.clear();
    endResetModel();
    emit paramsChanged();
}

void ParamStore::setValue(int row, const QString &value)
{
    if (row < 0 || row >= m_params.size()) return;
    m_params[row].value = value;
    m_dirty.insert(row);
    emit dataChanged(index(row), index(row), {ValueRole});
}

QString ParamStore::nameAt(int row) const
{
    return (row >= 0 && row < m_params.size()) ? m_params[row].name : QString();
}

QString ParamStore::valueAt(int row) const
{
    return (row >= 0 && row < m_params.size()) ? m_params[row].value : QString();
}

int ParamStore::typeAt(int row) const
{
    return (row >= 0 && row < m_params.size()) ? m_params[row].type : 0;
}

QList<ParamEntry> ParamStore::dirtyEntries() const
{
    QList<ParamEntry> result;
    for (int i : m_dirty) {
        if (i < m_params.size())
            result.append(m_params[i]);
    }
    return result;
}

void ParamStore::clearDirty() { m_dirty.clear(); }
