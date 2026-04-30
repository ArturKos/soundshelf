#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief High-level façade over library import / search / scan.
 *
 * LibraryManager is the single entry point used by UI and CLI for
 * "load this folder", "rescan everything", "find me X". It delegates
 * to @ref DatabaseManager for persistence, @ref TagInfo for tags, and
 * @ref FTS5Index for search; UI code never talks to those directly.
 *
 * All long-running operations are run on a worker thread via
 * @c QtConcurrent::run and report progress via signals; the public
 * methods themselves return immediately.
 */
class LibraryManager : public QObject {
    Q_OBJECT
public:
    explicit LibraryManager(QObject* parent = nullptr);
    ~LibraryManager() override;

    /// Imports one folder recursively. Each audio file becomes a Track row;
    /// the Disc/Album hierarchy is inferred from path + tags.
    /// Idempotent — already-known files are updated in place.
    /// @returns immediately, watch @ref importProgress / @ref importFinished.
    Result<void> importFolder(const QString& folderPath);

    /// Rescans every previously imported root, picking up new and changed files
    /// and marking removed files as missing.
    Result<void> rescanAll();

    /// Persist a track that was constructed elsewhere (e.g. by DiscManager).
    /// Returns the assigned track id.
    Result<int> addTrack(const Track& track);

    /// Synchronous full-text search. The result is small (`limit` rows).
    Result<QList<Track>> search(const QString& query, int limit = 100);

    /// Returns the Track for the given filepath, or an error if unknown.
    Result<Track> trackByPath(const QString& filepath);

    /// True while an import or rescan is in flight.
    bool isBusy() const;

signals:
    /// Periodic progress (0–100) during long-running scans.
    void importProgress(int pct, const QString& currentPath);

    /// Single emit per import / rescan run.
    void importFinished(int filesProcessed, int errors);

    /// Forwarded from DatabaseManager.
    void trackChanged(int id);

private:
    bool m_busy = false;
};

} // namespace soundshelf
