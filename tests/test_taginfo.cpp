#include <QtTest>
#include <QTemporaryFile>
#include "soundshelf/io/TagInfo.hpp"

using namespace soundshelf;

class TestTagInfo : public QObject {
    Q_OBJECT

private slots:

    void readNonExistentFile() {
        auto r = TagInfo::fromFile(QStringLiteral("/this/file/does/not/exist.mp3"));
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::FileNotFound);
    }

    void readEmptyFile() {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.close();
        auto r = TagInfo::fromFile(f.fileName());
        // TagLib zwraca pusty FileRef dla nieformatowanego pliku
        QVERIFY(r.isErr());
    }

    void applyToTrack() {
        TagInfo info;
        info.title = QStringLiteral("Test Title");
        info.artist = QStringLiteral("Test Artist");
        info.album = QStringLiteral("Test Album");
        info.year = 2024;
        info.trackNumber = 5;
        info.durationMs = 180000;
        info.rgTrackGain = -6.2;

        Track t;
        info.applyToTrack(t);

        QCOMPARE(t.title, QStringLiteral("Test Title"));
        QCOMPARE(t.artist, QStringLiteral("Test Artist"));
        QCOMPARE(t.album, QStringLiteral("Test Album"));
        QCOMPARE(t.year, 2024);
        QCOMPARE(t.trackNumber, 5);
        QCOMPARE(t.durationMs, 180000);
        QVERIFY(t.rgTrackGain.has_value());
        QCOMPARE(*t.rgTrackGain, -6.2);
    }

    void albumArtistFallback() {
        TagInfo info;
        info.artist = QStringLiteral("Solo Artist");
        info.albumArtist.clear();

        Track t;
        info.applyToTrack(t);

        // Gdy albumArtist jest pusty, używamy artist jako fallback
        QCOMPARE(t.albumArtist, QStringLiteral("Solo Artist"));
    }
};

QTEST_MAIN(TestTagInfo)
#include "test_taginfo.moc"
