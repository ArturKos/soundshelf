#pragma once

#include <QFuture>
#include <QJsonDocument>
#include <QObject>
#include <QString>
#include <functional>
#include <optional>

#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

namespace soundshelf {

class PlayerEngine;
class LyricsClient;

/**
 * @brief Core-layer controller for lyrics loading and caching on track change.
 *
 * Extracts the inline lyrics-on-track-change logic from MainWindow into a
 * testable CORE controller, mirroring the D8 VisualizationFeeder pattern:
 * injectable seams (CacheLookupFn, FetchFn, CacheStoreFn) let unit tests
 * exercise all code paths without real DB or network I/O.
 *
 * On every trackChanged signal the controller immediately emits lyricsCleared()
 * so stale lyrics never linger, then either serves the cached text or launches
 * an async LRCLib fetch. A generation counter discards replies that arrive
 * after the user has already switched to a different track.
 *
 * Layer: CORE. May use data::DatabaseManager and network::LyricsClient.
 * The UI layer depends on LyricsController — never the reverse.
 */
class LyricsController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Synchronous cache-lookup function type.
     *
     * Given a track id, returns the cached LyricsRow, or nullopt on miss.
     * The production default wraps DatabaseManager::getLyrics(), converting
     * an error Result to nullopt.
     */
    using CacheLookupFn =
        std::function<std::optional<DatabaseManager::LyricsRow>(int trackId)>;

    /**
     * @brief Asynchronous lyrics-fetch function type.
     *
     * Launches an LRCLib network request and returns a QFuture. The
     * production default calls LyricsClient::getLyrics().
     */
    using FetchFn =
        std::function<QFuture<Result<QJsonDocument>>(const QString& artist,
                                                      const QString& title,
                                                      const QString& album,
                                                      int durationSec)>;

    /**
     * @brief Cache-store function type.
     *
     * Persists a LyricsRow for @p trackId. The production default calls
     * DatabaseManager::instance().setLyrics().
     */
    using CacheStoreFn =
        std::function<void(int trackId, const DatabaseManager::LyricsRow& row)>;

    /**
     * @brief Branch-decision outcome used by the pure decide() helper.
     */
    enum class Outcome {
        ShowCached, ///< Cache hit — emit lyricsReady with cached data.
        Fetch,      ///< Cache miss and valid metadata — fetch from network.
        Empty,      ///< Cache miss and incomplete metadata — stay cleared.
    };

    /**
     * @brief Constructs a LyricsController with production-default seams.
     * @param parent Optional Qt parent.
     */
    explicit LyricsController(QObject* parent = nullptr);

    /**
     * @brief Wires this controller to @p engine.
     *
     * Connects engine->trackChanged to onTrackChanged(). Must be called
     * exactly once after the engine is constructed. Does not transfer
     * ownership of @p engine.
     *
     * @param engine Non-null PlayerEngine to drive.
     */
    void attachEngine(PlayerEngine* engine);

    /**
     * @brief Overrides the cache lookup function.
     *
     * Passing a default-constructed (empty) fn restores the production default.
     * @param fn Callable matching CacheLookupFn.
     */
    void setCacheLookup(CacheLookupFn fn);

    /**
     * @brief Overrides the network fetch function.
     *
     * Passing a default-constructed (empty) fn restores the production default.
     * @param fn Callable matching FetchFn.
     */
    void setFetcher(FetchFn fn);

    /**
     * @brief Overrides the cache store function.
     *
     * Passing a default-constructed (empty) fn restores the production default.
     * @param fn Callable matching CacheStoreFn.
     */
    void setCacheStore(CacheStoreFn fn);

    /**
     * @brief Pure branch-decision helper for deterministic testing.
     *
     * Determines the action to take given cache result and track metadata,
     * without touching any instance state.
     *
     * @param cacheHit  True when the cache lookup returned a value.
     * @param artist    Track artist string (may be empty).
     * @param title     Track title string (may be empty).
     * @return Outcome::ShowCached if cacheHit; Outcome::Empty if artist or
     *         title is empty; Outcome::Fetch otherwise.
     */
    static Outcome decide(bool cacheHit, const QString& artist, const QString& title);

public slots:
    /**
     * @brief Handles a track change from PlayerEngine::trackChanged.
     *
     * Steps:
     *  1. Bumps the generation counter and updates m_currentTrackId.
     *  2. Emits lyricsCleared() unconditionally — old lyrics never linger.
     *  3. Returns early when t.id < 0 (no active track).
     *  4. On cache hit → emits lyricsReady(row.plain, row.synced); returns.
     *  5. On miss with empty artist or title → stays cleared (non-fatal).
     *  6. Otherwise → launches FetchFn; on future completion the stale-reply
     *     guard drops the result if the generation has advanced.
     *
     * @param t Track that just started playing.
     */
    void onTrackChanged(const Track& t);

signals:
    /**
     * @brief Emitted when lyrics are available (cached or freshly fetched).
     * @param plain  Plain-text lyrics (may be empty if only synced is available).
     * @param synced LRC-format synced lyrics (may be empty).
     */
    void lyricsReady(const QString& plain, const QString& synced);

    /**
     * @brief Emitted immediately on every trackChanged so old lyrics never linger.
     */
    void lyricsCleared();

private:
    FetchFn makeDefaultFetcher();

    int     m_currentTrackId = -1;
    quint64 m_generation     = 0;

    CacheLookupFn m_cacheLookup;
    FetchFn       m_fetcher;
    CacheStoreFn  m_cacheStore;

    LyricsClient* m_lyricsClient = nullptr;
};

} // namespace soundshelf
