#include "soundshelf/io/TagInfo.hpp"

#include <QFileInfo>
#include <QLoggingCategory>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v1tag.h>
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

#include <QStringDecoder>

Q_LOGGING_CATEGORY(lcTag, "soundshelf.tag")

namespace soundshelf {

namespace {

/// Detects CP1250 bytes mistakenly tagged as Latin-1 in ID3v2 frames.
/// Lots of older Polish taggers wrote raw CP1250 bytes into a frame
/// that declared encoding=0 (Latin-1). TagLib faithfully decodes
/// Latin-1 → Unicode, so we end up with chars like £ ¿ ¶ instead of
/// Ł ż ¶. These chars are extremely uncommon in real audio metadata —
/// when we see them, retry the decode as CP1250.
bool looksLikeCp1250MisreadAsLatin1(const QString& s) {
    // Polish-CP1250 bytes that, when read as Latin-1, become these
    // unusual Unicode codepoints. Cheap O(n) scan with a constant
    // codepoint set.
    for (QChar c : s) {
        const ushort u = c.unicode();
        if (u == 0x00A3   // £  (CP1250 Ł)
         || u == 0x00A5   // ¥  (CP1250 Ą)
         || u == 0x00A6   // ¦  (CP1250 Ś)
         || u == 0x00A8   // ¨  (CP1250 ¨ / dier.)
         || u == 0x00AC   // ¬  (CP1250 Ź)
         || u == 0x00AF   // ¯  (CP1250 Ż)
         || u == 0x00B3   // ³  (CP1250 ł)
         || u == 0x00B5   // µ  (CP1250 µ)
         || u == 0x00B6   // ¶  (CP1250 ś)
         || u == 0x00B9   // ¹  (CP1250 ą)
         || u == 0x00BC   // ¼  (CP1250 ź)
         || u == 0x00BF   // ¿  (CP1250 ż)
         || u == 0x00C6   // Æ  (CP1250 Ć)
         || u == 0x00CA   // Ê  (CP1250 Ę)
         || u == 0x00D3   // Ó  (CP1250 Ó — same!)
         || u == 0x00DF   // ß
         || u == 0x00E6   // æ  (CP1250 ć)
         || u == 0x00EA   // ê  (CP1250 ę)
         || u == 0x00F3   // ó
            ) return true;
    }
    return false;
}

/// Fixes one Latin-1-pretending-to-be-CP1250 codepoint. Returns the
/// original char if it isn't one of the Polish CP1250 collisions.
QChar fixupCp1250Char(QChar c) {
    switch (c.unicode()) {
        case 0x00A3: return QChar(0x0141);  // £ → Ł
        case 0x00A5: return QChar(0x0104);  // ¥ → Ą
        case 0x00A6: return QChar(0x015A);  // ¦ → Ś
        case 0x00AC: return QChar(0x0179);  // ¬ → Ź
        case 0x00AF: return QChar(0x017B);  // ¯ → Ż
        case 0x00B3: return QChar(0x0142);  // ³ → ł
        case 0x00B6: return QChar(0x015B);  // ¶ → ś
        case 0x00B9: return QChar(0x0105);  // ¹ → ą
        case 0x00BC: return QChar(0x017A);  // ¼ → ź
        case 0x00BF: return QChar(0x017C);  // ¿ → ż
        case 0x00C6: return QChar(0x0106);  // Æ → Ć
        case 0x00CA: return QChar(0x0118);  // Ê → Ę
        case 0x00D1: return QChar(0x0143);  // Ñ → Ń
        case 0x00E6: return QChar(0x0107);  // æ → ć
        case 0x00EA: return QChar(0x0119);  // ê → ę
        case 0x00F1: return QChar(0x0144);  // ñ → ń
        // ó/Ó share the same code point in CP1250 and Latin-1 (0xF3/0xD3),
        // so no remap is needed.
        default: return c;
    }
}

QString tlString(const TagLib::String& s) {
    if (s.isEmpty()) return {};
    QString out = QString::fromStdString(s.to8Bit(true));
    if (looksLikeCp1250MisreadAsLatin1(out)) {
        for (int i = 0; i < out.size(); ++i) out[i] = fixupCp1250Char(out[i]);
    }
    return out;
}

/// ID3v1 has no encoding marker — the spec says Latin-1 but real-world
/// Polish files use Windows-1250. TagLib's default StringHandler reads
/// raw bytes as Latin-1; we override with a decoder that tries UTF-8
/// first (some taggers do this) and falls back to CP1250.
class Cp1250StringHandler : public TagLib::ID3v1::StringHandler {
public:
    TagLib::String parse(const TagLib::ByteVector& data) const override {
        QByteArray ba(data.data(), int(data.size()));
        // Strip trailing NULs and spaces (ID3v1 fields are space/NUL padded).
        while (!ba.isEmpty() && (ba.back() == '\0' || ba.back() == ' ')) ba.chop(1);
        if (ba.isEmpty()) return TagLib::String();

        // UTF-8 first — only succeeds if every byte is valid UTF-8.
        QStringDecoder utf8(QStringDecoder::Utf8,
                            QStringDecoder::Flag::ConvertInvalidToNull);
        const QString u = utf8.decode(ba);
        if (!utf8.hasError()) {
            return TagLib::String(u.toStdString(), TagLib::String::UTF8);
        }
        // Fallback: read as Latin-1 then remap the Polish-specific
        // codepoints. Qt 6 doesn't ship CP1250 in QStringConverter, so
        // we go through the same fixup table tlString() uses.
        QString s = QString::fromLatin1(ba);
        for (int i = 0; i < s.size(); ++i) s[i] = fixupCp1250Char(s[i]);
        return TagLib::String(s.toStdString(), TagLib::String::UTF8);
    }

    TagLib::ByteVector render(const TagLib::String& s) const override {
        // ID3v1 has no Polish-aware encoder in stdlib; persist as
        // Latin-1 (lossy) and let modern taggers prefer ID3v2.
        const QString q = QString::fromStdString(s.to8Bit(true));
        const QByteArray ba = q.toLatin1();
        return TagLib::ByteVector(ba.constData(), uint(ba.size()));
    }
};

void installCp1250Handler() {
    static Cp1250StringHandler handler;
    static bool installed = false;
    if (!installed) {
        TagLib::ID3v1::Tag::setStringHandler(&handler);
        installed = true;
    }
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
    installCp1250Handler();

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

    // PropertyMap dla bardziej zaawansowanych pól. Czytamy przez
    // File::properties() (nie Tag::properties()) — ten pierwszy zwraca custom
    // klucze jak REPLAYGAIN_* / ACOUSTID_ID z natywnych ramek (Xiph, ID3v2
    // TXXX/RVA2, MP4), które generyczny Tag potrafi pominąć.
    const auto props = ref.file()->properties();
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
    // Write through File::setProperties (not the generic Tag::setProperties):
    // the latter silently drops custom keys like REPLAYGAIN_* / ACOUSTID_ID
    // for several formats, whereas File::setProperties maps them to the right
    // native frames (Xiph comments, ID3v2 TXXX/RVA2, MP4 atoms).
    ref.file()->setProperties(props);

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
    track.coverData = coverData;
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
