#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <optional>
#include "soundshelf/core/Result.hpp"

class QThread;
#include "soundshelf/core/Track.hpp"
#include "soundshelf/core/Disc.hpp"

namespace soundshelf {

/// Singleton zarządzający połączeniem do SQLite.
/// WAL mode, FTS5, foreign_keys ON.
class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance();

    /// Otwiera bazę. Jeśli plik nie istnieje — tworzy go i aplikuje migracje.
    /// Jeśli istnieje ale wersja schema jest niższa — aplikuje brakujące migracje.
    Result<void> open(const QString& dbPath);

    /// Zamyka połączenie. Można potem znów open().
    void close();

    bool isOpen() const;
    QSqlDatabase database();

    /// Default path: $XDG_DATA_HOME/soundshelf/library.db
    static QString defaultDbPath();

    // ------- Library sources -------

    /// Metadata for a single imported folder source.
    struct SourceInfo {
        int       id;
        QString   path;
        QString   label;
        QDateTime addedAt;
    };

    /// Find-or-insert a library source by @p path. Returns the source id.
    /// @p label is only applied on the initial insert; subsequent calls for
    /// the same path are idempotent — the existing id is returned unchanged.
    Result<int> ensureSource(const QString& path, const QString& label);

    /// All library source rows ordered by @c added_at DESC.
    Result<QList<SourceInfo>> listSources();

    /// Rename a source row.
    Result<void> renameSource(int id, const QString& label);

    /// Delete a source row. Tracks that belonged to it have their
    /// @c source_id set to NULL beforehand (explicit, not FK cascade).
    Result<void> removeSource(int id);

    /// All non-missing tracks for the given source, ordered by
    /// @c added_at DESC. Limit defaults to 10 000.
    Result<QList<Track>> tracksBySource(int sourceId, int limit = 10000);

    // ------- Track operations -------

    /// INSERT lub UPDATE w zależności od istnienia filepath.
    Result<int> upsertTrack(Track& track);
    Result<Track> getTrack(int id);
    Result<Track> getTrackByPath(const QString& filepath);
    Result<QList<Track>> searchTracks(const QString& query, int limit = 100);
    Result<QList<Track>> listTracks(int limit = 1000, int offset = 0);
    Result<void> updatePlayCount(int trackId);

    /**
     * @brief Delete a single track row by @p id.
     *
     * The `tracks_ad` AFTER DELETE trigger (migration 008) keeps
     * `tracks_fts` in sync automatically — no manual FTS DELETE needed.
     *
     * @return Result<void>::ok() on success; Err when the query fails.
     */
    Result<void> removeTrack(int id);

    /**
     * @brief Delete multiple track rows in a single transaction.
     *
     * Each row is removed via the same trigger path as @ref removeTrack.
     * On any per-row failure the transaction is rolled back and an Err is
     * returned.
     *
     * @param ids  Track ids to delete (empty list is a no-op returning 0).
     * @return Number of rows actually removed on success, Err on failure.
     */
    Result<int> removeTracks(const QList<int>& ids);
    /// Writes ReplayGain values for a track. Pass std::nullopt to leave a
    /// column untouched (e.g. track-only analysis leaves album_* alone).
    Result<void> updateReplayGain(int trackId,
                                  std::optional<double> trackGain,
                                  std::optional<double> trackPeak,
                                  std::optional<double> albumGain = std::nullopt,
                                  std::optional<double> albumPeak = std::nullopt);
    Result<void> markMissing(int trackId, bool missing);

    // ------- Disc operations -------

    Result<int> upsertDisc(Disc& disc);
    Result<Disc> getDisc(int id);
    Result<Disc> getDiscByDiscId(const QString& tocDiscId);
    Result<QList<Disc>> searchDiscs(const QString& query, int limit = 50);
    /// Returns up to @p limit discs whose `type` column exactly matches
    /// @p filter, ordered by insertion time (newest first).
    /// Each DiscType value maps to a distinct stored string
    /// ('folder', 'physical', 'image', 'remote') via discTypeToString().
    /// Call once per DiscType to enumerate all discs by category.
    Result<QList<Disc>> listDiscs(DiscType filter, int limit = 1000);

    /// All tracks belonging to one disc, ordered by (disc_number, track_number).
    Result<QList<Track>> tracksByDisc(int discId);

    // ------- Reference data -------

    /// INSERT OR IGNORE dla artystów / gatunków, zwraca ID.
    Result<int> ensureArtist(const QString& name);
    Result<int> ensureGenre(const QString& name);

    /// Find-or-insert a folder-type @ref Disc identified by `(title,
    /// artistId)`. Used by @ref LibraryManager during folder imports so
    /// that the `album` column populates via the discs join.
    /// @p coverData is only written on the initial insert — subsequent
    /// calls with non-empty data fill cover_data only when the row had
    /// none, so the first track of an album wins.
    Result<int> ensureFolderDisc(const QString& albumTitle,
                                 int artistId,
                                 const QByteArray& coverData = {});

    // ------- Settings -------

    Result<QString> getSetting(const QString& key);
    Result<void>    setSetting(const QString& key, const QString& value);

    // ------- Lyrics cache -------

    /// Cached lyrics for one track. Either field may be empty when the
    /// upstream service only has one of the two formats.
    struct LyricsRow {
        QString plain;
        QString synced;
        QString source;
    };

    /// Returns the cached lyrics for @p trackId or an error when the
    /// row doesn't exist yet (caller's signal to fetch from LRCLib).
    Result<LyricsRow> getLyrics(int trackId);

    /// INSERT OR REPLACE on the lyrics row.
    Result<void> setLyrics(int trackId, const LyricsRow& row);

signals:
    void trackInserted(int id);
    void trackUpdated(int id);
    void discInserted(int id);

private:
    DatabaseManager() = default;

    /// Returns a QSqlDatabase usable on the CALLING thread. QSqlDatabase is not
    /// thread-safe: the main connection (`m_db`) may only be touched from the
    /// thread that opened it, so worker threads (e.g. the QtConcurrent library
    /// import) get their own connection to the same file (WAL handles the
    /// concurrency). All query methods go through this, never `m_db` directly.
    QSqlDatabase conn();

    QSqlDatabase m_db;
    QString m_connectionName = QStringLiteral("soundshelf_main");
    QString m_dbPath;                  ///< file path, for per-thread connections
    QThread* m_mainThread = nullptr;   ///< thread that opened m_db
};

} // namespace soundshelf
