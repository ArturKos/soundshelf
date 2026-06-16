#include <QtTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "soundshelf/io/LibraryExporter.hpp"
#include "soundshelf/io/LibraryImporter.hpp"

using namespace soundshelf;

namespace {

/// Builds a fully-populated track (all optional fields set).
Track makeFullTrack()
{
    Track t;
    t.id            = 99;    // DB-local — must NOT survive export/import
    t.discId        = 10;    // DB-local
    t.artistId      = 20;    // DB-local
    t.albumArtistId = 30;    // DB-local
    t.genreId       = 40;    // DB-local
    t.filepath      = QStringLiteral("/music/album/01 - Oxygène.flac");
    t.filename      = QStringLiteral("01 - Oxygène.flac");
    t.title         = QStringLiteral("Oxygène, Pt. 1");
    t.artist        = QStringLiteral("Jean-Michel Jarre");
    t.albumArtist   = QStringLiteral("Jean-Michel Jarre");
    t.album         = QStringLiteral("Oxygène");
    t.genre         = QStringLiteral("Electronic");
    t.trackNumber   = 1;
    t.discNumber    = 1;
    t.year          = 1976;
    t.durationMs    = 370000;
    t.bitrate       = 1050;
    t.samplerate    = 44100;
    t.channels      = 2;
    t.format        = AudioFormat::FLAC;
    t.codecProfile  = QStringLiteral("level 8");
    t.acoustid      = QStringLiteral("acoustid-abc123");
    t.mbRecordingId = QStringLiteral("mb-recording-def456");
    t.md5Hash       = QStringLiteral("abc1234567890abcdef");
    t.playCount     = 12;
    t.skipCount     = 1;
    t.rating        = 4.5;
    t.comment       = QStringLiteral("Ripped from vinyl");
    t.missing       = false;
    t.addedAt       = QDateTime::fromSecsSinceEpoch(1700000000, Qt::UTC);
    t.mtime         = QDateTime::fromSecsSinceEpoch(1700001000, Qt::UTC);
    t.lastPlayed    = QDateTime::fromSecsSinceEpoch(1700002000, Qt::UTC);
    t.rgTrackGain   = -6.5;
    t.rgTrackPeak   = 0.95;
    t.rgAlbumGain   = -7.0;
    t.rgAlbumPeak   = 0.98;
    t.cueOffsetMs   = 5000;
    t.cueDurationMs = 300000;
    return t;
}

/// Builds a minimal track (no optional fields set).
Track makeMinimalTrack()
{
    Track t;
    t.filepath = QStringLiteral("/music/single.mp3");
    t.filename = QStringLiteral("single.mp3");
    t.title    = QStringLiteral("Simple Song");
    t.artist   = QStringLiteral("Tester");
    t.format   = AudioFormat::MP3;
    t.durationMs = 180000;
    // rgTrackGain / rgTrackPeak / rgAlbumGain / rgAlbumPeak: nullopt (not set)
    // cueOffsetMs / cueDurationMs: nullopt (not set)
    // addedAt / mtime / lastPlayed: invalid QDateTime (not set)
    return t;
}

} // namespace

class TestLibraryIo : public QObject {
    Q_OBJECT

private slots:

    // --- envelope correctness ---

    void envelopeContainsRequiredKeys()
    {
        const QList<Track> tracks = { makeMinimalTrack() };
        const QJsonDocument doc = LibraryExporter::toJson(tracks);

        QVERIFY(doc.isObject());
        const QJsonObject root = doc.object();
        QCOMPARE(root[QStringLiteral("format")].toString(),
                 QStringLiteral("soundshelf-library"));
        QCOMPARE(root[QStringLiteral("version")].toInt(),
                 LibraryExporter::FORMAT_VERSION);
        QCOMPARE(root[QStringLiteral("track_count")].toInt(), 1);
        QVERIFY(root.contains(QStringLiteral("exported_at")));
        // exported_at must parse as a valid ISO 8601 date
        const QDateTime ts = QDateTime::fromString(
            root[QStringLiteral("exported_at")].toString(), Qt::ISODate);
        QVERIFY(ts.isValid());
        QVERIFY(root[QStringLiteral("tracks")].isArray());
        QCOMPARE(root[QStringLiteral("tracks")].toArray().size(), 1);
    }

    void trackCountMatchesInput()
    {
        const QList<Track> tracks = { makeFullTrack(), makeMinimalTrack() };
        const QJsonDocument doc = LibraryExporter::toJson(tracks);
        QCOMPARE(doc.object()[QStringLiteral("track_count")].toInt(), 2);
        QCOMPARE(doc.object()[QStringLiteral("tracks")].toArray().size(), 2);
    }

