#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/core/Track.hpp"

using namespace soundshelf;

class TestLibrarySources : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString dbPath = m_dir.filePath(QStringLiteral("sources_test.db"));
        auto r = DatabaseManager::instance().open(dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
    }

    // (1) ensureSource inserts once and is idempotent on duplicate path
    void test_ensureSource_idempotent() {
        auto r1 = DatabaseManager::instance().ensureSource(
            QStringLiteral("/music/jazz"), QStringLiteral("Jazz"));
        QVERIFY2(r1.isOk(), qPrintable(r1.isErr() ? r1.error().message : QString()));
        const int id1 = r1.value();
        QVERIFY(id1 > 0);

        // Second call with same path — same id returned, label NOT overwritten
        auto r2 = DatabaseManager::instance().ensureSource(
            QStringLiteral("/music/jazz"), QStringLiteral("Jazz Renamed"));
        QVERIFY2(r2.isOk(), qPrintable(r2.isErr() ? r2.error().message : QString()));
        QCOMPARE(r2.value(), id1);

        // Confirm label is still original "Jazz"
        auto ls = DatabaseManager::instance().listSources();
        QVERIFY(ls.isOk());
        const auto& sources = ls.value();
        auto it = std::find_if(sources.cbegin(), sources.cend(),
            [id1](const DatabaseManager::SourceInfo& s) { return s.id == id1; });
        QVERIFY(it != sources.cend());
        QCOMPARE(it->label, QStringLiteral("Jazz"));
    }

    // (2) listSources returns inserted rows with correct label/path
    void test_listSources_returns_rows() {
        // Insert a unique source for this test
        auto ri = DatabaseManager::instance().ensureSource(
            QStringLiteral("/music/rock"), QStringLiteral("Rock"));
        QVERIFY(ri.isOk());

        auto ls = DatabaseManager::instance().listSources();
        QVERIFY2(ls.isOk(), qPrintable(ls.isErr() ? ls.error().message : QString()));
        const auto& sources = ls.value();
        QVERIFY(!sources.isEmpty());

        // At least the Jazz source from the previous test and Rock are present
        bool foundRock = false;
        for (const auto& s : sources) {
            if (s.path == QStringLiteral("/music/rock")) {
                QCOMPARE(s.label, QStringLiteral("Rock"));
                foundRock = true;
                break;
            }
        }
        QVERIFY(foundRock);
    }

    // (3) renameSource updates label and persists
    void test_renameSource_persists() {
        auto ri = DatabaseManager::instance().ensureSource(
            QStringLiteral("/music/classical"), QStringLiteral("Classical"));
        QVERIFY(ri.isOk());
        const int id = ri.value();

        auto rr = DatabaseManager::instance().renameSource(id, QStringLiteral("Classical Music"));
        QVERIFY2(rr.isOk(), qPrintable(rr.isErr() ? rr.error().message : QString()));

        auto ls = DatabaseManager::instance().listSources();
        QVERIFY(ls.isOk());
        const auto& sources = ls.value();
        auto it = std::find_if(sources.cbegin(), sources.cend(),
            [id](const DatabaseManager::SourceInfo& s) { return s.id == id; });
        QVERIFY(it != sources.cend());
        QCOMPARE(it->label, QStringLiteral("Classical Music"));
    }

    // (4) upsertTrack with sourceId set → tracksBySource returns that track;
    //     a track with a different source is excluded
    void test_tracksBySource_filters() {
        // Create two sources
        auto rs1 = DatabaseManager::instance().ensureSource(
            QStringLiteral("/music/soul"), QStringLiteral("Soul"));
        QVERIFY(rs1.isOk());
        const int sourceA = rs1.value();

        auto rs2 = DatabaseManager::instance().ensureSource(
            QStringLiteral("/music/funk"), QStringLiteral("Funk"));
        QVERIFY(rs2.isOk());
        const int sourceB = rs2.value();

        // Insert a track tagged with sourceA
        Track tA;
        tA.filepath = QStringLiteral("/music/soul/track_a.flac");
        tA.filename = QStringLiteral("track_a.flac");
        tA.title    = QStringLiteral("Soul Track");
        tA.format   = AudioFormat::FLAC;
        tA.sourceId = sourceA;
        auto ua = DatabaseManager::instance().upsertTrack(tA);
        QVERIFY2(ua.isOk(), qPrintable(ua.isErr() ? ua.error().message : QString()));

        // Insert a track tagged with sourceB
        Track tB;
        tB.filepath = QStringLiteral("/music/funk/track_b.flac");
        tB.filename = QStringLiteral("track_b.flac");
        tB.title    = QStringLiteral("Funk Track");
        tB.format   = AudioFormat::FLAC;
        tB.sourceId = sourceB;
        auto ub = DatabaseManager::instance().upsertTrack(tB);
        QVERIFY(ub.isOk());

        // tracksBySource(sourceA) returns tA only
        auto ra = DatabaseManager::instance().tracksBySource(sourceA);
        QVERIFY2(ra.isOk(), qPrintable(ra.isErr() ? ra.error().message : QString()));
        const auto& tracksA = ra.value();
        QCOMPARE(tracksA.size(), 1);
        QCOMPARE(tracksA[0].title, QStringLiteral("Soul Track"));

        // tracksBySource(sourceB) returns tB only
        auto rb = DatabaseManager::instance().tracksBySource(sourceB);
        QVERIFY(rb.isOk());
        QCOMPARE(rb.value().size(), 1);
        QCOMPARE(rb.value()[0].title, QStringLiteral("Funk Track"));
    }

    // (5) removeSource deletes the source row; tracksBySource returns empty;
    //     the track row itself still exists with source_id = NULL
    void test_removeSource_keeps_track() {
        auto rs = DatabaseManager::instance().ensureSource(
            QStringLiteral("/music/ambient"), QStringLiteral("Ambient"));
        QVERIFY(rs.isOk());
        const int sourceId = rs.value();

        Track t;
        t.filepath = QStringLiteral("/music/ambient/track_c.flac");
        t.filename = QStringLiteral("track_c.flac");
        t.title    = QStringLiteral("Ambient Track");
        t.format   = AudioFormat::FLAC;
        t.sourceId = sourceId;
        auto ut = DatabaseManager::instance().upsertTrack(t);
        QVERIFY(ut.isOk());
        const int trackId = t.id;
        QVERIFY(trackId > 0);

        // Remove the source
        auto rr = DatabaseManager::instance().removeSource(sourceId);
        QVERIFY2(rr.isOk(), qPrintable(rr.isErr() ? rr.error().message : QString()));

        // tracksBySource returns empty
        auto rb = DatabaseManager::instance().tracksBySource(sourceId);
        QVERIFY(rb.isOk());
        QVERIFY(rb.value().isEmpty());

        // The track row still exists
        auto gt = DatabaseManager::instance().getTrack(trackId);
        QVERIFY2(gt.isOk(), qPrintable(gt.isErr() ? gt.error().message : QString()));
        QCOMPARE(gt.value().title, QStringLiteral("Ambient Track"));

        // source_id should now be NULL in the DB
        QSqlQuery q(DatabaseManager::instance().database());
        q.prepare(QStringLiteral("SELECT source_id FROM tracks WHERE id = ?"));
        q.addBindValue(trackId);
        QVERIFY(q.exec() && q.next());
        QVERIFY(q.value(0).isNull());

        // library_sources row is gone
        QSqlQuery qs(DatabaseManager::instance().database());
        qs.prepare(QStringLiteral("SELECT COUNT(*) FROM library_sources WHERE id = ?"));
        qs.addBindValue(sourceId);
        QVERIFY(qs.exec() && qs.next());
        QCOMPARE(qs.value(0).toInt(), 0);
    }
};

QTEST_GUILESS_MAIN(TestLibrarySources)
#include "test_library_sources.moc"
