#pragma once

#include <QAbstractListModel>
#include <QList>
#include "soundshelf/data/DatabaseManager.hpp"

namespace soundshelf {

/**
 * @brief List model that exposes library source folders.
 *
 * Row 0 is a synthetic "All music" entry (sourceId == -1, not stored in DB).
 * Rows 1..N correspond to @ref DatabaseManager::SourceInfo rows.
 *
 * Editable (Qt::ItemIsEditable on rows > 0): the in-place editor calls
 * @ref setData with Qt::EditRole, which persists the new label via
 * @ref DatabaseManager::renameSource.
 *
 * Call @ref reload() after any import/remove to refresh from the DB.
 */
class SourcesModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit SourcesModel(QObject* parent = nullptr);

    /// Reload source list from the database.
    void reload();

    /// Returns the source id for @p row (-1 for "All music" row 0).
    int sourceIdAt(int row) const;

    /// Remove the source at @p row (rows > 0 only). Returns false on failure.
    bool removeAt(int row);

    // QAbstractListModel interface
    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool     setData(const QModelIndex& index, const QVariant& value,
                     int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    QList<DatabaseManager::SourceInfo> m_sources;
};

} // namespace soundshelf