    // --- field-level round-trip (toJson → fromJson) ---

    void roundTripFullTrack()
    {
        const Track src = makeFullTrack();
        const QList<Track> before = { src };

        auto r = LibraryImporter::fromJson(LibraryExporter::toJson(before));
        QVERIFY(r.isOk());
        QCOMPARE(r.value().size(), 1);

        const Track& a = r.value()[0];

        // DB-local identity fields must NOT be carried over
        QCOMPARE(a.id,            -1);
        QCOMPARE(a.discId,        -1);
        QCOMPARE(a.artistId,      -1);
        QCOMPARE(a.albumArtistId, -1);
        QCOMPARE(a.genreId,       -1);

        // Portable fields
        QCOMPARE(a.filepath,     src.filepath);
        QCOMPARE(a.filename,     src.filename);
        QCOMPARE(a.title,        src.title);
        QCOMPARE(a.artist,       src.artist);
        QCOMPARE(a.albumArtist,  src.albumArtist);
        QCOMPARE(a.album,        src.album);
        QCOMPARE(a.genre,        src.genre);
        QCOMPARE(a.trackNumber,  src.trackNumber);
        QCOMPARE(a.discNumber,   src.discNumber);
        QCOMPARE(a.year,         src.year);
        QCOMPARE(a.durationMs,   src.durationMs);
        QCOMPARE(a.bitrate,      src.bitrate);
        QCOMPARE(a.samplerate,   src.samplerate);
        QCOMPARE(a.channels,     src.channels);
        QCOMPARE(a.format,       src.format);
        QCOMPARE(a.codecProfile, src.codecProfile);
        QCOMPARE(a.acoustid,     src.acoustid);
        QCOMPARE(a.mbRecordingId,src.mbRecordingId);
        QCOMPARE(a.md5Hash,      src.md5Hash);
        QCOMPARE(a.playCount,    src.playCount);
        QCOMPARE(a.skipCount,    src.skipCount);
        QCOMPARE(a.rating,       src.rating);
        QCOMPARE(a.comment,      src.comment);
        QCOMPARE(a.missing,      src.missing);

        // QDateTime — compare as UTC (ISO 8601 round-trip uses UTC)
        QCOMPARE(a.addedAt.toUTC(),    src.addedAt.toUTC());
        QCOMPARE(a.mtime.toUTC(),      src.mtime.toUTC());
        QCOMPARE(a.lastPlayed.toUTC(), src.lastPlayed.toUTC());

        // ReplayGain
        QVERIFY(a.rgTrackGain.has_value());
        QCOMPARE(*a.rgTrackGain, *src.rgTrackGain);
        QVERIFY(a.rgTrackPeak.has_value());
        QCOMPARE(*a.rgTrackPeak, *src.rgTrackPeak);
        QVERIFY(a.rgAlbumGain.has_value());
        QCOMPARE(*a.rgAlbumGain, *src.rgAlbumGain);
        QVERIFY(a.rgAlbumPeak.has_value());
        QCOMPARE(*a.rgAlbumPeak, *src.rgAlbumPeak);

        // Cue offsets
        QVERIFY(a.cueOffsetMs.has_value());
        QCOMPARE(*a.cueOffsetMs, *src.cueOffsetMs);
        QVERIFY(a.cueDurationMs.has_value());
        QCOMPARE(*a.cueDurationMs, *src.cueDurationMs);
    }

    void roundTripMinimalTrackHasNoOptionals()
    {
        const Track src = makeMinimalTrack();
        auto r = LibraryImporter::fromJson(LibraryExporter::toJson({ src }));
        QVERIFY(r.isOk());
        const Track& a = r.value()[0];

        QCOMPARE(a.filepath, src.filepath);
        QCOMPARE(a.format,   src.format);

        // Optional ReplayGain fields must be nullopt
        QVERIFY(!a.rgTrackGain.has_value());
        QVERIFY(!a.rgTrackPeak.has_value());
        QVERIFY(!a.rgAlbumGain.has_value());
        QVERIFY(!a.rgAlbumPeak.has_value());

        // Optional cue fields must be nullopt
        QVERIFY(!a.cueOffsetMs.has_value());
        QVERIFY(!a.cueDurationMs.has_value());

        // DateTime fields must be invalid (keys were absent)
        QVERIFY(!a.addedAt.isValid());
        QVERIFY(!a.mtime.isValid());
        QVERIFY(!a.lastPlayed.isValid());
    }

