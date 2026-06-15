#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <optional>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Data-layer façade for the @c bookmarks table — feature #12
 *        (Podcasty/audiobooki).
 *
 * Stores two kinds of rows per track:
 *  - **Resume marker** (@c is_resume = 1) — the single position the player
 *    should resume from on next open. At most one per track (enforced by a
 *    partial UNIQUE index).
 *  - **Named bookmarks** (@c is_resume = 0) — arbitrary user-placed markers
 *    with an optional label.
 *
 * All queries go through @ref DatabaseManager::instance() — no extra database
 * connection is opened.  Use @ref Result<T> for error propagation; no
 * exceptions are thrown.
 */
class BookmarkStore : public QObject {
    Q_OBJECT
public:
    /// One row from the @c bookmarks table.
    struct Bookmark {
        int id = -1;
        int trackId = -1;
        int positionMs = 0;
        QString label;
        bool isResume = false;
        QDateTime createdAt;
    };

    explicit BookmarkStore(QObject* parent = nullptr);
    ~BookmarkStore() override;

    /**
     * @brief Upserts the resume marker for @p trackId.
     *
     * Deletes any existing resume row for the track, then inserts a new one
     * at @p positionMs.  This is safe even if no prior marker exists.
     */
    Result<void> setResumePosition(int trackId, int positionMs);

    /**
     * @brief Returns the resume position in milliseconds, or @c std::nullopt
     *        when no resume marker exists for @p trackId.
     */
    Result<std::optional<int>> resumePosition(int trackId);

    /**
     * @brief Removes the resume marker for @p trackId (no-op when absent).
     */
    Result<void> clearResume(int trackId);

    /**
     * @brief Inserts a named bookmark (@c is_resume = 0) at @p positionMs.
     * @return The new row's primary key.
     */
    Result<int> addBookmark(int trackId, int positionMs, const QString& label);

    /**
     * @brief Returns all named bookmarks for @p trackId, ordered by
     *        @c position_ms ascending.  Resume markers are excluded.
     */
    Result<QList<Bookmark>> bookmarksForTrack(int trackId);

    /**
     * @brief Deletes the bookmark row with primary key @p id.
     */
    Result<void> removeBookmark(int id);

    /**
     * @brief Removes every bookmark (resume + named) for @p trackId.
     * @return The number of rows deleted.
     */
    Result<int> removeAllForTrack(int trackId);
};

} // namespace soundshelf
