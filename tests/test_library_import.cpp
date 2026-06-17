#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QProcess>
#include <QStandardPaths>
#include "soundshelf/core/LibraryManager.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

using namespace soundshelf;

// Regression test for the segfault on "add/import folder": LibraryManager runs
// the import on a QtConcurrent worker thread, which used the main-thread SQLite
// connection (QSqlDatabase is not thread-safe) → crash. DatabaseManager now
// hands each thread its own connection; this test exercises that path.
class TestLibraryImport : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    bool m_haveFfmpeg = false;

    bool makeAudio(const QString& path, const QString& title) {
        QProcess p;
        p.start(QStandardPaths::findExecutable("ffmpeg"), {
            "-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:duration=1",
            "-metadata", "title=" + title, "-metadata", "artist=TestArtist",
            "-y", path});
        return p.waitForFinished(20000) && p.exitCode() == 0;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_haveFfmpeg = !QStandardPaths::findExecutable("ffmpeg").isEmpty();
        if (!m_haveFfmpeg) QSKIP("ffmpeg not on PATH");

        QVERIFY(makeAudio(m_dir.filePath("a.flac"), "Track A"));
        QVERIFY(makeAudio(m_dir.filePath("b.flac"), "Track B"));
        QVERIFY(makeAudio(m_dir.filePath("c.mp3"),  "Track C"));

        auto r = DatabaseManager::instance().open(m_dir.filePath("lib.db"));
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));
    }

    // The crux: importing on the worker thread must not crash and must persist
    // the tracks via the per-thread DB connection.
    void importFolderOnWorkerThreadDoesNotCrash() {
        LibraryManager lib;
        QSignalSpy finished(&lib, &LibraryManager::importFinished);

        auto r = lib.importFolder(m_dir.path());
        QVERIFY2(r.isOk(), qPrintable(r.isErr() ? r.error().message : QString()));

        // Wait for the QtConcurrent worker to finish (event loop pumped by spy).
        QVERIFY2(finished.wait(30000), "import did not finish in time");
        QCOMPARE(finished.count(), 1);
        const int processed = finished.takeFirst().at(0).toInt();
        QCOMPARE(processed, 3);

        // Tracks were written from the worker thread's connection.
        auto tracks = DatabaseManager::instance().listTracks(100);
        QVERIFY(tracks.isOk());
        QCOMPARE(tracks.value().size(), 3);
    }

    void secondImportIsRejectedWhileBusyOrSucceedsAfter() {
        // A repeat import of the same folder must also be safe (re-uses the
        // worker thread's cached connection) and not crash.
        LibraryManager lib;
        QSignalSpy finished(&lib, &LibraryManager::importFinished);
        QVERIFY(lib.importFolder(m_dir.path()).isOk());
        QVERIFY(finished.wait(30000));
        QCOMPARE(finished.count(), 1);
    }

    void cleanupTestCase() {
        DatabaseManager::instance().close();
    }
};

QTEST_GUILESS_MAIN(TestLibraryImport)
#include "test_library_import.moc"
