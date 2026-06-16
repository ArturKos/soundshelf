#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/BookmarkStore.hpp"

using namespace soundshelf;

class TestBookmarkStore : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    BookmarkStore m_store;
    int m_trackId = -1;

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString dbPath = m_dir.filePath(QStringLiteral("lib.db"));
        auto r = DatabaseManager::instance().open(dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));

        // Insert a minimal tracks row to satisfy FK constraints.
        // title, filepath and filename are NOT NULL in the schema.
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral(
            "INSERT INTO tracks(filepath, filename, title) VALUES (?, ?, ?)"));
        q.addBindValue(QStringLiteral("/tmp/test_track.mp3"));
        q.addBindValue(QStringLiteral("test_track.mp3"));
        q.addBindValue(QStringLiteral("Test Track"));
        QVERIFY2(q.exec(), qPrintable(q.lastError().text()));
        m_trackId = q.lastInsertId().toInt();
        QVERIFY(m_trackId > 0);
    }

    void setResumeThenRead() {
        auto r = m_store.setResumePosition(m_trackId, 12345);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));

        auto pos = m_store.resumePosition(m_trackId);
        QVERIFY(pos.isOk());
        QVERIFY(pos.value().has_value());
        QCOMPARE(*pos.value(), 12345);
    }

    void setResumeUpdatesNotDuplicates() {
        // Call setResumePosition twice — should result in exactly one is_resume row
        auto r1 = m_store.setResumePosition(m_trackId, 1000);
        QVERIFY(r1.isOk());
        auto r2 = m_store.setResumePosition(m_trackId, 2000);
        QVERIFY(r2.isOk());

        // Verify updated value
        auto pos = m_store.resumePosition(m_trackId);
        QVERIFY(pos.isOk());
        QVERIFY(pos.value().has_value());
        QCOMPARE(*pos.value(), 2000);

        // Verify there is only one is_resume row for this track
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM bookmarks WHERE track_id = ? AND is_resume = 1"));
        q.addBindValue(m_trackId);
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void clearResumeReturnsNullopt() {
        auto set = m_store.setResumePosition(m_trackId, 5000);
        QVERIFY(set.isOk());

        auto clr = m_store.clearResume(m_trackId);
        QVERIFY2(clr.isOk(), qPrintable(clr.isErr() ? clr.error().message : QString()));

        auto pos = m_store.resumePosition(m_trackId);
        QVERIFY(pos.isOk());
        QVERIFY(!pos.value().has_value());
    }

    void addBookmarksAndListOrdered() {
        // Clean slate: remove any leftover named bookmarks
        m_store.removeAllForTrack(m_trackId);

        auto id1 = m_store.addBookmark(m_trackId, 3000, QStringLiteral("B"));
        auto id2 = m_store.addBookmark(m_trackId, 1000, QStringLiteral("A"));
        auto id3 = m_store.addBookmark(m_trackId, 2000, QStringLiteral("C"));
        QVERIFY(id1.isOk());
        QVERIFY(id2.isOk());
        QVERIFY(id3.isOk());
        QVERIFY(id1.value() > 0);
        QVERIFY(id2.value() > 0);
        QVERIFY(id3.value() > 0);

        auto list = m_store.bookmarksForTrack(m_trackId);
        QVERIFY2(list.isOk(), qPrintable(list.isErr() ? list.error().message : QString()));
        QCOMPARE(list.value().size(), 3);
        // Ordered by position_ms ASC
        QCOMPARE(list.value()[0].positionMs, 1000);
        QCOMPARE(list.value()[1].positionMs, 2000);
        QCOMPARE(list.value()[2].positionMs, 3000);
        QCOMPARE(list.value()[0].label, QStringLiteral("A"));
        // All must be named (not resume)
        for (const auto& bm : list.value()) {
            QVERIFY(!bm.isResume);
        }
    }

    void removeBookmarkDeletesOne() {
        m_store.removeAllForTrack(m_trackId);
        auto id1 = m_store.addBookmark(m_trackId, 100, QStringLiteral("X"));
        auto id2 = m_store.addBookmark(m_trackId, 200, QStringLiteral("Y"));
        QVERIFY(id1.isOk());
        QVERIFY(id2.isOk());

        auto rem = m_store.removeBookmark(id1.value());
        QVERIFY2(rem.isOk(), qPrintable(rem.isErr() ? rem.error().message : QString()));

        auto list = m_store.bookmarksForTrack(m_trackId);
        QVERIFY(list.isOk());
        QCOMPARE(list.value().size(), 1);
        QCOMPARE(list.value()[0].id, id2.value());
    }

    void removeAllForTrackReturnsCount() {
        m_store.removeAllForTrack(m_trackId);
        m_store.addBookmark(m_trackId, 10, QStringLiteral("p"));
        m_store.addBookmark(m_trackId, 20, QStringLiteral("q"));
        m_store.setResumePosition(m_trackId, 15);

        auto r = m_store.removeAllForTrack(m_trackId);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value(), 3);  // 2 named + 1 resume

        auto list = m_store.bookmarksForTrack(m_trackId);
        QVERIFY(list.isOk());
        QVERIFY(list.value().isEmpty());

        auto pos = m_store.resumePosition(m_trackId);
        QVERIFY(pos.isOk());
        QVERIFY(!pos.value().has_value());
    }

    void resumePositionUnknownTrackReturnsNullopt() {
        // Use a track_id that does not exist in the DB
        auto pos = m_store.resumePosition(999999);
        QVERIFY(pos.isOk());
        QVERIFY(!pos.value().has_value());
    }
};

QTEST_GUILESS_MAIN(TestBookmarkStore)
#include "test_bookmark_store.moc"
