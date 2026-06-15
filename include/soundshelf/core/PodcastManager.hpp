#pragma once

#include <QObject>
#include <QString>
#include <functional>

#include "soundshelf/core/Result.hpp"

namespace soundshelf {

class PodcastFeedParser;
class PodcastStore;
class RestClient;

/**
 * @brief Core-layer orchestrator for podcast subscribe, refresh, and download
 *        (feature #12 — Podcasty/audiobooki).
 *
 * Ties together io::PodcastFeedParser for RSS parsing, data::PodcastStore for
 * persistence, and an injectable synchronous HTTP fetcher for network access.
 * All public methods are **synchronous** and block until complete — callers on
 * the GUI thread must dispatch via @c QtConcurrent::run() or a @c QThread.
 *
 * The default feed/enclosure fetcher is backed by
 * network::RestClient::getBytes() with a blocking @c QFuture::result() call,
 * which is acceptable when the manager is driven from a worker thread or the
 * CLI.  Inject a stub via setFeedFetcher() / setEnclosureFetcher() for
 * unit tests — this also prevents any @c QNetworkAccessManager from being
 * instantiated (the @c RestClient is created lazily only when the default
 * fetcher is invoked).
 *
 * Signals are emitted synchronously from within the calling thread.
 */
class PodcastManager : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Synchronous byte-fetcher function type.
     *
     * Receives a full URL string and returns the raw HTTP response body, or an
     * @ref Error on network or HTTP failure.  Used for both RSS feed XML and
     * enclosure (audio file) downloads.
     */
    using FeedFetcher = std::function<Result<QByteArray>(const QString& url)>;

    explicit PodcastManager(QObject* parent = nullptr);
    ~PodcastManager() override;

    /**
     * @brief Replaces the fetcher used by subscribe() and refreshFeed().
     *
     * The default implementation uses network::RestClient::getBytes() with a
     * blocking @c .result() call.  Inject a stub for deterministic unit tests.
     *
     * @param fetcher  New feed-fetching function.
     */
    void setFeedFetcher(FeedFetcher fetcher);

    /**
     * @brief Replaces the fetcher used for enclosure (audio file) downloads.
     *
     * Defaults to the same RestClient backend as the feed fetcher.
     *
     * @param fetcher  New enclosure-fetching function.
     */
    void setEnclosureFetcher(FeedFetcher fetcher);

    /**
     * @brief Subscribes to a podcast feed URL.
     *
     * Fetches feed bytes via the feed fetcher, parses via PodcastFeedParser,
     * then persists via PodcastStore::subscribe() + updateFeedMetadata() +
     * upsertEpisodes().  Idempotent — subscribing to the same URL twice
     * returns the same feed id without inserting a duplicate row.
     *
     * Emits errorOccurred() on failure.
     *
     * @param url  RSS 2.0 (or iTunes-extended) feed URL.
     * @return The feed id (existing or newly created) on success, or an Error.
     */
    Result<int> subscribe(const QString& url);

    /**
     * @brief Refreshes a single feed by re-fetching and re-parsing it.
     *
     * Looks up the feed URL in the store, re-fetches, updates metadata, and
     * upserts episodes (preserving @c is_played and @c local_path on existing
     * episodes).  Emits feedRefreshed() on success and errorOccurred() on
     * failure.
     *
     * @param feedId  Primary key of the feed to refresh.
     * @return Number of new episodes inserted, or an Error.
     */
    Result<int> refreshFeed(int feedId);

    /**
     * @brief Refreshes every subscribed feed.
     *
     * Iterates every feed returned by PodcastStore::feeds().  Per-feed errors
     * are logged via @c qCDebug and skipped — the batch continues processing
     * remaining feeds.  Emits feedRefreshed() for each feed that succeeds.
     *
     * @return Total new episodes across all feeds on success, or an Error if
     *         the feed list itself cannot be read from the store.
     */
    Result<int> refreshAll();

    /**
     * @brief Downloads a podcast episode's enclosure to disk.
     *
     * Looks up the episode by @p episodeId to retrieve its enclosure URL,
     * fetches the bytes via the enclosure fetcher, writes them to a file in
     * @p targetDir using Qt file APIs (cross-platform, no POSIX-only calls),
     * and persists the absolute path via PodcastStore::setLocalPath().
     *
     * The filename is derived from the episode title (sanitised) with the
     * extension inferred from the enclosure MIME type; falls back to the URL
     * extension, then to @c .mp3.  Emits episodeDownloaded() on success and
     * errorOccurred() on failure.
     *
     * @param episodeId  Primary key of the episode to download.
     * @param targetDir  Existing directory where the file will be written.
     * @return Absolute path to the saved file, or an Error.
     */
    Result<QString> downloadEpisode(int episodeId, const QString& targetDir);

signals:
    /**
     * @brief Emitted after refreshFeed() or refreshAll() when a feed is
     *        successfully updated.
     * @param feedId      Primary key of the refreshed feed.
     * @param newEpisodes Number of new episodes inserted in this refresh.
     */
    void feedRefreshed(int feedId, int newEpisodes);

    /**
     * @brief Emitted after a successful downloadEpisode() call.
     * @param episodeId  Primary key of the downloaded episode.
     * @param path       Absolute path to the saved audio file.
     */
    void episodeDownloaded(int episodeId, const QString& path);

    /**
     * @brief Emitted whenever any operation (subscribe, refresh, download)
     *        encounters an error.
     * @param message  Human-readable description of the failure.
     */
    void errorOccurred(const QString& message);

private:
    PodcastFeedParser* m_parser     = nullptr;
    PodcastStore*      m_store      = nullptr;
    RestClient*        m_restClient = nullptr; ///< Lazily created on first network use.
    FeedFetcher        m_feedFetcher;
    FeedFetcher        m_enclosureFetcher;
};

} // namespace soundshelf
