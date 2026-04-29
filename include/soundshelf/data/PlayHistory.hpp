#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Read/write façade over the `play_history` table.
 *
 * Records are appended every time @ref PlayerEngine reports an
 * end-of-track or a meaningful skip. The class also exposes the simple
 * roll-ups used by the Stats widget (top tracks, top artists, recent).
 *
 * Queries go through @ref DatabaseManager::instance() — no extra
 * connection is opened.
 */
class PlayHistory : public QObject {
    Q_OBJECT
public:
    /// One play_history row.
    struct Entry {
        int id = -1;
        int trackId = -1;
        QDateTime playedAt;
        int playedMs = 0;
        bool completed = false;
        QString source;          ///< gui / cli / remote / mpris
    };

    /// Aggregated stats row (track id + count).
    struct Aggregate {
        int trackId = -1;
        int playCount = 0;
        QString label;           ///< title/artist for display
    };

    explicit PlayHistory(QObject* parent = nullptr);
    ~PlayHistory() override;

    /// Inserts one play event. Bumps tracks.play_count when @p completed.
    Result<int> recordPlay(int trackId,
                           int playedMs,
                           bool completed,
                           const QString& source = QStringLiteral("gui"));

    /// Returns the most recent @p limit entries (newest first).
    Result<QList<Entry>> recent(int limit = 50);

    /// Returns the @p limit entries for a single track (newest first).
    Result<QList<Entry>> forTrack(int trackId, int limit = 50);

    /// Top-N tracks by play count over an optional time window.
    /// @p sinceDays = 0 means "all time".
    Result<QList<Aggregate>> topTracks(int limit = 25, int sinceDays = 0);

    /// Number of plays per weekday, slot 0 = Monday … 6 = Sunday.
    Result<QList<int>> playsPerWeekday(int sinceDays = 30);

    /// Total played duration in milliseconds across all rows.
    Result<qint64> totalPlayedMs(int sinceDays = 0);

    /// Removes everything older than @p olderThanDays. Returns the number
    /// of rows deleted.
    Result<int> prune(int olderThanDays);
};

} // namespace soundshelf
