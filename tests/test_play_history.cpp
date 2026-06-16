#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/PlayHistory.hpp"
#include "soundshelf/core/Track.hpp"

using namespace soundshelf;

class TestPlayHistory : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    PlayHistory   m_ph;
    int m_trackWithArtist = -1;  ///< track that has a named artist
    int m_trackNoArtist   = -1;  ///< track with no artist_id (NULL)

private:
    /// Deletes all play_history rows so each test starts with a clean slate.
    static void clearHistory() {
        QSqlQuery q(DatabaseManager::instance().database());
        q.exec(QStringLiteral("DELETE FROM play_history"));
    }

    /// Inserts a play_history row with a caller-supplied played_at timestamp
    /// (ISO 8601 string, e.g. "2024-01-01 10:00:00").  Returns true on success.
    static bool insertHistoryAt(int trackId, const QString& playedAt,
                                int playedMs, bool completed)
    {
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral(
            "INSERT INTO play_history(track_id, played_at, played_ms, completed, source) "
            "VALUES (?, ?, ?, ?, 'test')"));
        q.addBindValue(trackId);
        q.addBindValue(playedAt);
        q.addBindValue(playedMs);
        q.addBindValue(completed ? 1 : 0);
        return q.exec();
    }

    /// Reads play_count for @p trackId directly from the tracks table.
    static int readPlayCount(int trackId) {
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral("SELECT play_count FROM tracks WHERE id = ?"));
        q.addBindValue(trackId);
        if (q.exec() && q.next())
            return q.value(0).toInt();
        return -1;
    }

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString dbPath = m_dir.filePath(QStringLiteral("ph_test.db"));
        auto r = DatabaseManager::instance().open(dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));

        // Seed one artist
        auto ar = DatabaseManager::instance().ensureArtist(QStringLiteral("Kraftwerk"));
        QVERIFY2(ar.isOk(), qPrintable(ar.isErr() ? ar.error().message : QString()));
        const int artistId = ar.value();

        // Track 1: has a named artist
        QSqlQuery q1(DatabaseManager::instance().database());
        q1.prepare(QStringLiteral(
            "INSERT INTO tracks(filepath, filename, title, artist_id, format) "
            "VALUES (?, ?, ?, ?, 'FLAC')"));
        q1.addBindValue(QStringLiteral("/music/autobahn.flac"));
        q1.addBindValue(QStringLiteral("autobahn.flac"));
        q1.addBindValue(QStringLiteral("Autobahn"));
        q1.addBindValue(artistId);
        QVERIFY2(q1.exec(), qPrintable(q1.lastError().text()));
        m_trackWithArtist = q1.lastInsertId().toInt();
        QVERIFY(m_trackWithArtist > 0);

        // Track 2: no artist_id (NULL)
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral(
            "INSERT INTO tracks(filepath, filename, title) VALUES (?, ?, ?)"));
        q.addBindValue(QStringLiteral("/music/computer_world.ogg"));
        q.addBindValue(QStringLiteral("computer_world.ogg"));
        q.addBindValue(QStringLiteral("Computer World"));
        QVERIFY2(q.exec(), qPrintable(q.lastError().text()));
        m_trackNoArtist = q.lastInsertId().toInt();
        QVERIFY(m_trackNoArtist > 0);
    }

    // ------------------------------------------------------------------ (1)
    /** recordPlay(completed=true) returns Ok with a positive id, inserts a row
     *  in play_history, bumps tracks.play_count by 1, sets last_played, and
     *  leaves the track findable in tracks_fts — proving migration 008 fixed
     *  the contentless-FTS5 DELETE bug in the tracks_au trigger. */
    void recordPlayCompletedInsertsAndBumps() {
        const int before = readPlayCount(m_trackWithArtist);
        QVERIFY(before >= 0);

        auto r = m_ph.recordPlay(m_trackWithArtist, 60000, true);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QVERIFY(r.value() > 0);

        QCOMPARE(readPlayCount(m_trackWithArtist), before + 1);

        QSqlQuery lp(DatabaseManager::instance().database());
        lp.prepare(QStringLiteral("SELECT last_played FROM tracks WHERE id = ?"));
        lp.addBindValue(m_trackWithArtist);
        QVERIFY2(lp.exec() && lp.next(), qPrintable(lp.lastError().text()));
        QVERIFY2(!lp.value(0).isNull(),
                 "last_played should be set after a completed play");

        // FTS integrity: tracks_au fired (migration 008 fix); the track must
        // still be searchable by its title after the UPDATE on tracks.
        {
            QSqlQuery fts(DatabaseManager::instance().database());
            fts.prepare(QStringLiteral(
                "SELECT rowid FROM tracks_fts WHERE tracks_fts MATCH ?"));
            fts.addBindValue(QStringLiteral("Autobahn"));
            QVERIFY2(fts.exec(), qPrintable(fts.lastError().text()));
            bool found = false;
            while (fts.next()) {
                if (fts.value(0).toInt() == m_trackWithArtist)
                    found = true;
            }
            QVERIFY2(found, "track must still be findable in FTS after tracks_au fired");
        }

        // Rename: new title becomes searchable; old title stops matching this rowid.
        {
            QSqlQuery upQ(DatabaseManager::instance().database());
            upQ.prepare(QStringLiteral("UPDATE tracks SET title = ? WHERE id = ?"));
            upQ.addBindValue(QStringLiteral("Trans-Europa Express"));
            upQ.addBindValue(m_trackWithArtist);
            QVERIFY2(upQ.exec(), qPrintable(upQ.lastError().text()));

            QSqlQuery ftsNew(DatabaseManager::instance().database());
            ftsNew.prepare(QStringLiteral(
                "SELECT rowid FROM tracks_fts WHERE tracks_fts MATCH ?"));
            ftsNew.addBindValue(QStringLiteral("Europa"));
            QVERIFY2(ftsNew.exec(), qPrintable(ftsNew.lastError().text()));
            bool foundNew = false;
            while (ftsNew.next()) {
                if (ftsNew.value(0).toInt() == m_trackWithArtist)
                    foundNew = true;
            }
            QVERIFY2(foundNew, "renamed title must be searchable in FTS");

            // "Autobahn" no longer in FTS for this track (m_trackNoArtist title is "Computer World")
            QSqlQuery ftsOld(DatabaseManager::instance().database());
            ftsOld.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM tracks_fts WHERE tracks_fts MATCH ?"));
            ftsOld.addBindValue(QStringLiteral("Autobahn"));
            QVERIFY2(ftsOld.exec() && ftsOld.next(), qPrintable(ftsOld.lastError().text()));
            QCOMPARE(ftsOld.value(0).toInt(), 0);

            // Restore title so subsequent tests work correctly
            QSqlQuery restoreQ(DatabaseManager::instance().database());
            restoreQ.prepare(QStringLiteral("UPDATE tracks SET title = ? WHERE id = ?"));
            restoreQ.addBindValue(QStringLiteral("Autobahn"));
            restoreQ.addBindValue(m_trackWithArtist);
            QVERIFY2(restoreQ.exec(), qPrintable(restoreQ.lastError().text()));
        }

        // Delete: a track removed from tracks also disappears from tracks_fts.
        {
            QSqlQuery insQ(DatabaseManager::instance().database());
            insQ.prepare(QStringLiteral(
                "INSERT INTO tracks(filepath, filename, title) VALUES (?, ?, ?)"));
            insQ.addBindValue(QStringLiteral("/tmp/radioactivity.mp3"));
            insQ.addBindValue(QStringLiteral("radioactivity.mp3"));
            insQ.addBindValue(QStringLiteral("Radioactivity"));
            QVERIFY2(insQ.exec(), qPrintable(insQ.lastError().text()));
            const int tmpId = insQ.lastInsertId().toInt();

            QSqlQuery delQ(DatabaseManager::instance().database());
            delQ.prepare(QStringLiteral("DELETE FROM tracks WHERE id = ?"));
            delQ.addBindValue(tmpId);
            QVERIFY2(delQ.exec(), qPrintable(delQ.lastError().text()));

            QSqlQuery ftsQ(DatabaseManager::instance().database());
            ftsQ.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM tracks_fts WHERE tracks_fts MATCH ?"));
            ftsQ.addBindValue(QStringLiteral("Radioactivity"));
            QVERIFY2(ftsQ.exec() && ftsQ.next(), qPrintable(ftsQ.lastError().text()));
            QCOMPARE(ftsQ.value(0).toInt(), 0);
        }
    }

    /** recordPlay(completed=false) inserts a row but does NOT bump play_count. */
    void recordPlayNotCompletedDoesNotBump() {
        const int before = readPlayCount(m_trackWithArtist);
        QVERIFY(before >= 0);

        auto r = m_ph.recordPlay(m_trackWithArtist, 15000, false);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QVERIFY(r.value() > 0);

        QCOMPARE(readPlayCount(m_trackWithArtist), before);
    }

    // ------------------------------------------------------------------ (2)
    /** recent(limit) returns newest-first and honours the limit. */
    void recentNewestFirstAndLimit() {
        clearHistory();
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-01-01 10:00:00"), 10000, true));
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-01-02 10:00:00"), 20000, true));
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-01-03 10:00:00"), 30000, true));

        auto r = m_ph.recent(2);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 2);
        // Newest entry has played_ms=30000
        QCOMPARE(r.value()[0].playedMs, 30000);
        QCOMPARE(r.value()[1].playedMs, 20000);
    }

    // ------------------------------------------------------------------ (3)
    /** forTrack(trackId) returns only rows for the requested track. */
    void forTrackReturnsOnlyThatTrack() {
        clearHistory();
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-02-01 10:00:00"), 1000, true));
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-02-02 10:00:00"), 2000, true));
        QVERIFY(insertHistoryAt(m_trackNoArtist,   QStringLiteral("2025-02-03 10:00:00"), 3000, true));

        auto r = m_ph.forTrack(m_trackWithArtist, 50);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 2);
        for (const auto& e : r.value())
            QCOMPARE(e.trackId, m_trackWithArtist);
    }

    // ------------------------------------------------------------------ (4)
    /** topTracks counts only completed=1, orders by play count DESC, honours
     *  limit, and builds correct labels (artist — title vs. title-only). */
    void topTracksCompletedOnlyOrderedAndLabeled() {
        clearHistory();
        // m_trackWithArtist: 2 completed plays
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-03-01 10:00:00"), 60000, true));
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-03-02 10:00:00"), 60000, true));
        // m_trackNoArtist: 1 completed + 1 non-completed (non-completed must NOT be counted)
        QVERIFY(insertHistoryAt(m_trackNoArtist, QStringLiteral("2025-03-03 10:00:00"), 60000, true));
        QVERIFY(insertHistoryAt(m_trackNoArtist, QStringLiteral("2025-03-04 10:00:00"), 60000, false));

        auto r = m_ph.topTracks(25, 0);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 2);

        // First entry: trackWithArtist, 2 completed plays
        QCOMPARE(r.value()[0].trackId, m_trackWithArtist);
        QCOMPARE(r.value()[0].playCount, 2);
        // Label format: "Artist — Title"
        QCOMPARE(r.value()[0].label,
                 QStringLiteral("Kraftwerk — Autobahn"));

        // Second entry: trackNoArtist, 1 completed play
        QCOMPARE(r.value()[1].trackId, m_trackNoArtist);
        QCOMPARE(r.value()[1].playCount, 1);
        QCOMPARE(r.value()[1].label, QStringLiteral("Computer World"));

        // Limit is honoured: only the top-1 track is returned when limit=1
        auto rLimited = m_ph.topTracks(1, 0);
        QVERIFY2(rLimited.isOk(), qPrintable(rLimited.isErr() ? rLimited.error().message : QString()));
        QCOMPARE(rLimited.value().size(), 1);
        QCOMPARE(rLimited.value()[0].trackId, m_trackWithArtist);
    }

    // ------------------------------------------------------------------ (5)
    /** totalPlayedMs(0) sums played_ms of ALL rows (including non-completed). */
    void totalPlayedMsIncludesNonCompleted() {
        clearHistory();
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-04-01 10:00:00"), 10000, true));
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2025-04-02 10:00:00"), 20000, false));
        QVERIFY(insertHistoryAt(m_trackNoArtist,   QStringLiteral("2025-04-03 10:00:00"), 30000, true));

        auto r = m_ph.totalPlayedMs(0);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value(), qint64(60000));  // 10000 + 20000 + 30000
    }

    /** totalPlayedMs(0) returns 0 when the table is empty. */
    void totalPlayedMsReturnsZeroOnEmpty() {
        clearHistory();
        auto r = m_ph.totalPlayedMs(0);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value(), qint64(0));
    }

    // ------------------------------------------------------------------ (6)
    /** playsPerWeekday returns QList<int> of size 7 (Monday=0..Sunday=6),
     *  counts only completed=1, and correctly remaps SQLite Sunday(0)->6. */
    void playsPerWeekdayMappingAndSundayRemap() {
        clearHistory();
        // 2024-01-01 = Monday  → SQLite strftime('%w') = 1 → our index (1+6)%7 = 0
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2024-01-01 10:00:00"), 1000, true));
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2024-01-01 12:00:00"), 1000, true));
        // 2024-01-02 = Tuesday → SQLite %w = 2 → our index (2+6)%7 = 1
        QVERIFY(insertHistoryAt(m_trackNoArtist,   QStringLiteral("2024-01-02 10:00:00"), 1000, true));
        // 2024-01-07 = Sunday  → SQLite %w = 0 → our index (0+6)%7 = 6
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2024-01-07 10:00:00"), 1000, true));
        // non-completed on Wednesday: must NOT be counted
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2024-01-03 10:00:00"), 1000, false));

        // sinceDays=0 → no time filter → all historic rows are included
        auto r = m_ph.playsPerWeekday(0);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value().size(), 7);
        QCOMPARE(r.value()[0], 2);  // Monday
        QCOMPARE(r.value()[1], 1);  // Tuesday
        QCOMPARE(r.value()[2], 0);  // Wednesday (non-completed, not counted)
        QCOMPARE(r.value()[3], 0);  // Thursday
        QCOMPARE(r.value()[4], 0);  // Friday
        QCOMPARE(r.value()[5], 0);  // Saturday
        QCOMPARE(r.value()[6], 1);  // Sunday
    }

    // ------------------------------------------------------------------ (7)
    /** prune(days) deletes only rows older than the cutoff and returns the count. */
    void pruneDeletesOldRowsAndReturnsCount() {
        clearHistory();
        // Two rows well in the past
        QVERIFY(insertHistoryAt(m_trackWithArtist, QStringLiteral("2020-01-01 10:00:00"), 1000, true));
        QVERIFY(insertHistoryAt(m_trackNoArtist,   QStringLiteral("2020-06-15 10:00:00"), 1000, true));
        // One recent row via recordPlay (CURRENT_TIMESTAMP)
        auto rRec = m_ph.recordPlay(m_trackWithArtist, 5000, true);
        QVERIFY2(rRec.isOk(), qPrintable(rRec.isErr() ? rRec.error().message : QString()));

        auto r = m_ph.prune(30);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value(), 2);   // only the two 2020 rows

        // The recent row must still be present
        QSqlQuery q(DatabaseManager::instance().database());
        q.exec(QStringLiteral("SELECT COUNT(*) FROM play_history"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    /** prune(0) returns an InvalidArgument error. */
    void pruneInvalidArgReturnsError() {
        auto r = m_ph.prune(0);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }
};

QTEST_APPLESS_MAIN(TestPlayHistory)
#include "test_play_history.moc"
