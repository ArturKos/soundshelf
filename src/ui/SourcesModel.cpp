#include "soundshelf/ui/SourcesModel.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSources, "soundshelf.ui.sources")

namespace soundshelf {

SourcesModel::SourcesModel(QObject* parent)
    : QAbstractListModel(parent)
{
    reload();
}

void SourcesModel::reload() {
    beginResetModel();
    auto r = DatabaseManager::instance().listSources();
    if (r) {
        m_sources = r.value();
    } else {
        qCWarning(lcSources) << "listSources failed:" << r.error().message;
        m_sources.clear();
    }
    endResetModel();
}

int SourcesModel::sourceIdAt(int row) const {
    if (row == 0) return -1;
    const int idx = row - 1;
    if (idx < 0 || idx >= m_sources.size()) return -1;
    return m_sources[idx].id;
}

bool SourcesModel::removeAt(int row) {
    if (row <= 0 || row > m_sources.size()) return false;
    const int id = m_sources[row - 1].id;
    auto r = DatabaseManager::instance().removeSource(id);
    if (!r) {
        qCWarning(lcSources) << "removeSource failed:" << r.error().message;
        return false;
    }
    beginRemoveRows({}, row, row);
    m_sources.removeAt(row - 1);
    endRemoveRows();
    return true;
}

int SourcesModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 1 + m_sources.size();  // synthetic "All music" + DB rows
}

QVariant SourcesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) return {};

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.row() == 0) return tr("All music");
        return m_sources[index.row() - 1].label;
    }
    if (role == Qt::UserRole) {
        return sourceIdAt(index.row());
    }
    if (role == Qt::ToolTipRole && index.row() > 0) {
        return m_sources[index.row() - 1].path;
    }
    return {};
}

bool SourcesModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole) return false;
    if (!index.isValid() || index.row() <= 0 || index.row() > m_sources.size()) return false;

    const QString newLabel = value.toString().trimmed();
    if (newLabel.isEmpty()) return false;

    const int id = m_sources[index.row() - 1].id;
    auto r = DatabaseManager::instance().renameSource(id, newLabel);
    if (!r) {
        qCWarning(lcSources) << "renameSource failed:" << r.error().message;
        return false;
    }
    m_sources[index.row() - 1].label = newLabel;
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

Qt::ItemFlags SourcesModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.row() > 0) f |= Qt::ItemIsEditable;
    return f;
}

} // namespace soundshelf
