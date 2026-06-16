#include "soundshelf/io/LibraryImporter.hpp"
#include "soundshelf/io/LibraryExporter.hpp"

#include <QFile>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcLibImp, "soundshelf.io.library")

namespace soundshelf {

namespace {

Track trackFromJson(const QJsonObject& obj)
{
    Track t;
    // id / discId / artistId / albumArtistId / genreId stay at -1 (DB-local)

    t.filepath     = obj[QStringLiteral("filepath")].toString();
    t.filename     = obj[QStringLiteral("filename")].toString();
    t.title        = obj[QStringLiteral("title")].toString();
    t.artist       = obj[QStringLiteral("artist")].toString();
    t.albumArtist  = obj[QStringLiteral("albumArtist")].toString();
    t.album        = obj[QStringLiteral("album")].toString();
    t.genre        = obj[QStringLiteral("genre")].toString();
    t.trackNumber  = obj[QStringLiteral("trackNumber")].toInt();
    t.discNumber   = obj[QStringLiteral("discNumber")].toInt();
    t.year         = obj[QStringLiteral("year")].toInt();
    t.durationMs   = obj[QStringLiteral("durationMs")].toInt();
    t.bitrate      = obj[QStringLiteral("bitrate")].toInt();
    t.samplerate   = obj[QStringLiteral("samplerate")].toInt();
    t.channels     = obj[QStringLiteral("channels")].toInt();
    t.format       = audioFormatFromString(obj[QStringLiteral("format")].toString());
    t.codecProfile = obj[QStringLiteral("codecProfile")].toString();
    t.acoustid     = obj[QStringLiteral("acoustid")].toString();
    t.mbRecordingId= obj[QStringLiteral("mbRecordingId")].toString();
    t.md5Hash      = obj[QStringLiteral("md5Hash")].toString();
    t.playCount    = obj[QStringLiteral("playCount")].toInt();
    t.skipCount    = obj[QStringLiteral("skipCount")].toInt();
    t.rating       = obj[QStringLiteral("rating")].toDouble();
    t.comment      = obj[QStringLiteral("comment")].toString();
    t.missing      = obj[QStringLiteral("missing")].toBool();

    // QDateTime fields — only when key is present
    if (obj.contains(QStringLiteral("addedAt")))
        t.addedAt = QDateTime::fromString(obj[QStringLiteral("addedAt")].toString(), Qt::ISODate);
    if (obj.contains(QStringLiteral("mtime")))
        t.mtime = QDateTime::fromString(obj[QStringLiteral("mtime")].toString(), Qt::ISODate);
    if (obj.contains(QStringLiteral("lastPlayed")))
        t.lastPlayed = QDateTime::fromString(obj[QStringLiteral("lastPlayed")].toString(), Qt::ISODate);

    // ReplayGain — only when key is present
    if (obj.contains(QStringLiteral("rgTrackGain")))
        t.rgTrackGain = obj[QStringLiteral("rgTrackGain")].toDouble();
    if (obj.contains(QStringLiteral("rgTrackPeak")))
        t.rgTrackPeak = obj[QStringLiteral("rgTrackPeak")].toDouble();
    if (obj.contains(QStringLiteral("rgAlbumGain")))
        t.rgAlbumGain = obj[QStringLiteral("rgAlbumGain")].toDouble();
    if (obj.contains(QStringLiteral("rgAlbumPeak")))
        t.rgAlbumPeak = obj[QStringLiteral("rgAlbumPeak")].toDouble();

    // Cue offsets — only when key is present
    if (obj.contains(QStringLiteral("cueOffsetMs")))
        t.cueOffsetMs = obj[QStringLiteral("cueOffsetMs")].toInt();
    if (obj.contains(QStringLiteral("cueDurationMs")))
        t.cueDurationMs = obj[QStringLiteral("cueDurationMs")].toInt();

    return t;
}

} // namespace

LibraryImporter::LibraryImporter(QObject* parent) : QObject(parent) {}
LibraryImporter::~LibraryImporter() = default;

Result<QList<Track>> LibraryImporter::fromJson(const QJsonDocument& doc)
{
    if (!doc.isObject()) {
        return Result<QList<Track>>::err(Error::InvalidArgument,
            tr("Library document is not a JSON object"));
    }

    const QJsonObject root = doc.object();

    if (root[QStringLiteral("format")].toString() != QLatin1String("soundshelf-library")) {
        return Result<QList<Track>>::err(Error::InvalidArgument,
            tr("Not a soundshelf-library document (unexpected format string)"));
    }

    const int version = root[QStringLiteral("version")].toInt(0);
    if (version < 1 || version > LibraryExporter::FORMAT_VERSION) {
        return Result<QList<Track>>::err(Error::InvalidArgument,
            tr("Unsupported soundshelf-library version: %1").arg(version));
    }

    const QJsonArray arr = root[QStringLiteral("tracks")].toArray();
    QList<Track> tracks;
    tracks.reserve(arr.size());
    for (const QJsonValue& v : arr)
        tracks.append(trackFromJson(v.toObject()));

    qCDebug(lcLibImp) << "Imported" << tracks.size() << "tracks from JSON";
    return Result<QList<Track>>(tracks);
}

Result<QList<Track>> LibraryImporter::importFromFile(const QString& path)
{
    QFile f(path);
    if (!f.exists()) {
        return Result<QList<Track>>::err(Error::FileNotFound,
            tr("Library file not found: %1").arg(path));
    }
    if (!f.open(QIODevice::ReadOnly)) {
        return Result<QList<Track>>::err(Error::FileAccessDenied,
            tr("Cannot open library file: %1").arg(f.errorString()));
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        return Result<QList<Track>>::err(Error::InvalidArgument,
            tr("JSON parse error in library file: %1").arg(parseErr.errorString()));
    }

    return fromJson(doc);
}

} // namespace soundshelf