    void dbLocalFieldsNotInJson()
    {
        const Track src = makeFullTrack();
        const QJsonDocument doc = LibraryExporter::toJson({ src });
        const QJsonObject trackObj = doc.object()[QStringLiteral("tracks")]
                                        .toArray()[0].toObject();

        // These keys must NOT be present in the exported object
        QVERIFY(!trackObj.contains(QStringLiteral("id")));
        QVERIFY(!trackObj.contains(QStringLiteral("discId")));
        QVERIFY(!trackObj.contains(QStringLiteral("artistId")));
        QVERIFY(!trackObj.contains(QStringLiteral("albumArtistId")));
        QVERIFY(!trackObj.contains(QStringLiteral("genreId")));
        QVERIFY(!trackObj.contains(QStringLiteral("coverHash")));
        QVERIFY(!trackObj.contains(QStringLiteral("coverData")));
    }

    // --- file round-trip ---

    void fileRoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QList<Track> before = { makeFullTrack(), makeMinimalTrack() };
        const QString path = tmp.filePath(QStringLiteral("library.json"));

        LibraryExporter exporter;
        const auto exportResult = exporter.exportToFile(path, before);
        QVERIFY2(exportResult.isOk(), qPrintable(exportResult.isErr()
            ? exportResult.error().message : QString()));

        QVERIFY(QFile::exists(path));

        LibraryImporter importer;
        const auto importResult = importer.importFromFile(path);
        QVERIFY2(importResult.isOk(), qPrintable(importResult.isErr()
            ? importResult.error().message : QString()));

        const QList<Track>& after = importResult.value();
        QCOMPARE(after.size(), before.size());

        // Spot-check key fields from both tracks
        QCOMPARE(after[0].filepath, before[0].filepath);
        QCOMPARE(after[0].title,    before[0].title);
        QCOMPARE(after[0].format,   before[0].format);
        QCOMPARE(*after[0].rgTrackGain, *before[0].rgTrackGain);

        QCOMPARE(after[1].filepath, before[1].filepath);
        QCOMPARE(after[1].title,    before[1].title);
        QVERIFY(!after[1].rgTrackGain.has_value());
    }

    void exportedFileIsValidJson()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("lib.json"));

        LibraryExporter exporter;
        QVERIFY(exporter.exportToFile(path, { makeMinimalTrack() }).isOk());

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        QCOMPARE(err.error, QJsonParseError::NoError);
        QVERIFY(doc.isObject());
    }

    // --- error paths ---

    void fromJsonErrorOnNullDocument()
    {
        const auto r = LibraryImporter::fromJson(QJsonDocument{});
        QVERIFY(r.isErr());
    }

    void fromJsonErrorOnArrayDocument()
    {
        const auto r = LibraryImporter::fromJson(QJsonDocument(QJsonArray{}));
        QVERIFY(r.isErr());
    }

    void fromJsonErrorOnWrongFormatString()
    {
        QJsonObject root;
        root[QStringLiteral("format")]  = QStringLiteral("wrong-format");
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("tracks")]  = QJsonArray{};
        const auto r = LibraryImporter::fromJson(QJsonDocument(root));
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }

    void fromJsonErrorOnUnsupportedVersion()
    {
        QJsonObject root;
        root[QStringLiteral("format")]  = QStringLiteral("soundshelf-library");
        root[QStringLiteral("version")] = 999;
        root[QStringLiteral("tracks")]  = QJsonArray{};
        const auto r = LibraryImporter::fromJson(QJsonDocument(root));
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }

    void fromJsonErrorOnVersionZero()
    {
        QJsonObject root;
        root[QStringLiteral("format")]  = QStringLiteral("soundshelf-library");
        root[QStringLiteral("version")] = 0;
        root[QStringLiteral("tracks")]  = QJsonArray{};
        const auto r = LibraryImporter::fromJson(QJsonDocument(root));
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }

    void importFromFileErrorFileNotFound()
    {
        LibraryImporter importer;
        const auto r = importer.importFromFile(
            QStringLiteral("/nonexistent/path/to/library.json"));
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::FileNotFound);
    }

    void importFromFileErrorMalformedJson()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("bad.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArrayLiteral("{ not : valid ; json }"));
        f.close();

        LibraryImporter importer;
        const auto r = importer.importFromFile(path);
        QVERIFY(r.isErr());
        QCOMPARE(r.error().code, Error::InvalidArgument);
    }
};

QTEST_APPLESS_MAIN(TestLibraryIo)
#include "test_library_io.moc"
