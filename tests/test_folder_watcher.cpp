#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "soundshelf/core/FolderWatcher.hpp"

using namespace soundshelf;

namespace {

// Write a zero-byte file at path so extension-based detection works.
void touch(const QString& path) {
    QFile f(path);
    f.open(QIODevice::WriteOnly);
}

} // namespace

class TestFolderWatcher : public QObject {
    Q_OBJECT

private slots:

    // Case 1: only audio files directly in dir are returned; non-audio and
    // files inside a subdirectory are excluded.
    void scanAudioFiles_findsOnlyAudio() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        touch(tmp.filePath(QStringLiteral("a.mp3")));
        touch(tmp.filePath(QStringLiteral("b.flac")));
        touch(tmp.filePath(QStringLiteral("notes.txt")));
        touch(tmp.filePath(QStringLiteral("cover.jpg")));

        // Subdirectory with an audio file — must NOT appear in results.
        QDir(tmp.path()).mkdir(QStringLiteral("sub"));
        touch(tmp.filePath(QStringLiteral("sub/c.mp3")));

        const QStringList result = FolderWatcher::scanAudioFiles(tmp.path());

        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0], tmp.filePath(QStringLiteral("a.mp3")));
        QCOMPARE(result[1], tmp.filePath(QStringLiteral("b.flac")));
    }

    // Case 2: extension matching is case-insensitive.
    void scanAudioFiles_caseInsensitiveExtension() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        touch(tmp.filePath(QStringLiteral("A.MP3")));
        touch(tmp.filePath(QStringLiteral("B.Flac")));

        const QStringList result = FolderWatcher::scanAudioFiles(tmp.path());

        QCOMPARE(result.size(), 2);
        // Both upper-case-extension files detected.
        QStringList names;
        for (const QString& p : result)
            names << QFileInfo(p).fileName();
        names.sort();
        QVERIFY(names.contains(QStringLiteral("A.MP3")));
        QVERIFY(names.contains(QStringLiteral("B.Flac")));
    }

    // Case 3: empty directory yields empty list; non-existing path also
    // yields empty list (no crash).
    void scanAudioFiles_emptyOrMissingDir() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Empty dir.
        QCOMPARE(FolderWatcher::scanAudioFiles(tmp.path()), QStringList());

        // Non-existing path — QDir::entryInfoList returns nothing safely.
        QCOMPARE(FolderWatcher::scanAudioFiles(
                     tmp.path() + QStringLiteral("/does_not_exist")),
                 QStringList());
    }

    // Case 4: added = in current but not known; removed = in known but not current.
    void diffFiles_addedAndRemoved() {
        const QSet<QString> known  = {QStringLiteral("/x/a"), QStringLiteral("/x/b")};
        const QSet<QString> current = {QStringLiteral("/x/b"), QStringLiteral("/x/c")};

        const FolderWatcher::DirDiff diff = FolderWatcher::diffFiles(known, current);

        QCOMPARE(diff.added,   QStringList({QStringLiteral("/x/c")}));
        QCOMPARE(diff.removed, QStringList({QStringLiteral("/x/a")}));
    }

    // Case 5: identical sets produce empty added and removed.
    void diffFiles_identicalSets() {
        const QSet<QString> s = {QStringLiteral("/a"), QStringLiteral("/b")};

        const FolderWatcher::DirDiff diff = FolderWatcher::diffFiles(s, s);

        QVERIFY(diff.added.isEmpty());
        QVERIFY(diff.removed.isEmpty());
    }

    // Case 6: both output lists are sorted regardless of QSet iteration order.
    void diffFiles_sortedDeterministic() {
        // Supply many paths so QSet ordering is unlikely to be sorted by accident.
        const QSet<QString> known = {
            QStringLiteral("/z"), QStringLiteral("/m"), QStringLiteral("/a"),
        };
        const QSet<QString> current = {
            QStringLiteral("/m"), QStringLiteral("/q"), QStringLiteral("/b"),
        };

        const FolderWatcher::DirDiff diff = FolderWatcher::diffFiles(known, current);

        // added = /b, /q  (current - known, sorted)
        QStringList expectedAdded = {QStringLiteral("/b"), QStringLiteral("/q")};
        QCOMPARE(diff.added, expectedAdded);

        // removed = /a, /z  (known - current, sorted)
        QStringList expectedRemoved = {QStringLiteral("/a"), QStringLiteral("/z")};
        QCOMPARE(diff.removed, expectedRemoved);
    }
};

QTEST_MAIN(TestFolderWatcher)
#include "test_folder_watcher.moc"
