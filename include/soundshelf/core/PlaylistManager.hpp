#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief Static playlists CRUD plus the runtime queue.
 *
 * SoundShelf has two flavours of playlist:
 *  - **static** — a manual list of track ids, persisted in
 *    `playlists` + `playlist_tracks`. CRUD lives here.
 *  - **smart** — a JSON rule tree resolved at query time by
 *    @ref SmartPlaylistEvaluator. PlaylistManager just stores the
 *    rules blob; evaluation is delegated.
 *
 * On top of that PlaylistManager owns the runtime "now playing" queue
 * (in-memory only) used by the player widgets.
 */
class PlaylistManager : public QObject {
    Q_OBJECT
public:
    /// One row of the `playlists` table.
    struct Playlist {
        int id = -1;
        QString name;
        bool smart = false;       ///< if true the rules JSON is used
        QString rulesJson;        ///< only meaningful when smart
        QDateTime createdAt;
        QDateTime modifiedAt;
    };

    explicit PlaylistManager(QObject* parent = nullptr);
    ~PlaylistManager() override;

    // ---- Static playlists ----

    Result<int> create(const QString& name);
    Result<void> rename(int id, const QString& newName);
    Result<void> remove(int id);
    Result<QList<Playlist>> list();
    Result<Playlist> load(int id);

    /// Appends one track id to a playlist.
    Result<void> appendTrack(int playlistId, int trackId);

    /// Replaces the entire ordered set of tracks for a playlist.
    Result<void> setTracks(int playlistId, const QList<int>& trackIds);

    Result<QList<Track>> tracksOf(int playlistId);

    // ---- Smart playlists ----

    Result<int> createSmart(const QString& name, const QString& rulesJson);
    Result<void> updateSmartRules(int id, const QString& rulesJson);

    // ---- Runtime queue (in-memory) ----

    QList<Track> queue() const;
    int  queueIndex() const;
    void setQueue(const QList<Track>& queue, int startIndex = 0);
    void appendToQueue(const Track& t);
    void clearQueue();
    bool advanceQueue();             ///< returns false if at end
    bool retreatQueue();             ///< returns false if at start
    void setQueueIndex(int idx);

signals:
    void playlistCreated(int id);
    void playlistRemoved(int id);
    void playlistChanged(int id);
    void queueChanged();
    void queueIndexChanged(int idx);

private:
    QList<Track> m_queue;
    int          m_queueIndex = -1;
};

} // namespace soundshelf
