#include <QtTest>
#include <QTemporaryDir>

#include "soundshelf/cli/CLIController.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/PodcastStore.hpp"
#include "soundshelf/io/PodcastFeedParser.hpp"
#include "soundshelf/core/Result.hpp"

using namespace soundshelf;

// ---------------------------------------------------------------------------
// RSS fixtures (same shape as test_podcast_manager.cpp)
// ---------------------------------------------------------------------------
namespace {

QByteArray xmlTwoEpisodes()
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<rss version=\"2.0\" xmlns:itunes=\"http://www.itunes.com/dtds/podcast-1.0.dtd\">\n"
           "  <channel>\n"
           "    <title>CLI Test Podcast</title>\n"
           "    <link>https://cli-example.com/podcast</link>\n"
           "    <description>Feed for CLI tests</description>\n"
           "    <language>en-US</language>\n"
           "    <itunes:author>Test Author</itunes:author>\n"
           "    <itunes:image href=\"https://cli-example.com/cover.jpg\"/>\n"
           "    <item>\n"
           "      <title>CLI Episode 1</title>\n"
           "      <guid>https://cli-example.com/ep1</guid>\n"
           "      <pubDate>Mon, 01 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://cli-example.com/ep1.mp3\""
           " type=\"audio/mpeg\" length=\"1000000\"/>\n"
           "      <itunes:duration>01:00:00</itunes:duration>\n"
           "      <itunes:episode>1</itunes:episode>\n"
           "    </item>\n"
           "    <item>\n"
           "      <title>CLI Episode 2</title>\n"
           "      <guid>https://cli-example.com/ep2</guid>\n"
           "      <pubDate>Mon, 08 Jan 2024 12:00:00 +0000</pubDate>\n"
           "      <enclosure url=\"https://cli-example.com/ep2.mp3\""
           " type=\"audio/mpeg\" length=\"2000000\"/>\n"
           "      <itunes:duration>00:30:00</itunes:duration>\n"
           "      <itunes:episode>2</itunes:episode>\n"
           "    </item>\n"
           "  </channel>\n"
           "</rss>";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------
class TestCliPodcast : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString       m_dbPath;

