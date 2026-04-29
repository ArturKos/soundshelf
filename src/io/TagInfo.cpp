#include "soundshelf/io/TagInfo.hpp"

#include <QFileInfo>
#include <QLoggingCategory>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/vorbisfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/opusfile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>

Q_LOGGING_CATEGORY(lcTag, "soundshelf.tag")

namespace soundshelf {

namespace {

QString tlString(const TagLib::String& s) {
    if (s.isEmpty()) return {};
    return QString::fromStdString(s.to8Bit(true));
}

TagLib::String fromQ(const QString& s) {
    return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
}

/// Parsuje pole "1/12" → (1, 12) lub "1" → (1, 0)
std::pair<int, int> parseSlashNumber(const QString& s) {
    if (s.isEmpty()) return {0, 0};
    const auto parts = s.split('/');
    bool ok1 = false, ok2 = false;
    int n1 = parts.value(0).toInt(&ok1);
    int n2 = parts.value(1).toInt(&ok2);
    return {ok1 ? n1 : 0, ok2 ? n2 : 0};
}

/// Wydobywa ReplayGain z PropertyMap (Vorbis Comment, ID3v2 TXXX, etc.)
void extractReplayGain(const TagLib::PropertyMap& map, TagInfo& info) {
    auto getDb = [&map](const char* key) -> std::optional<double> {
        const auto it = map.find(key);
        if (it == map.end() || it->second.isEmpty()) return std::nullopt;
        const QString val = tlString(it->second.front());
        // Format: "-6.20 dB" lub "-6.20"
        QString cleaned = val;
        cleaned.remove(QLatin1String(" dB"), Qt::CaseInsensitive);
        bool ok = false;
        const double d = cleaned.toDouble(&ok);
        return ok ? std::optional<double>(d) : std::nullopt;
    };

    info.rgTrackGain = getDb("REPLAYGAIN_TRACK_GAIN");
    info.rgTrackPeak = getDb("REPLAYGAIN_TRACK_PEAK");
    info.rgAlbumGain = getDb("REPLAYGAIN_ALBUM_GAIN");
    info.rgAlbumPeak = getDb("REPLAYGAIN_ALBUM_PEAK");
}

/// Wydobywa AcoustID i MusicBrainz IDs z PropertyMap
void extractMusicBrainzIds(const TagLib::PropertyMap& map, TagInfo& info) {
    auto getStr = [&map](const char* key) -> QString {
        const auto it = map.find(key);
        if (it == map.end() || it->second.isEmpty()) return {};
        return tlString(it->second.front());
    };
    info.acoustid = getStr("ACOUSTID_ID");
    info.mbRecordingId = getStr("MUSICBRAINZ_TRACKID");
    info.mbReleaseId = getStr("MUSICBRAINZ_ALBUMID");
    info.mbArtistId = getStr("MUSICBRAINZ_ARTISTID");
}

/// Czyta okładkę z ID3v2 APIC frame
bool extractCoverFromId3v2(TagLib::ID3v2::Tag* tag, TagInfo& info) {
    if (!tag) return false;
    const auto frames = tag->frameListMap()["APIC"];
    if (frames.isEmpty()) return false;
    auto* pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
    if (!pic) return false;

    info.coverData = QByteArray(pic->picture().data(),
                                static_cast<int>(pic->picture().size()));
    info.coverMime = tlString(pic->mimeType());
    return true;
}

/// Czyta okładkę z FLAC picture block
bool extractCoverFromFlac(TagLib::FLAC::File* file, TagInfo& info) {
    if (!file) return false;
    const auto pics = file->pictureList();
    if (pics.isEmpty()) return false;
    const auto* pic = pics.front();
    info.coverData = QByteArray(pic->data().data(),
                                static_cast<int>(pic->data().size()));
    info.coverMime = tlString(pic->mimeType());
    return true;
}

/// Czyta okładkę z MP4 covr atom
bool extractCoverFromMp4(TagLib::MP4::Tag* tag, TagInfo& info) {
    if (!tag) return false;
    if (!tag->itemMap().contains("covr")) return false;
    const auto cover = tag->itemMap()["covr"].toCoverArtList();
    if (cover.isEmpty()) return false;
    info.coverData = QByteArray(cover.front().data().data(),
                                static_cast<int>(cover.front().data().size()));
    info.coverMime = (cover.front().format() == TagLib::MP4::CoverArt::PNG)
                         ? QStringLiteral("image/png")
                         : QStringLiteral("image/jpeg");
    return true;
}

} // anonymous namespace

Result<TagInfo> TagInfo::fromFile(const QString& path) {
    if (!QFileInfo::exists(path)) {
        return Result<TagInfo>::err(Error::FileNotFound,
            QStringLiteral("File not found: %1").arg(path));
    }

    TagLib::FileRef ref(path.toUtf8().constData());
    if (ref.isNull() || !ref.tag()) {
        return Result<TagInfo>::err(Error::InvalidFormat,
            QStringLiteral("Cannot read tags from %1").arg(path));
    }

    TagInfo info;

    // Generic tag fields
    auto* tag = ref.tag();
    info.title   = tlString(tag->title());
    info.artist  = tlString(tag->artist());
    info.album   = tlString(tag->album());
    info.year    = static_cast<int>(tag->year());
    info.genre   = tlString(tag->genre());
    info.comment = tlString(tag->comment());
    info.trackNumber = static_cast<int>(tag->track());

    // PropertyMap dla bardziej zaawansowanych pól
    const auto props = tag->properties();
    extractReplayGain(props, info);
    extractMusicBrainzIds(props, info);

    // Disc number / track total
    if (props.contains("TRACKNUMBER")) {
        const QString tnVal = tlString(props["TRACKNUMBER"].front());
        const auto [tn, tt] = parseSlashNumber(tnVal);
        if (tn > 0) info.trackNumber = tn;
        if (tt > 0) info.trackTotal = tt;
    }
    if (props.contains("DISCNUMBER")) {
        const QString dnVal = tlString(props["DISCNUMBER"].front());
        const auto [dn, dt] = parseSlashNumber(dnVal);
        info.discNumber = dn;
        info.discTotal = dt;
    }
    if (props.contains("ALBUMARTIST")) {
        info.albumArtist = tlString(props["ALBUMARTIST"].front());
    }

    // Audio properties
    if (auto* ap = ref.audioProperties()) {
        info.durationMs = ap->lengthInMilliseconds();
        info.bitrate    = ap->bitrate();
        info.samplerate = ap->sampleRate();
        info.channels   = ap->channels();
    }

    // Cover art — format-specific
    if (auto* mp3 = dynamic_cast<TagLib::MPEG::File*>(ref.file())) {
        extractCoverFromId3v2(mp3->ID3v2Tag(), info);
    } else if (auto* flac = dynamic_cast<TagLib::FLAC::File*>(ref.file())) {
        extractCoverFromFlac(flac, info);
        // FLAC może też mieć ID3v2
        if (info.coverData.isEmpty()) {
            extractCoverFromId3v2(flac->ID3v2Tag(), info);
        }
    } else if (auto* mp4 = dynamic_cast<TagLib::MP4::File*>(ref.file())) {
        extractCoverFromMp4(mp4->tag(), info);
    }

    qCDebug(lcTag) << "Read tags from" << path
                   << "title:" << info.title
                   << "artist:" << info.artist
                   << "format:" << ref.file()->name()
                   << "duration:" << info.durationMs << "ms";

    return Result<TagInfo>::ok(std::move(info));
}

Result<void> TagInfo::saveTo(const QString& path,
                              bool writeId3v1,
                              bool writeId3v24) const {
    Q_UNUSED(writeId3v1);
    Q_UNUSED(writeId3v24);

    TagLib::FileRef ref(path.toUtf8().constData());
    if (ref.isNull() || !ref.tag()) {
        return Result<void>::err(Error::InvalidFormat,
            QStringLiteral("Cannot open %1 for tag write").arg(path));
    }

    auto* tag = ref.tag();
    tag->setTitle(fromQ(title));
    tag->setArtist(fromQ(artist));
    tag->setAlbum(fromQ(album));
    tag->setGenre(fromQ(genre));
    tag->setYear(static_cast<unsigned int>(year));
    tag->setComment(fromQ(comment));
    tag->setTrack(static_cast<unsigned int>(trackNumber));

    // PropertyMap dla pól nieobsługiwanych przez generic Tag interface
    auto props = tag->properties();
    if (!albumArtist.isEmpty()) {
        props.replace("ALBUMARTIST", fromQ(albumArtist));
    }
    if (discNumber > 0) {
        const QString dn = (discTotal > 0)
            ? QStringLiteral("%1/%2").arg(discNumber).arg(discTotal)
            : QString::number(discNumber);
        props.replace("DISCNUMBER", fromQ(dn));
    }
    if (rgTrackGain.has_value()) {
        props.replace("REPLAYGAIN_TRACK_GAIN",
            fromQ(QStringLiteral("%1 dB").arg(*rgTrackGain, 0, 'f', 2)));
    }
    if (rgTrackPeak.has_value()) {
        props.replace("REPLAYGAIN_TRACK_PEAK",
            fromQ(QString::number(*rgTrackPeak, 'f', 6)));
    }
    if (rgAlbumGain.has_value()) {
        props.replace("REPLAYGAIN_ALBUM_GAIN",
            fromQ(QStringLiteral("%1 dB").arg(*rgAlbumGain, 0, 'f', 2)));
    }
    if (rgAlbumPeak.has_value()) {
        props.replace("REPLAYGAIN_ALBUM_PEAK",
            fromQ(QString::number(*rgAlbumPeak, 'f', 6)));
    }
    if (!acoustid.isEmpty()) {
        props.replace("ACOUSTID_ID", fromQ(acoustid));
    }
    if (!mbRecordingId.isEmpty()) {
        props.replace("MUSICBRAINZ_TRACKID", fromQ(mbRecordingId));
    }
    tag->setProperties(props);

    if (!ref.save()) {
        return Result<void>::err(Error::FileAccessDenied,
            QStringLiteral("Failed to save tags to %1").arg(path));
    }

    qCDebug(lcTag) << "Saved tags to" << path;
    return Result<void>::ok();
}

void TagInfo::applyToTrack(Track& track) const {
    track.title = title;
    track.artist = artist;
    track.albumArtist = albumArtist.isEmpty() ? artist : albumArtist;
    track.album = album;
    track.genre = genre;
    track.year = year;
    track.trackNumber = trackNumber;
    track.discNumber = discNumber;
    track.durationMs = durationMs;
    track.bitrate = bitrate;
    track.samplerate = samplerate;
    track.channels = channels;
    track.comment = comment;
    track.rgTrackGain = rgTrackGain;
    track.rgTrackPeak = rgTrackPeak;
    track.rgAlbumGain = rgAlbumGain;
    track.rgAlbumPeak = rgAlbumPeak;
    track.acoustid = acoustid;
    track.mbRecordingId = mbRecordingId;
}

} // namespace soundshelf
