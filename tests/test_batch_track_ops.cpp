#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/core/BatchTrackOps.hpp"
#include "soundshelf/core/Track.hpp"

using namespace soundshelf;

/// Write a small dummy file at @p path and return true on success.
static bool writeDummyFile(const QString& path, const QByteArray& content = "DUMMYAUDIO") {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(content);
    return true;
}

class TestBatchTrackOps : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:

    void initTestCase() {
        QVERIFY(m_dir.isValid());
        const QString dbPath = m_dir.filePath(QStringLiteral("batch_test.db"));
        auto r = DatabaseManager::instance().open(dbPath);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
    }

    // (1) copyToFolder copies files to dest, skips collisions, returns correct count.
    void test_copyToFolder() {
        const QString srcDir = m_dir.filePath(QStringLiteral("copy_src"));
        const QString dstDir = m_dir.filePath(QStringLiteral("copy_dst"));
        QDir().mkpath(srcDir);

        const QByteArray content1 = "AUDIO_BYTES_TRACK_1";
        const QByteArray content2 = "AUDIO_BYTES_TRACK_2";

        const QString p1 = srcDir + QLatin1String("/track1.mp3");
        const QString p2 = srcDir + QLatin1String("/track2.mp3");
        QVERIFY(writeDummyFile(p1, content1));
        QVERIFY(writeDummyFile(p2, content2));

        Track t1; t1.filepath = p1; t1.filename = QStringLiteral("track1.mp3");
        Track t2; t2.filepath = p2; t2.filename = QStringLiteral("track2.mp3");

        auto r = BatchTrackOps::copyToFolder({t1, t2}, dstDir);
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value(), 2);

        const QString dst1 = dstDir + QLatin1String("/track1.mp3");
        const QString dst2 = dstDir + QLatin1String("/track2.mp3");
        QVERIFY(QFile::exists(dst1));
        QVERIFY(QFile::exists(dst2));

        // Verify identical bytes
        QFile f1(dst1);
        QVERIFY(f1.open(QIODevice::ReadOnly));
        QCOMPARE(f1.readAll(), content1);
        QFile f2(dst2);
        QVERIFY(f2.open(QIODevice::ReadOnly));
        QCOMPARE(f2.readAll(), content2);

        // Second call — both files exist at dest → 0 copied (skip-on-collision)
        auto r2 = BatchTrackOps::copyToFolder({t1, t2}, dstDir);
        QVERIFY(r2.isOk());
        QCOMPARE(r2.value(), 0);
    }

    // (2) removeFromLibrary deletes DB rows but leaves files on disk.
    void test_removeFromLibrary() {
        const QString path = m_dir.filePath(QStringLiteral("rl_track.flac"));
        QVERIFY(writeDummyFile(path));

        Track t;
        t.filepath = path;
        t.filename = QStringLiteral("rl_track.flac");
        t.title    = QStringLiteral("RemoveLibTest");
        t.format   = AudioFormat::FLAC;
        auto ur = DatabaseManager::instance().upsertTrack(t);
        QVERIFY2(ur.isOk(), qPrintable(ur.isErr() ? ur.error().message : QString()));
        const int trackId = t.id;
        QVERIFY(trackId > 0);

        auto r = BatchTrackOps::removeFromLibrary(DatabaseManager::instance(), {trackId});
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value(), 1);

        // DB row gone
        QVERIFY(DatabaseManager::instance().getTrack(trackId).isErr());

        // File still on disk
        QVERIFY(QFile::exists(path));
    }

    // (3) deleteFiles removes file from disk AND its DB row.
    void test_deleteFiles() {
        const QString path = m_dir.filePath(QStringLiteral("df_track.ogg"));
        QVERIFY(writeDummyFile(path));

        Track t;
        t.filepath = path;
        t.filename = QStringLiteral("df_track.ogg");
        t.title    = QStringLiteral("DeleteFilesTest");
        t.format   = AudioFormat::OGG;
        auto ur = DatabaseManager::instance().upsertTrack(t);
        QVERIFY2(ur.isOk(), qPrintable(ur.isErr() ? ur.error().message : QString()));
        const int trackId = t.id;
        QVERIFY(trackId > 0);

        auto r = BatchTrackOps::deleteFiles(DatabaseManager::instance(), {trackId});
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
        QCOMPARE(r.value(), 1);

        // File gone from disk
        QVERIFY(!QFile::exists(path));

        // DB row gone
        QVERIFY(DatabaseManager::instance().getTrack(trackId).isErr());
    }

    // (4) removeTrack keeps FTS in sync — searchTracks no longer finds the title.
    void test_removeTrack_fts_sync() {
        const QString path = m_dir.filePath(QStringLiteral("fts_sync_track.mp3"));
        QVERIFY(writeDummyFile(path));

        const QString uniqueTitle = QStringLiteral("BatchTrackOpsFtsSyncXYZ99");

        Track t;
        t.filepath = path;
        t.filename = QStringLiteral("fts_sync_track.mp3");
        t.title    = uniqueTitle;
        t.format   = AudioFormat::MP3;
        auto ur = DatabaseManager::instance().upsertTrack(t);
        QVERIFY2(ur.isOk(), qPrintable(ur.isErr() ? ur.error().message : QString()));
        const int trackId = t.id;
        QVERIFY(trackId > 0);

        // FTS should find it before removal
        auto sr1 = DatabaseManager::instance().searchTracks(uniqueTitle);
        QVERIFY(sr1.isOk());
        QVERIFY(!sr1.value().isEmpty());

        // Remove via removeTrack (tracks_ad trigger cleans FTS)
        auto dr = DatabaseManager::instance().removeTrack(trackId);
        QVERIFY2(dr.isOk(), qPrintable(dr.isErr() ? dr.error().message : QString()));

        // FTS should no longer return the removed track
        auto sr2 = DatabaseManager::instance().searchTracks(uniqueTitle);
        QVERIFY(sr2.isOk());
        QVERIFY(sr2.value().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestBatchTrackOps)
#include "test_batch_track_ops.moc"
