#include "soundshelf/core/LyricsController.hpp"
#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/network/LyricsClient.hpp"

#include <QFutureWatcher>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcLyrics, "soundshelf.core.lyrics")

namespace soundshelf {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LyricsController::LyricsController(QObject* parent)
    : QObject(parent)
    , m_lyricsClient(new LyricsClient(this))
{
    m_cacheLookup = [](int id) -> std::optional<DatabaseManager::LyricsRow> {
        auto r = DatabaseManager::instance().getLyrics(id);
        if (!r) return std::nullopt;
        return r.value();
    };
    m_fetcher   = makeDefaultFetcher();
    m_cacheStore = [](int id, const DatabaseManager::LyricsRow& row) {
        DatabaseManager::instance().setLyrics(id, row);
    };
}

// ---------------------------------------------------------------------------
// Seam setters
// ---------------------------------------------------------------------------

void LyricsController::attachEngine(PlayerEngine* engine)
{
    Q_ASSERT(engine);
    connect(engine, &PlayerEngine::trackChanged,
            this, &LyricsController::onTrackChanged);
}

void LyricsController::setCacheLookup(CacheLookupFn fn)
{
    if (fn)
        m_cacheLookup = std::move(fn);
    else
        m_cacheLookup = [](int id) -> std::optional<DatabaseManager::LyricsRow> {
            auto r = DatabaseManager::instance().getLyrics(id);
            if (!r) return std::nullopt;
            return r.value();
        };
}

void LyricsController::setFetcher(FetchFn fn)
{
    if (fn)
        m_fetcher = std::move(fn);
    else
        m_fetcher = makeDefaultFetcher();
}

void LyricsController::setCacheStore(CacheStoreFn fn)
{
    if (fn)
        m_cacheStore = std::move(fn);
    else
        m_cacheStore = [](int id, const DatabaseManager::LyricsRow& row) {
            DatabaseManager::instance().setLyrics(id, row);
        };
}

// ---------------------------------------------------------------------------
// Pure helper
// ---------------------------------------------------------------------------

LyricsController::Outcome LyricsController::decide(bool cacheHit,
                                                    const QString& artist,
                                                    const QString& title)
{
    if (cacheHit)
        return Outcome::ShowCached;
    if (artist.isEmpty() || title.isEmpty())
        return Outcome::Empty;
    return Outcome::Fetch;
}

// ---------------------------------------------------------------------------
// Slot
// ---------------------------------------------------------------------------

void LyricsController::onTrackChanged(const Track& t)
{
    ++m_generation;
    m_currentTrackId = t.id;
    emit lyricsCleared(); // always clear first so stale lyrics never linger

    if (t.id < 0)
        return;

    const auto cached = m_cacheLookup(t.id);
    const auto outcome = decide(cached.has_value(), t.artist, t.title);

    switch (outcome) {
        case Outcome::ShowCached:
            emit lyricsReady(cached->plain, cached->synced);
            return;
        case Outcome::Empty:
            qCDebug(lcLyrics) << "No lyrics metadata for track" << t.id << "— staying cleared";
            return;
        case Outcome::Fetch:
            break; // fall through to async fetch
    }

    const quint64 gen = m_generation;
    const int id      = t.id;
    auto fut = m_fetcher(t.artist, t.title, t.album, qMax(0, t.durationMs / 1000));

    auto* w = new QFutureWatcher<Result<QJsonDocument>>(this);
    connect(w, &QFutureWatcher<Result<QJsonDocument>>::finished, this,
            [this, w, gen, id]() {
        w->deleteLater();
        if (gen != m_generation) {
            qCDebug(lcLyrics) << "Stale lyrics reply dropped (gen" << gen
                              << "vs current" << m_generation << ')';
            return;
        }
        const auto r = w->result();
        if (!r) {
            qCDebug(lcLyrics) << "No lyrics from network:" << r.error().message;
            return; // non-fatal — stay cleared
        }
        const auto decoded = LyricsClient::decode(r.value());
        emit lyricsReady(decoded.plain, decoded.synced);
        DatabaseManager::LyricsRow row;
        row.plain  = decoded.plain;
        row.synced = decoded.synced;
        row.source = decoded.source;
        m_cacheStore(id, row);
    });
    w->setFuture(fut);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

LyricsController::FetchFn LyricsController::makeDefaultFetcher()
{
    return [this](const QString& artist, const QString& title,
                  const QString& album, int durationSec) {
        return m_lyricsClient->getLyrics(artist, title, album, durationSec);
    };
}

} // namespace soundshelf
