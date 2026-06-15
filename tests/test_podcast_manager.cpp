#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/PodcastStore.hpp"
#include "soundshelf/core/PodcastManager.hpp"

using namespace soundshelf;

// ---------------------------------------------------------------------------
// RSS fixtures — same shape as test_podcast_feed_parser.cpp
// ---------------------------------------------------------------------------
namespace {

QByteArray xmlTwoEpisodes()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<rss version=\"2.0\" xmlns:itunes=\"http://www.itunes.com/dtds/podcast-1.0.dtd\">\n"
           "  <channel>\n"
           "    <title>Test Podcast</title>\n"
           "    <link>https://example.com/podcast</link>\n"
           "    <description>A test podcast feed</description>\n"
           "    <language>en-US</language>\n"
           "    <itunes:author>Jane Doe</itunes:author>\n"
           "    <itunes:image href=\"https://example.com/cover.jpg\"/>\n"
           "    <item>\n"
           "      <title>Episode 1: The Beginning</title>\n"
           "      <guid>https://example.com/ep1</guid>\n"
           "      <pubDate>Mon, 01 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://example.com/ep1.mp3\""
           " type=\"audio/mpeg\" length=\"12345678\"/>\n"
           "      <itunes:duration>01:02:03</itunes:duration>\n"
           "      <itunes:episode>1</itunes:episode>\n"
           "      <description>First episode description.</description>\n"
           "    </item>\n"
           "    <item>\n"
           "      <title>Episode 2: The Return</title>\n"
           "      <guid>https://example.com/ep2</guid>\n"
           "      <pubDate>Mon, 08 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://example.com/ep2.mp3\""
           " type=\"audio/mpeg\" length=\"98765432\"/>\n"
           "      <itunes:duration>00:45:30</itunes:duration>\n"
           "      <itunes:episode>2</itunes:episode>\n"
           "    </item>\n"
           "  </channel>\n"
           "</rss>";
}

