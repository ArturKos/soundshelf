#pragma once

#include <QWidget>
#include <QList>
#include <QTimer>
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

    /// All tracks currently shown (in the order they were last set).
    /// Used by the host to push a queue into PlaylistManager.
    const QList<Track>& tracks() const { return m_tracks; }

signals:
    void trackActivated(const Track& t);

    /// Emitted (debounced 250 ms) when the search box changes.
    /// The shell is expected to run an FTS5-backed query and call
    /// @ref setTracks with the results — empty query means "show
    /// everything" (the shell typically calls listTracks).
    void searchRequested(const QString& query);

private:
    QTableView*            m_view = nullptr;
    QStandardItemModel*    m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;
    QLineEdit*             m_search = nullptr;
    QTimer                 m_searchDebounce;
    QList<Track>           m_tracks;
};

} // namespace soundshelf
