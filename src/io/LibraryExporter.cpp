#include "soundshelf/io/LibraryExporter.hpp"

#include <QFile>
#include <QSaveFile>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcLibExp, "soundshelf.io.library")

namespace soundshelf {

namespace {

QJsonObject trackToJson(const Track& t)
{
    QJsonObject obj;

    // Portable string/numeric fields
    obj[QStringLiteral("filepath")]     = t.filepath;
    obj[QStringLiteral("filename")]     = t.filename;
    obj[QStringLiteral("title")]        = t.title;
    obj[QStringLiteral("artist")]       = t.artist;
    obj[QStringLiteral("albumArtist")]  = t.albumArtist;
    obj[QStringLiteral("album")]        = t.album;
    obj[QStringLiteral("genre")]        = t.genre;
    obj[QStringLiteral("trackNumber")]  = t.trackNumber;
    obj[QStringLiteral("discNumber")]   = t.discNumber;
    obj[QStringLiteral("year")]         = t.year;
    obj[QStringLiteral("durationMs")]   = t.durationMs;
    obj[QStringLiteral("bitrate")]      = t.bitrate;
    obj[QStringLiteral("samplerate")]   = t.samplerate;
    obj[QStringLiteral("channels")]     = t.channels;
    obj[QStringLiteral("format")]       = audioFormatToString(t.format);
    obj[QStringLiteral("codecProfile")] = t.codecProfile;
    obj[QStringLiteral("acoustid")]     = t.acoustid;
    obj[QStringLiteral("mbRecordingId")]= t.mbRecordingId;
    obj[QStringLiteral("md5Hash")]      = t.md5Hash;
    obj[QStringLiteral("playCount")]    = t.playCount;
    obj[QStringLiteral("skipCount")]    = t.skipCount;
    obj[QStringLiteral("rating")]       = t.rating;
    obj[QStringLiteral("comment")]      = t.comment;
    obj[QStringLiteral("missing")]      = t.missing;

    // QDateTime fields — omit when invalid
    if (t.addedAt.isValid())
        obj[QStringLiteral("addedAt")] = t.addedAt.toUTC().toString(Qt::ISODate);
    if (t.mtime.isValid())
        obj[QStringLiteral("mtime")] = t.mtime.toUTC().toString(Qt::ISODate);
    if (t.lastPlayed.isValid())
        obj[QStringLiteral("lastPlayed")] = t.lastPlayed.toUTC().toString(Qt::ISODate);

    // ReplayGain — omit when nullopt
    if (t.rgTrackGain.has_value())
        obj[QStringLiteral("rgTrackGain")] = *t.rgTrackGain;
    if (t.rgTrackPeak.has_value())
        obj[QStringLiteral("rgTrackPeak")] = *t.rgTrackPeak;
    if (t.rgAlbumGain.has_value())
        obj[QStringLiteral("rgAlbumGain")] = *t.rgAlbumGain;
    if (t.rgAlbumPeak.has_value())
        obj[QStringLiteral("rgAlbumPeak")] = *t.rgAlbumPeak;

    // Cue offsets — omit when nullopt
    if (t.cueOffsetMs.has_value())
        obj[QStringLiteral("cueOffsetMs")]  = *t.cueOffsetMs;
    if (t.cueDurationMs.has_value())
        obj[QStringLiteral("cueDurationMs")] = *t.cueDurationMs;

    return obj;
}

} // namespace

LibraryExporter::LibraryExporter(QObject* parent) : QObject(parent) {}
LibraryExporter::~LibraryExporter() = default;

QJsonDocument LibraryExporter::toJson(const QList<Track>& tracks)
{
    QJsonArray arr;
    for (const Track& t : tracks)
        arr.append(trackToJson(t));

    QJsonObject envelope;
    envelope[QStringLiteral("format")]       = QStringLiteral("soundshelf-library");
    envelope[QStringLiteral("version")]      = FORMAT_VERSION;
    envelope[QStringLiteral("exported_at")]  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    envelope[QStringLiteral("track_count")]  = static_cast<int>(tracks.size());
    envelope[QStringLiteral("tracks")]       = arr;

    qCDebug(lcLibExp) << "Built library JSON with" << tracks.size() << "tracks";
    return QJsonDocument(envelope);
}

Result<void> LibraryExporter::exportToFile(const QString& path, const QList<Track>& tracks)
{
    const QJsonDocument doc = toJson(tracks);

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return Result<void>::err(Error::FileAccessDenied,
            tr("Cannot open file for writing: %1").arg(out.errorString()));
    }
    out.write(doc.toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        return Result<void>::err(Error::FileAccessDenied,
            tr("Failed to save library file: %1").arg(out.errorString()));
    }

    qCDebug(lcLibExp) << "Exported" << tracks.size() << "tracks to" << path;
    return Result<void>::ok();
}

} // namespace soundshelf
