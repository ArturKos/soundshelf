#include <QtTest>
#include <QTemporaryDir>
#include "soundshelf/io/PlaylistImporter.hpp"
#include "soundshelf/io/PlaylistExporter.hpp"

using namespace soundshelf;

namespace {

Track makeTrack(const QString& path, const QString& title,
                const QString& artist, const QString& album, int durationMs) {
    Track t;
    t.id = 1;
    t.filepath = path;
    t.title = title;
    t.artist = artist;
    t.album = album;
    t.durationMs = durationMs;
    t.trackNumber = 1;
    return t;
}

} // namespace

class TestPlaylistIo : public QObject {
    Q_OBJECT

private slots:

    void detectFormatByExtension() {
        QCOMPARE(PlaylistImporter::detectFormat(QStringLiteral("a.m3u"),  QByteArray()),
                 PlaylistImporter::Format::M3U);
        QCOMPARE(PlaylistImporter::detectFormat(QStringLiteral("a.M3U8"), QByteArray()),
                 PlaylistImporter::Format::M3U);
        QCOMPARE(PlaylistImporter::detectFormat(QStringLiteral("a.pls"),  QByteArray()),
                 PlaylistImporter::Format::PLS);
        QCOMPARE(PlaylistImporter::detectFormat(QStringLiteral("a.xspf"), QByteArray()),
                 PlaylistImporter::Format::XSPF);
    }

    void detectFormatByContentSniff() {
        QCOMPARE(PlaylistImporter::detectFormat(QStringLiteral("noext"),
                                                QByteArrayLiteral("<?xml version=\"1.0\"?>\n<playlist/>")),
                 PlaylistImporter::Format::XSPF);
        QCOMPARE(PlaylistImporter::detectFormat(QStringLiteral("noext"),
                                                QByteArrayLiteral("[playlist]\nNumberOfEntries=0")),
                 PlaylistImporter::Format::PLS);
        QCOMPARE(PlaylistImporter::detectFormat(QStringLiteral("noext"),
                                                QByteArrayLiteral("#EXTM3U\nfoo.mp3")),
                 PlaylistImporter::Format::M3U);
    }

    void formatFromExtensionDefaultsToM3U() {
        QCOMPARE(PlaylistExporter::formatFromExtension(QStringLiteral("x.unknown")),
                 PlaylistExporter::Format::M3U);
        QCOMPARE(PlaylistExporter::formatFromExtension(QStringLiteral("x.xspf")),
                 PlaylistExporter::Format::XSPF);
    }

    void m3uExportContainsExtinfAndPath() {
        const QList<Track> tracks {
            makeTrack(QStringLiteral("/music/song1.mp3"),
                      QStringLiteral("Song One"),
                      QStringLiteral("Band"),
                      QStringLiteral("Album"), 240000),
        };
        const QString out = PlaylistExporter::toM3U(tracks, QStringLiteral("/music"),
                                                    /*relative=*/true);
        QVERIFY(out.startsWith(QStringLiteral("#EXTM3U")));
        QVERIFY(out.contains(QStringLiteral("#EXTINF:240,Band - Song One")));
        QVERIFY(out.contains(QStringLiteral("song1.mp3")));
    }

    void plsExportNumbersEntries() {
        const QList<Track> tracks {
            makeTrack(QStringLiteral("/music/a.mp3"), QStringLiteral("A"),
                      QStringLiteral(""), QStringLiteral(""), 60000),
            makeTrack(QStringLiteral("/music/b.mp3"), QStringLiteral("B"),
                      QStringLiteral(""), QStringLiteral(""), 90000),
        };
        const QString out = PlaylistExporter::toPLS(tracks, QStringLiteral("/music"), true);
        QVERIFY(out.contains(QStringLiteral("[playlist]")));
        QVERIFY(out.contains(QStringLiteral("NumberOfEntries=2")));
        QVERIFY(out.contains(QStringLiteral("File1=a.mp3")));
        QVERIFY(out.contains(QStringLiteral("File2=b.mp3")));
        QVERIFY(out.contains(QStringLiteral("Length1=60")));
        QVERIFY(out.contains(QStringLiteral("Length2=90")));
    }

    void xspfExportProducesValidXml() {
        const QList<Track> tracks {
            makeTrack(QStringLiteral("/music/a.mp3"), QStringLiteral("A"),
                      QStringLiteral("Artist"), QStringLiteral("Album"), 60000),
        };
        const QString out = PlaylistExporter::toXSPF(tracks, QStringLiteral("/music"), true);
        QVERIFY(out.contains(QStringLiteral("<?xml")));
        QVERIFY(out.contains(QStringLiteral("xmlns=\"http://xspf.org/ns/0/\"")));
        QVERIFY(out.contains(QStringLiteral("<title>A</title>")));
        QVERIFY(out.contains(QStringLiteral("<creator>Artist</creator>")));
        QVERIFY(out.contains(QStringLiteral("<duration>60000</duration>")));
    }

    void m3uRoundTrip() {
        // Write M3U on disk and re-import — paths should round-trip,
        // titles should make it through #EXTINF.
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Ensure the audio file exists so the imported path is valid.
        const QString audio = tmp.filePath(QStringLiteral("song.mp3"));
        QFile fa(audio); fa.open(QIODevice::WriteOnly); fa.close();

        const QList<Track> tracks {
            makeTrack(audio, QStringLiteral("Roundtrip"),
                      QStringLiteral("Tester"), QString(), 120000),
        };
        const QString playlistPath = tmp.filePath(QStringLiteral("p.m3u"));
        PlaylistExporter exporter;
        QVERIFY(exporter.exportToFile(playlistPath, tracks, /*relative=*/true).isOk());

        PlaylistImporter importer;
        auto r = importer.importFile(playlistPath);
        QVERIFY(r.isOk());
        const auto& entries = r.value();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(QFileInfo(entries[0].path).canonicalFilePath(),
                 QFileInfo(audio).canonicalFilePath());
        QVERIFY(entries[0].titleHint.contains(QStringLiteral("Roundtrip")));
        QCOMPARE(entries[0].durationSec, 120);
    }
};

QTEST_APPLESS_MAIN(TestPlaylistIo)
#include "test_playlist_io.moc"
