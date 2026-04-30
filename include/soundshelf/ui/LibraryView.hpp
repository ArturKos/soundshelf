#pragma once

#include <QWidget>
#include <QList>
#include "soundshelf/core/Track.hpp"

class QTableView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QLineEdit;

namespace soundshelf {

/**
 * @brief Main track listing — `QTableView` + `QStandardItemModel` +
 *        `QSortFilterProxyModel`.
 *
 * Columns: # / title / artist / album / genre / year / duration.
 * Sortable by every column; the search box on top filters across them.
 * Double-click on a row emits @ref trackActivated which the shell
 * wires to PlayerEngine.
 */
class LibraryView : public QWidget {
    Q_OBJECT
public:
    explicit LibraryView(QWidget* parent = nullptr);
    ~LibraryView() override;

    /// Replaces the model contents with @p tracks.
    void setTracks(const QList<Track>& tracks);

    /// Currently selected track ids.
    QList<int> selectedTrackIds() const;

signals:
    void trackActivated(const Track& t);

private:
    QTableView*            m_view = nullptr;
    QStandardItemModel*    m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    QLineEdit*             m_search = nullptr;
    QList<Track>           m_tracks;
};

} // namespace soundshelf
