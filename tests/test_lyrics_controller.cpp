#include <QtTest>
#include <QSignalSpy>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPromise>

#include "soundshelf/core/LyricsController.hpp"
#include "soundshelf/core/Track.hpp"
#include "soundshelf/core/Result.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

using namespace soundshelf;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Build a LRCLib-shaped JSON document that LyricsClient::decode() can parse.
QJsonDocument makeDoc(const QString& plain, const QString& synced)
{
    QJsonObject obj;
    obj[QStringLiteral("plainLyrics")]  = plain;
    obj[QStringLiteral("syncedLyrics")] = synced;
    return QJsonDocument(obj);
}

/// Create an already-finished future carrying @p result.
template<typename T>
QFuture<T> makeReadyFuture(T result)
{
    QPromise<T> promise;
    auto future = promise.future();
    promise.start();
    promise.addResult(std::move(result));
    promise.finish();
    return future;
}

/// Build a Track with the given fields.
Track makeTrack(int id, const QString& artist = {}, const QString& title = {},
                const QString& album = {}, int durationMs = 0)
{
    Track t;
    t.id         = id;
    t.artist     = artist;
    t.title      = title;
    t.album      = album;
    t.durationMs = durationMs;
    return t;
}

/// Seam that always returns a cache miss.
LyricsController::CacheLookupFn cacheMiss()
{
    return [](int) -> std::optional<DatabaseManager::LyricsRow> {
        return std::nullopt;
    };
}

/// Seam that always returns a cache hit with @p plain / @p synced.
LyricsController::CacheLookupFn cacheHit(const QString& plain, const QString& synced)
{
    return [plain, synced](int) -> std::optional<DatabaseManager::LyricsRow> {
        DatabaseManager::LyricsRow row;
        row.plain  = plain;
        row.synced = synced;
        row.source = QStringLiteral("test");
        return row;
    };
}

/// Seam that counts calls and returns an ok result.
LyricsController::FetchFn makeFetcher(const QString& plain, const QString& synced,
                                       int* callCount = nullptr)
{
    return [plain, synced, callCount](const QString&, const QString&, const QString&, int)
        -> QFuture<Result<QJsonDocument>>
    {
        if (callCount)
            ++(*callCount);
        return makeReadyFuture(Result<QJsonDocument>(makeDoc(plain, synced)));
    };
}

/// Seam that returns an error result.
LyricsController::FetchFn makeErrorFetcher(int* callCount = nullptr)
{
    return [callCount](const QString&, const QString&, const QString&, int)
        -> QFuture<Result<QJsonDocument>>
    {
        if (callCount)
            ++(*callCount);
        return makeReadyFuture(Result<QJsonDocument>::err(
            Error::NetworkError, QStringLiteral("simulated network error")));
    };
}

