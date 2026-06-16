#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/PodcastStore.hpp"
#include "soundshelf/io/PodcastFeedParser.hpp"

using namespace soundshelf;

class TestPodcastStore : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    PodcastStore m_store;

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString dbPath = m_dir.filePath(QStringLiteral("podcasts.db"));
        auto r = DatabaseManager::instance().open(dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
    }

    // subscribe is idempotent: same url twice → same id, only one row
    void subscribeIdempotent() {
        const QString url = QStringLiteral("https://example.com/feed1.rss");
        auto r1 = m_store.subscribe(url);
        QVERIFY2(r1.isOk(), qPrintable(r1.isErr() ? r1.error().message : QString()));
        QVERIFY(r1.value() > 0);

        auto r2 = m_store.subscribe(url);
        QVERIFY2(r2.isOk(), qPrintable(r2.isErr() ? r2.error().message : QString()));
        QCOMPARE(r2.value(), r1.value());

        // Confirm only one row
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM podcast_feeds WHERE url = ?"));
        q.addBindValue(url);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    // updateFeedMetadata persists title/author/etc and sets last_refreshed
    void updateFeedMetadataPersistsFields() {
        auto subR = m_store.subscribe(QStringLiteral("https://example.com/feed2.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        PodcastFeedParser::Feed parsed;
        parsed.title       = QStringLiteral("Test Podcast");
        parsed.author      = QStringLiteral("Test Author");
        parsed.description = QStringLiteral("A test feed");
        parsed.imageUrl    = QStringLiteral("https://example.com/img.jpg");
        parsed.link        = QStringLiteral("https://example.com");
        parsed.language    = QStringLiteral("en");

        auto r = m_store.updateFeedMetadata(feedId, parsed);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));

        auto feedR = m_store.feed(feedId);
        QVERIFY(feedR.isOk() && feedR.value().has_value());
        const auto& f = *feedR.value();
        QCOMPARE(f.title,       QStringLiteral("Test Podcast"));
        QCOMPARE(f.author,      QStringLiteral("Test Author"));
        QCOMPARE(f.description, QStringLiteral("A test feed"));
        QCOMPARE(f.imageUrl,    QStringLiteral("https://example.com/img.jpg"));
        QCOMPARE(f.link,        QStringLiteral("https://example.com"));
        QCOMPARE(f.language,    QStringLiteral("en"));
        QVERIFY(f.lastRefreshed.isValid());
    }

    // upsertEpisodes: insert N, then re-upsert same guids inserts 0 new,
    // preserves is_played/local_path but updates title
    void upsertEpisodesInsertsAndPreserves() {
        auto subR = m_store.subscribe(QStringLiteral("https://example.com/feed3.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        QList<PodcastFeedParser::Episode> eps;
        {
            PodcastFeedParser::Episode e1;
            e1.guid  = QStringLiteral("guid-ep1");
            e1.title = QStringLiteral("Episode 1");
            e1.durationMs = 60000;
            eps.append(e1);

            PodcastFeedParser::Episode e2;
            e2.guid  = QStringLiteral("guid-ep2");
            e2.title = QStringLiteral("Episode 2");
            e2.durationMs = 120000;
            eps.append(e2);
        }

        // First upsert: inserts 2
        auto r1 = m_store.upsertEpisodes(feedId, eps);
        QVERIFY2(r1.isOk(), qPrintable(r1.isErr() ? r1.error().message : QString()));
        QCOMPARE(r1.value(), 2);

        // Mark ep1 played and set a local path via DB helpers
        auto epList = m_store.episodesForFeed(feedId);
        QVERIFY(epList.isOk() && epList.value().size() == 2);
        int ep1Id = -1;
        for (const auto& ep : epList.value()) {
            if (ep.guid == QStringLiteral("guid-ep1")) ep1Id = ep.id;
        }
        QVERIFY(ep1Id > 0);
        QVERIFY(m_store.setPlayed(ep1Id, true).isOk());
        QVERIFY(m_store.setLocalPath(ep1Id, QStringLiteral("/tmp/ep1.mp3")).isOk());

        // Modify title in re-upsert
        eps[0].title = QStringLiteral("Episode 1 Updated");

        // Second upsert of same guids: inserts 0 new
        auto r2 = m_store.upsertEpisodes(feedId, eps);
        QVERIFY2(r2.isOk(), qPrintable(r2.isErr() ? r2.error().message : QString()));
        QCOMPARE(r2.value(), 0);

        // is_played and local_path must be preserved; title must be updated
        auto epList2 = m_store.episodesForFeed(feedId);
        QVERIFY(epList2.isOk());
        for (const auto& ep : epList2.value()) {
            if (ep.guid == QStringLiteral("guid-ep1")) {
                QVERIFY(ep.isPlayed);
                QCOMPARE(ep.localPath, QStringLiteral("/tmp/ep1.mp3"));
                QCOMPARE(ep.title, QStringLiteral("Episode 1 Updated"));
            }
        }
    }

    // episodesForFeed ordered by pub_date DESC (NULLs last)
    void episodesOrderedByPubDateDesc() {
        auto subR = m_store.subscribe(QStringLiteral("https://example.com/feed4.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        QList<PodcastFeedParser::Episode> eps;
        {
            PodcastFeedParser::Episode older;
            older.guid    = QStringLiteral("older");
            older.title   = QStringLiteral("Older");
            older.pubDate = QDateTime::fromString(QStringLiteral("2024-01-01T00:00:00"),
                                                  Qt::ISODate);
            eps.append(older);

            PodcastFeedParser::Episode newer;
            newer.guid    = QStringLiteral("newer");
            newer.title   = QStringLiteral("Newer");
            newer.pubDate = QDateTime::fromString(QStringLiteral("2024-06-01T00:00:00"),
                                                  Qt::ISODate);
            eps.append(newer);

            PodcastFeedParser::Episode noDate;
            noDate.guid  = QStringLiteral("nodate");
            noDate.title = QStringLiteral("No date");
            // pubDate left invalid — should appear last
            eps.append(noDate);
        }

        QVERIFY(m_store.upsertEpisodes(feedId, eps).isOk());

        auto list = m_store.episodesForFeed(feedId);
        QVERIFY2(list.isOk(), qPrintable(list.isErr() ? list.error().message : QString()));
        QCOMPARE(list.value().size(), 3);
        QCOMPARE(list.value()[0].guid, QStringLiteral("newer"));
        QCOMPARE(list.value()[1].guid, QStringLiteral("older"));
        QCOMPARE(list.value()[2].guid, QStringLiteral("nodate"));
    }

    // setPlayed / setLocalPath round-trip
    void setPlayedAndLocalPathRoundTrip() {
        auto subR = m_store.subscribe(QStringLiteral("https://example.com/feed5.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        QList<PodcastFeedParser::Episode> eps;
        PodcastFeedParser::Episode e;
        e.guid  = QStringLiteral("rt-guid");
        e.title = QStringLiteral("RT Episode");
        eps.append(e);
        QVERIFY(m_store.upsertEpisodes(feedId, eps).isOk());

        auto list = m_store.episodesForFeed(feedId);
        QVERIFY(list.isOk() && !list.value().isEmpty());
        const int epId = list.value().first().id;

        // Default: not played, no local path
        QVERIFY(!list.value().first().isPlayed);
        QVERIFY(list.value().first().localPath.isEmpty());

        QVERIFY(m_store.setPlayed(epId, true).isOk());
        QVERIFY(m_store.setLocalPath(epId, QStringLiteral("/mnt/music/rt.mp3")).isOk());

        auto updated = m_store.episodesForFeed(feedId);
        QVERIFY(updated.isOk() && !updated.value().isEmpty());
        QVERIFY(updated.value().first().isPlayed);
        QCOMPARE(updated.value().first().localPath, QStringLiteral("/mnt/music/rt.mp3"));

        // Toggle back
        QVERIFY(m_store.setPlayed(epId, false).isOk());
        auto toggled = m_store.episodesForFeed(feedId);
        QVERIFY(toggled.isOk());
        QVERIFY(!toggled.value().first().isPlayed);
    }

    // unsubscribe cascades — episodes gone
    void unsubscribeCascadesEpisodes() {
        auto subR = m_store.subscribe(QStringLiteral("https://example.com/feed6.rss"));
        QVERIFY(subR.isOk());
        const int feedId = subR.value();

        QList<PodcastFeedParser::Episode> eps;
        PodcastFeedParser::Episode e;
        e.guid  = QStringLiteral("cascade-guid");
        e.title = QStringLiteral("Cascade Ep");
        eps.append(e);
        QVERIFY(m_store.upsertEpisodes(feedId, eps).isOk());

        // Confirm episode exists
        auto before = m_store.episodesForFeed(feedId);
        QVERIFY(before.isOk() && before.value().size() == 1);

        auto delR = m_store.unsubscribe(feedId);
        QVERIFY2(delR.isOk(), qPrintable(delR.isErr() ? delR.error().message : QString()));

        // feed() must return nullopt
        auto feedR = m_store.feed(feedId);
        QVERIFY(feedR.isOk() && !feedR.value().has_value());

        // Episodes must be gone (cascade)
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM podcast_episodes WHERE feed_id = ?"));
        q.addBindValue(feedId);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    // feeds() returns all feeds ordered by title
    void feedsOrderedByTitle() {
        m_store.subscribe(QStringLiteral("https://example.com/zz.rss"));
        m_store.subscribe(QStringLiteral("https://example.com/aa.rss"));

        // Update titles so the ordering is deterministic
        PodcastFeedParser::Feed fz;
        fz.title = QStringLiteral("ZZZ Feed");
        PodcastFeedParser::Feed fa;
        fa.title = QStringLiteral("AAA Feed");

        auto zSub = m_store.subscribe(QStringLiteral("https://example.com/zz.rss"));
        auto aSub = m_store.subscribe(QStringLiteral("https://example.com/aa.rss"));
        QVERIFY(zSub.isOk() && aSub.isOk());
        m_store.updateFeedMetadata(zSub.value(), fz);
        m_store.updateFeedMetadata(aSub.value(), fa);

        auto all = m_store.feeds();
        QVERIFY2(all.isOk(), qPrintable(all.isErr() ? all.error().message : QString()));
        // Find our two feeds and confirm relative ordering
        int idxA = -1, idxZ = -1;
        for (int i = 0; i < all.value().size(); ++i) {
            if (all.value()[i].title == QStringLiteral("AAA Feed")) idxA = i;
            if (all.value()[i].title == QStringLiteral("ZZZ Feed")) idxZ = i;
        }
        QVERIFY(idxA >= 0 && idxZ >= 0);
        QVERIFY(idxA < idxZ);
    }
};

QTEST_GUILESS_MAIN(TestPodcastStore)
#include "test_podcast_store.moc"
