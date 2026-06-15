#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <optional>

#include "soundshelf/core/Result.hpp"
#include "soundshelf/io/PodcastFeedParser.hpp"

namespace soundshelf {

/**
 * @brief Data-layer façade for the @c podcast_feeds and @c podcast_episodes
 *        tables — feature #12 (Podcasty/audiobooki), DB-schema half.
 *
 * All queries go through @ref DatabaseManager::instance() — no extra database
 * connection is opened. Use @ref Result<T> for error propagation; no
 * exceptions are thrown.
 *
 * Network download and feed refresh remain the responsibility of a higher-level
 * manager (PodcastManager, future work). This class is purely responsible for
 * persisting and querying the structured data.
 */
class PodcastStore : public QObject {
    Q_OBJECT
public:
    /**
     * @brief One row from the @c podcast_feeds table.
     */
    struct Feed {
        int id = -1;
        QString url;
        QString title;
        QString author;
        QString description;
        QString imageUrl;
        QString link;
        QString language;
        QDateTime lastRefreshed;
        QDateTime addedAt;
    };

    /**
     * @brief One row from the @c podcast_episodes table.
     */
    struct Episode {
        int id = -1;
        int feedId = -1;
        QString guid;
        QString title;
        QString description;
        QString enclosureUrl;
        QString enclosureType;
        qint64 enclosureLength = 0;
        QDateTime pubDate;
        int durationMs = 0;
        int episodeNumber = 0;
        bool isPlayed = false;
        QString localPath;
    };

    explicit PodcastStore(QObject* parent = nullptr);
    ~PodcastStore() override;

    /**
     * @brief Subscribes to a feed URL, inserting a row if absent.
     *
     * Idempotent — calling with the same URL twice returns the same feed id
     * without inserting a duplicate row.
     *
     * @param url  The feed URL (must be unique in the @c podcast_feeds table).
     * @return The existing or newly inserted feed id.
     */
    Result<int> subscribe(const QString& url);

    /**
     * @brief Overwrites the metadata columns for @p feedId using the fields
     *        from a freshly parsed @ref PodcastFeedParser::Feed.
     *
     * Sets @c last_refreshed to the current UTC time.
     *
     * @param feedId  Primary key of the row to update.
     * @param parsed  Parsed feed data from @ref PodcastFeedParser.
     */
    Result<void> updateFeedMetadata(int feedId, const PodcastFeedParser::Feed& parsed);

    /**
     * @brief Inserts new episodes for @p feedId, updating mutable metadata on
     *        conflict but preserving @c is_played and @c local_path.
     *
     * Episodes are matched by @c (feed_id, guid). On conflict the mutable
     * columns (title, description, enclosure_url, enclosure_type,
     * enclosure_length, pub_date, duration_ms, episode_number) are overwritten;
     * @c is_played and @c local_path are left unchanged.
     *
     * @param feedId    Feed to associate episodes with.
     * @param episodes  List of parsed episodes from @ref PodcastFeedParser.
     * @return Number of *new* rows inserted (conflicts are not counted).
     */
    Result<int> upsertEpisodes(int feedId, const QList<PodcastFeedParser::Episode>& episodes);

    /**
     * @brief Returns all feeds ordered by @c title ASC.
     */
    Result<QList<Feed>> feeds();

    /**
     * @brief Returns a single feed by primary key, or @c std::nullopt when
     *        no row with that id exists.
     */
    Result<std::optional<Feed>> feed(int feedId);

    /**
     * @brief Returns all episodes for @p feedId ordered by @c pub_date DESC
     *        (NULLs last).
     */
    Result<QList<Episode>> episodesForFeed(int feedId);

    /**
     * @brief Returns a single episode by primary key.
     *
     * Added to support PodcastManager::downloadEpisode(), which must look up
     * an episode by id without knowing its feed.
     *
     * @param episodeId  Primary key of the episode row.
     * @return The episode on success, @c std::nullopt when no row exists, or
     *         an Error on database failure.
     */
    Result<std::optional<Episode>> episode(int episodeId);

    /**
     * @brief Updates the @c is_played flag for a single episode.
     *
     * @param episodeId  Primary key of the episode row.
     * @param played     New value for @c is_played.
     */
    Result<void> setPlayed(int episodeId, bool played);

    /**
     * @brief Sets the @c local_path column for a downloaded episode.
     *
     * @param episodeId  Primary key of the episode row.
     * @param path       Absolute path to the downloaded audio file.
     */
    Result<void> setLocalPath(int episodeId, const QString& path);

    /**
     * @brief Deletes the feed row and all its episodes (via CASCADE).
     *
     * @param feedId  Primary key of the feed to remove.
     */
    Result<void> unsubscribe(int feedId);
};

} // namespace soundshelf