/// No-op cache store.
LyricsController::CacheStoreFn noopStore()
{
    return [](int, const DatabaseManager::LyricsRow&) {};
}

} // namespace

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestLyricsController : public QObject {
    Q_OBJECT

private slots:

    // (a) Cache hit → lyricsReady with cached plain/synced; fetch NOT invoked.
    void cacheHitEmitsLyricsReady()
    {
        LyricsController ctrl;
        ctrl.setCacheLookup(cacheHit(QStringLiteral("Cached plain"),
                                     QStringLiteral("[00:01.00]Cached synced")));
        int fetchCalls = 0;
        ctrl.setFetcher(makeFetcher(QString(), QString(), &fetchCalls));
        ctrl.setCacheStore(noopStore());

        QSignalSpy spyReady(&ctrl, &LyricsController::lyricsReady);
        QSignalSpy spyCleared(&ctrl, &LyricsController::lyricsCleared);

        ctrl.onTrackChanged(makeTrack(42, QStringLiteral("Artist"), QStringLiteral("Song")));

        // lyricsCleared fires first (synchronously), then lyricsReady (synchronously from cache).
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(spyReady.count(), 1);
        QCOMPARE(fetchCalls, 0); // no network call

        const auto args = spyReady.takeFirst();
        QCOMPARE(args[0].toString(), QStringLiteral("Cached plain"));
        QCOMPARE(args[1].toString(), QStringLiteral("[00:01.00]Cached synced"));
    }

    // (b) Cache miss + empty artist/title → lyricsCleared only; no fetch; no crash.
    void cacheMissEmptyMetadataStaysCleared()
    {
        LyricsController ctrl;
        ctrl.setCacheLookup(cacheMiss());
        int fetchCalls = 0;
        ctrl.setFetcher(makeFetcher(QString(), QString(), &fetchCalls));
        ctrl.setCacheStore(noopStore());

        QSignalSpy spyReady(&ctrl, &LyricsController::lyricsReady);
        QSignalSpy spyCleared(&ctrl, &LyricsController::lyricsCleared);

        // Missing artist
        ctrl.onTrackChanged(makeTrack(1, QString(), QStringLiteral("Song")));
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(spyReady.count(), 0);
        QCOMPARE(fetchCalls, 0);

        // Missing title
        ctrl.onTrackChanged(makeTrack(2, QStringLiteral("Artist"), QString()));
        QCOMPARE(spyCleared.count(), 2);
        QCOMPARE(spyReady.count(), 0);
        QCOMPARE(fetchCalls, 0);

        // Both missing
        ctrl.onTrackChanged(makeTrack(3));
        QCOMPARE(spyCleared.count(), 3);
        QCOMPARE(spyReady.count(), 0);
        QCOMPARE(fetchCalls, 0);
    }

    // (c) Cache miss + valid metadata → fetch invoked, ok result → lyricsReady;
    //     CacheStoreFn is called with the decoded row.
    void cacheMissValidMetadataFetchesAndStores()
    {
        LyricsController ctrl;
        ctrl.setCacheLookup(cacheMiss());
        int fetchCalls = 0;
        ctrl.setFetcher(makeFetcher(QStringLiteral("Network plain"),
                                    QStringLiteral("[00:02.00]Network synced"),
                                    &fetchCalls));

        int storeCalls = 0;
        DatabaseManager::LyricsRow storedRow;
        ctrl.setCacheStore([&](int id, const DatabaseManager::LyricsRow& row) {
            ++storeCalls;
            storedRow = row;
            QCOMPARE(id, 7);
        });

        QSignalSpy spyReady(&ctrl, &LyricsController::lyricsReady);
        QSignalSpy spyCleared(&ctrl, &LyricsController::lyricsCleared);

        ctrl.onTrackChanged(makeTrack(7, QStringLiteral("Artist"), QStringLiteral("Song"),
                                      QStringLiteral("Album"), 240000));

        // lyricsCleared fires synchronously; lyricsReady arrives after the event loop
        // delivers the watcher's finished() signal.
        QCOMPARE(spyCleared.count(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(spyReady.count(), 1, 2000);
        QCOMPARE(fetchCalls, 1);
        QCOMPARE(storeCalls, 1);

        const auto args = spyReady.first();
        QCOMPARE(args[0].toString(), QStringLiteral("Network plain"));
        QCOMPARE(args[1].toString(), QStringLiteral("[00:02.00]Network synced"));
        QCOMPARE(storedRow.plain,  QStringLiteral("Network plain"));
        QCOMPARE(storedRow.synced, QStringLiteral("[00:02.00]Network synced"));
    }

    // (d) STALE GUARD: switch A→B before A's reply delivers; only B's content shown.
    //
    // Both futures are already resolved so their watcher finished() signals are
    // queued for the next event-loop iteration. We call onTrackChanged(A) then
    // onTrackChanged(B) before processing the loop. When the loop runs:
    //   - A's watcher fires: gen(1) != m_generation(2) → dropped
    //   - B's watcher fires: gen(2) == m_generation(2) → lyricsReady with B text
    void staleGuardDropsOldReply()
    {
        LyricsController ctrl;
        ctrl.setCacheLookup(cacheMiss());
        ctrl.setCacheStore(noopStore());

        int callNum = 0;
        ctrl.setFetcher([&](const QString&, const QString&, const QString&, int)
            -> QFuture<Result<QJsonDocument>>
        {
            ++callNum;
            if (callNum == 1)
                return makeReadyFuture(Result<QJsonDocument>(makeDoc(
                    QStringLiteral("A plain"), QStringLiteral("A synced"))));
            return makeReadyFuture(Result<QJsonDocument>(makeDoc(
                QStringLiteral("B plain"), QStringLiteral("B synced"))));
        });

        QSignalSpy spyReady(&ctrl, &LyricsController::lyricsReady);
        QSignalSpy spyCleared(&ctrl, &LyricsController::lyricsCleared);

        // Fire track A — future resolved, watcher's finished() queued.
        ctrl.onTrackChanged(makeTrack(1, QStringLiteral("X"), QStringLiteral("Song A")));
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(spyReady.count(), 0); // not yet delivered

        // Switch to B before the event loop runs — bumps generation.
        ctrl.onTrackChanged(makeTrack(2, QStringLiteral("X"), QStringLiteral("Song B")));
        QCOMPARE(spyCleared.count(), 2);

        // Process event loop: A's reply is stale (dropped), B's reply arrives.
        QTRY_COMPARE_WITH_TIMEOUT(spyReady.count(), 1, 2000);

        const auto args = spyReady.first();
        QCOMPARE(args[0].toString(), QStringLiteral("B plain"));
        QCOMPARE(args[1].toString(), QStringLiteral("B synced"));
        QCOMPARE(callNum, 2); // fetcher called once per track
    }

    // (e) Fetch returns an error Result → non-fatal, stays cleared, no crash.
    void fetchErrorIsNonFatal()
    {
        LyricsController ctrl;
        ctrl.setCacheLookup(cacheMiss());
        ctrl.setCacheStore(noopStore());
        int storeCalls = 0;
        ctrl.setCacheStore([&](int, const DatabaseManager::LyricsRow&) { ++storeCalls; });
        ctrl.setFetcher(makeErrorFetcher());

        QSignalSpy spyReady(&ctrl, &LyricsController::lyricsReady);
        QSignalSpy spyCleared(&ctrl, &LyricsController::lyricsCleared);

        ctrl.onTrackChanged(makeTrack(5, QStringLiteral("Artist"), QStringLiteral("Song")));

        QCOMPARE(spyCleared.count(), 1);
        // Allow the event loop to deliver the error result.
        QTest::qWait(200);
        QCOMPARE(spyReady.count(), 0); // no lyricsReady on error
        QCOMPARE(storeCalls, 0);       // cache not written on error
    }

    // (f) Pure decide() truth table.
    void decideTruthTable()
    {
        using O = LyricsController::Outcome;

        // cache hit → ShowCached regardless of metadata
        QCOMPARE(LyricsController::decide(true, QString(), QString()), O::ShowCached);
        QCOMPARE(LyricsController::decide(true, QStringLiteral("A"), QString()), O::ShowCached);
        QCOMPARE(LyricsController::decide(true, QStringLiteral("A"), QStringLiteral("B")), O::ShowCached);

        // cache miss + empty artist → Empty
        QCOMPARE(LyricsController::decide(false, QString(), QStringLiteral("Song")), O::Empty);
        // cache miss + empty title → Empty
        QCOMPARE(LyricsController::decide(false, QStringLiteral("Artist"), QString()), O::Empty);
        // cache miss + both empty → Empty
        QCOMPARE(LyricsController::decide(false, QString(), QString()), O::Empty);

        // cache miss + both present → Fetch
        QCOMPARE(LyricsController::decide(false, QStringLiteral("Artist"), QStringLiteral("Song")),
                 O::Fetch);
    }

    // (g) Invalid track id (< 0) → lyricsCleared only; controller stays idle.
    void invalidTrackIdStaysCleared()
    {
        LyricsController ctrl;
        ctrl.setCacheLookup(cacheMiss());
        int fetchCalls = 0;
        ctrl.setFetcher(makeFetcher(QString(), QString(), &fetchCalls));
        ctrl.setCacheStore(noopStore());

        QSignalSpy spyReady(&ctrl, &LyricsController::lyricsReady);
        QSignalSpy spyCleared(&ctrl, &LyricsController::lyricsCleared);

        ctrl.onTrackChanged(makeTrack(-1, QStringLiteral("Artist"), QStringLiteral("Song")));

        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(spyReady.count(), 0);
        QCOMPARE(fetchCalls, 0);
    }
};

QTEST_GUILESS_MAIN(TestLyricsController)
#include "test_lyrics_controller.moc"