QByteArray xmlThreeEpisodes()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<rss version=\"2.0\" xmlns:itunes=\"http://www.itunes.com/dtds/podcast-1.0.dtd\">\n"
           "  <channel>\n"
           "    <title>Test Podcast</title>\n"
           "    <link>https://example.com/podcast</link>\n"
           "    <description>A test podcast feed</description>\n"
           "    <language>en-US</language>\n"
           "    <itunes:author>Jane Doe</itunes:author>\n"
           "    <itunes:image href=\"https://example.com/cover.jpg\"/>\n"
           "    <item>\n"
           "      <title>Episode 1: The Beginning</title>\n"
           "      <guid>https://example.com/ep1</guid>\n"
           "      <pubDate>Mon, 01 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://example.com/ep1.mp3\""
           " type=\"audio/mpeg\" length=\"12345678\"/>\n"
           "      <itunes:duration>01:02:03</itunes:duration>\n"
           "      <itunes:episode>1</itunes:episode>\n"
           "      <description>First episode description.</description>\n"
           "    </item>\n"
           "    <item>\n"
           "      <title>Episode 2: The Return</title>\n"
           "      <guid>https://example.com/ep2</guid>\n"
           "      <pubDate>Mon, 08 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://example.com/ep2.mp3\""
           " type=\"audio/mpeg\" length=\"98765432\"/>\n"
           "      <itunes:duration>00:45:30</itunes:duration>\n"
           "      <itunes:episode>2</itunes:episode>\n"
           "    </item>\n"
           "    <item>\n"
           "      <title>Episode 3: The Finale</title>\n"
           "      <guid>https://example.com/ep3</guid>\n"
           "      <pubDate>Mon, 15 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://example.com/ep3.mp3\""
           " type=\"audio/mpeg\" length=\"11111111\"/>\n"
           "      <itunes:duration>00:30:00</itunes:duration>\n"
           "      <itunes:episode>3</itunes:episode>\n"
           "    </item>\n"
           "  </channel>\n"
           "</rss>";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestPodcastManager : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:

    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        const QString dbPath = m_dir.filePath(QStringLiteral("pm_test.db"));
        auto r = DatabaseManager::instance().open(dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
    }

    // -----------------------------------------------------------------------
    // (1) subscribe() with a 2-item feed → feed metadata + 2 episodes persisted
    // -----------------------------------------------------------------------
    void subscribeCreatesAndPersistsFeedAndEpisodes()
    {
        PodcastManager manager;
        manager.setFeedFetcher([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(xmlTwoEpisodes());
        });

        const QString url = QStringLiteral("https://pm-test.example.com/feed1.rss");
        auto r = manager.subscribe(url);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        const int feedId = r.value();
        QVERIFY(feedId > 0);

        // Verify feed metadata via a separate store using the same DB
        PodcastStore store;
        auto feedR = store.feed(feedId);
        QVERIFY(feedR.isOk() && feedR.value().has_value());
        QCOMPARE(feedR.value()->title,  QStringLiteral("Test Podcast"));
        QCOMPARE(feedR.value()->author, QStringLiteral("Jane Doe"));
        QCOMPARE(feedR.value()->url,    url);
        QVERIFY(feedR.value()->lastRefreshed.isValid());

        // Verify 2 episodes
        auto epsR = store.episodesForFeed(feedId);
        QVERIFY(epsR.isOk());
        QCOMPARE(epsR.value().size(), 2);
    }

    // -----------------------------------------------------------------------
    // (2) refreshFeed() with 3-episode feed after a 2-episode subscribe:
    //     returns 1 new episode; existing episode's is_played/local_path preserved
    // -----------------------------------------------------------------------
    void refreshFeedAddsNewEpisodePreservesExisting()
    {
        PodcastManager manager;
        const QString url = QStringLiteral("https://pm-test.example.com/feed2.rss");

        manager.setFeedFetcher([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(xmlTwoEpisodes());
        });
        auto subR = manager.subscribe(url);
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        // Mark episode 1 as played + set a local path via a separate store
        PodcastStore store;
        auto epsR = store.episodesForFeed(feedId);
        QVERIFY(epsR.isOk() && epsR.value().size() == 2);
        int ep1Id = -1;
        for (const auto& ep : epsR.value()) {
            if (ep.guid == QStringLiteral("https://example.com/ep1"))
                ep1Id = ep.id;
        }
        QVERIFY(ep1Id > 0);
        QVERIFY(store.setPlayed(ep1Id, true).isOk());
        QVERIFY(store.setLocalPath(ep1Id, QStringLiteral("/tmp/ep1.mp3")).isOk());

        // Refresh with 3-episode feed
        manager.setFeedFetcher([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(xmlThreeEpisodes());
        });

        QSignalSpy spy(&manager, &PodcastManager::feedRefreshed);
        auto refreshR = manager.refreshFeed(feedId);
        QVERIFY2(refreshR.isOk(), qPrintable(refreshR.isErr() ? refreshR.error().message : QString()));
        QCOMPARE(refreshR.value(), 1); // exactly 1 new episode

        // Total episodes must now be 3
        auto eps2R = store.episodesForFeed(feedId);
        QVERIFY(eps2R.isOk());
        QCOMPARE(eps2R.value().size(), 3);

        // is_played and local_path on episode 1 must be preserved
        bool ep1Found = false;
        for (const auto& ep : eps2R.value()) {
            if (ep.guid == QStringLiteral("https://example.com/ep1")) {
                QVERIFY(ep.isPlayed);
                QCOMPARE(ep.localPath, QStringLiteral("/tmp/ep1.mp3"));
                ep1Found = true;
            }
        }
        QVERIFY(ep1Found);

        // feedRefreshed signal emitted once with correct args
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), feedId);
        QCOMPARE(spy.at(0).at(1).toInt(), 1);
    }

    // -----------------------------------------------------------------------
    // (3) Fetcher returning Err → PodcastManager returns Err, errorOccurred emitted
    // -----------------------------------------------------------------------
    void errorFetcherEmitsErrorOccurred()
    {
        PodcastManager manager;
        manager.setFeedFetcher([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::err(Error::NetworkError,
                                          QStringLiteral("Connection refused"));
        });

        QSignalSpy spy(&manager, &PodcastManager::errorOccurred);

        // subscribe() must fail and emit the signal
        auto subR = manager.subscribe(QStringLiteral("https://unreachable.example.com/f.rss"));
        QVERIFY(subR.isErr());
        QCOMPARE(spy.count(), 1);
        QVERIFY(!spy.at(0).at(0).toString().isEmpty());

        // Subscribe a valid feed first to get a feedId for refreshFeed testing
        manager.setFeedFetcher([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(xmlTwoEpisodes());
        });
        auto subR2 = manager.subscribe(QStringLiteral("https://pm-test.example.com/feed3err.rss"));
        QVERIFY(subR2.isOk());
        const int feedId = subR2.value();

        // Break fetcher again and call refreshFeed
        spy.clear();
        manager.setFeedFetcher([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::err(Error::NetworkError,
                                          QStringLiteral("Timeout"));
        });
        auto refreshR = manager.refreshFeed(feedId);
        QVERIFY(refreshR.isErr());
        QCOMPARE(spy.count(), 1);
    }

    // -----------------------------------------------------------------------
    // (4) downloadEpisode() with stub enclosure fetcher: file written, store updated,
    //     episodeDownloaded signal emitted
    // -----------------------------------------------------------------------
    void downloadEpisodeWritesFileAndPersistsPath()
    {
        PodcastManager manager;
        manager.setFeedFetcher([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(xmlTwoEpisodes());
        });

        const QString url = QStringLiteral("https://pm-test.example.com/feed4.rss");
        auto subR = manager.subscribe(url);
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        // Grab first episode id via a separate store
        PodcastStore store;
        auto epsR = store.episodesForFeed(feedId);
        QVERIFY(epsR.isOk() && !epsR.value().isEmpty());
        // episodesForFeed returns pub_date DESC — ep2 (Jan 08) is first
        const int episodeId = epsR.value().first().id;

        const QByteArray fakeAudio("FAKE_MP3_BYTES_FOR_TEST", 23);
        manager.setEnclosureFetcher([&fakeAudio](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(fakeAudio);
        });

        QSignalSpy spy(&manager, &PodcastManager::episodeDownloaded);
        auto dlR = manager.downloadEpisode(episodeId, m_dir.path());
        QVERIFY2(dlR.isOk(), qPrintable(dlR.isErr() ? dlR.error().message : QString()));

        const QString path = dlR.value();
        QVERIFY(!path.isEmpty());

        // File must exist with the exact bytes written
        QFile f(path);
        QVERIFY(f.exists());
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), fakeAudio);
        f.close();

        // Store must reflect local_path for the downloaded episode
        auto updatedEps = store.episodesForFeed(feedId);
        QVERIFY(updatedEps.isOk());
        bool found = false;
        for (const auto& ep : updatedEps.value()) {
            if (ep.id == episodeId) {
                QCOMPARE(ep.localPath, path);
                found = true;
            }
        }
        QVERIFY(found);

        // episodeDownloaded signal emitted with correct args
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), episodeId);
        QCOMPARE(spy.at(0).at(1).toString(), path);
    }
};

QTEST_APPLESS_MAIN(TestPodcastManager)
#include "test_podcast_manager.moc"