    // Prepend global --db flag so each run() uses the temp DB.
    QStringList mkArgs(QStringList sub) const {
        return QStringList{QStringLiteral("soundshelf"),
                           QStringLiteral("--db"), m_dbPath}
               + sub;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_dbPath = m_dir.filePath(QStringLiteral("cli_podcast.db"));
        auto r = DatabaseManager::instance().open(m_dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
    }

    // -----------------------------------------------------------------------
    // podcast list — seeded via PodcastStore, asserts exit code 0
    // -----------------------------------------------------------------------
    void listReturnsZero() {
        PodcastStore store;
        auto subR = store.subscribe(QStringLiteral("https://cli-test.example.com/list1.rss"));
        QVERIFY(subR.isOk());

        CLIController ctl;
        QCOMPARE(ctl.run(mkArgs({QStringLiteral("podcast"), QStringLiteral("list")})), 0);
    }

    // -----------------------------------------------------------------------
    // podcast list --format json — exit code 0
    // -----------------------------------------------------------------------
    void listJsonReturnsZero() {
        CLIController ctl;
        QCOMPARE(ctl.run(mkArgs({QStringLiteral("--format"), QStringLiteral("json"),
                                 QStringLiteral("podcast"), QStringLiteral("list")})), 0);
    }

    // -----------------------------------------------------------------------
    // podcast episodes <feedId> — seeded episodes via PodcastStore
    // -----------------------------------------------------------------------
    void episodesReturnsZero() {
        PodcastStore store;
        auto subR = store.subscribe(QStringLiteral("https://cli-test.example.com/ep-feed.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        PodcastFeedParser::Episode ep;
        ep.guid          = QStringLiteral("cli-ep-seed-1");
        ep.title         = QStringLiteral("Seeded CLI Episode");
        ep.durationMs    = 1800000;
        ep.episodeNumber = 1;
        QVERIFY(store.upsertEpisodes(feedId, {ep}).isOk());

        CLIController ctl;
        QCOMPARE(ctl.run(mkArgs({QStringLiteral("podcast"),
                                 QStringLiteral("episodes"),
                                 QString::number(feedId)})), 0);
    }

    // -----------------------------------------------------------------------
    // podcast played <epId> — verify is_played flips in store
    // -----------------------------------------------------------------------
    void playedFlipsFlag() {
        PodcastStore store;
        auto subR = store.subscribe(QStringLiteral("https://cli-test.example.com/played-feed.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        PodcastFeedParser::Episode ep;
        ep.guid          = QStringLiteral("cli-played-ep-1");
        ep.title         = QStringLiteral("Played Toggle Episode");
        ep.episodeNumber = 1;
        QVERIFY(store.upsertEpisodes(feedId, {ep}).isOk());

        auto epsR = store.episodesForFeed(feedId);
        QVERIFY(epsR.isOk() && !epsR.value().isEmpty());
        const int epId = epsR.value().first().id;
        QVERIFY(!epsR.value().first().isPlayed);

        // mark as played
        CLIController ctl;
        QCOMPARE(ctl.run(mkArgs({QStringLiteral("podcast"),
                                 QStringLiteral("played"),
                                 QString::number(epId)})), 0);

        auto after = store.episodesForFeed(feedId);
        QVERIFY(after.isOk());
        QVERIFY(after.value().first().isPlayed);

        // unset
        CLIController ctl2;
        QCOMPARE(ctl2.run(mkArgs({QStringLiteral("podcast"),
                                  QStringLiteral("played"),
                                  QString::number(epId),
                                  QStringLiteral("--unset")})), 0);

        auto after2 = store.episodesForFeed(feedId);
        QVERIFY(after2.isOk());
        QVERIFY(!after2.value().first().isPlayed);
    }

    // -----------------------------------------------------------------------
    // podcast unsubscribe <feedId> — feed + episodes gone from store
    // -----------------------------------------------------------------------
    void unsubscribeRemovesFeed() {
        PodcastStore store;
        auto subR = store.subscribe(QStringLiteral("https://cli-test.example.com/unsub-feed.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        CLIController ctl;
        QCOMPARE(ctl.run(mkArgs({QStringLiteral("podcast"),
                                 QStringLiteral("unsubscribe"),
                                 QString::number(feedId)})), 0);

        auto feedR = store.feed(feedId);
        QVERIFY(feedR.isOk());
        QVERIFY(!feedR.value().has_value());
    }

    // -----------------------------------------------------------------------
    // podcast subscribe with injected fetcher — feed inserted
    // -----------------------------------------------------------------------
    void subscribeWithInjectedFetcherInsertsFeed() {
        const QString url = QStringLiteral("https://injected-cli.example.com/feed.rss");

        CLIController ctl;
        ctl.setPodcastFetcherForTesting([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(xmlTwoEpisodes());
        });
        QCOMPARE(ctl.run(mkArgs({QStringLiteral("podcast"),
                                 QStringLiteral("subscribe"),
                                 url})), 0);

        PodcastStore store;
        auto feedsR = store.feeds();
        QVERIFY(feedsR.isOk());
        bool found = false;
        for (const auto& f : feedsR.value()) {
            if (f.url == url) { found = true; break; }
        }
        QVERIFY(found);
    }

    // -----------------------------------------------------------------------
    // podcast refresh --all with injected fetcher — exits 0
    // -----------------------------------------------------------------------
    void refreshAllWithInjectedFetcherReturnsZero() {
        // Ensure at least one subscribed feed exists for refresh to iterate
        PodcastStore store;
        auto subR = store.subscribe(QStringLiteral("https://refresh-all-cli.example.com/f.rss"));
        QVERIFY(subR.isOk());

        CLIController ctl;
        ctl.setPodcastFetcherForTesting([](const QString&) -> Result<QByteArray> {
            return Result<QByteArray>::ok(xmlTwoEpisodes());
        });
        QCOMPARE(ctl.run(mkArgs({QStringLiteral("podcast"),
                                 QStringLiteral("refresh"),
                                 QStringLiteral("--all")})), 0);
    }

    // -----------------------------------------------------------------------
    // Unknown subcommand → non-zero exit
    // -----------------------------------------------------------------------
    void unknownSubcommandReturnsNonZero() {
        CLIController ctl;
        QVERIFY(ctl.run(mkArgs({QStringLiteral("podcast"), QStringLiteral("bogus")})) != 0);
    }

    // -----------------------------------------------------------------------
    // Missing subcommand → non-zero exit
    // -----------------------------------------------------------------------
    void missingSubcommandReturnsNonZero() {
        CLIController ctl;
        QVERIFY(ctl.run(mkArgs({QStringLiteral("podcast")})) != 0);
    }
};

QTEST_GUILESS_MAIN(TestCliPodcast)
#include "test_cli_podcast.moc"
