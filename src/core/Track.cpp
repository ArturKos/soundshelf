#include "soundshelf/core/Track.hpp"
#include "soundshelf/core/Disc.hpp"
#include <QFileInfo>

namespace soundshelf {

QString audioFormatToString(AudioFormat fmt) {
    switch (fmt) {
        case AudioFormat::MP3:  return QStringLiteral("MP3");
        case AudioFormat::FLAC: return QStringLiteral("FLAC");
        case AudioFormat::OGG:  return QStringLiteral("OGG");
        case AudioFormat::OPUS: return QStringLiteral("OPUS");
        case AudioFormat::AAC:  return QStringLiteral("AAC");
        case AudioFormat::WAV:  return QStringLiteral("WAV");
        case AudioFormat::ALAC: return QStringLiteral("ALAC");
        case AudioFormat::APE:  return QStringLiteral("APE");
        case AudioFormat::WV:   return QStringLiteral("WV");
        default:                return QStringLiteral("UNKNOWN");
    }
}

AudioFormat audioFormatFromString(const QString& s) {
    const QString u = s.toUpper();
    if (u == QLatin1String("MP3"))  return AudioFormat::MP3;
    if (u == QLatin1String("FLAC")) return AudioFormat::FLAC;
    if (u == QLatin1String("OGG") || u == QLatin1String("VORBIS")) return AudioFormat::OGG;
    if (u == QLatin1String("OPUS")) return AudioFormat::OPUS;
    if (u == QLatin1String("AAC") || u == QLatin1String("M4A")) return AudioFormat::AAC;
    if (u == QLatin1String("WAV"))  return AudioFormat::WAV;
    if (u == QLatin1String("ALAC")) return AudioFormat::ALAC;
    if (u == QLatin1String("APE"))  return AudioFormat::APE;
    if (u == QLatin1String("WV"))   return AudioFormat::WV;
    return AudioFormat::Unknown;
}

AudioFormat audioFormatFromFilename(const QString& filename) {
    const QString suffix = QFileInfo(filename).suffix().toUpper();
    if (suffix == QLatin1String("MP3"))  return AudioFormat::MP3;
    if (suffix == QLatin1String("FLAC")) return AudioFormat::FLAC;
    if (suffix == QLatin1String("OGG") || suffix == QLatin1String("OGA")) return AudioFormat::OGG;
    if (suffix == QLatin1String("OPUS")) return AudioFormat::OPUS;
    if (suffix == QLatin1String("AAC") || suffix == QLatin1String("M4A") || suffix == QLatin1String("MP4")) return AudioFormat::AAC;
    if (suffix == QLatin1String("WAV"))  return AudioFormat::WAV;
    if (suffix == QLatin1String("ALAC")) return AudioFormat::ALAC;
    if (suffix == QLatin1String("APE"))  return AudioFormat::APE;
    if (suffix == QLatin1String("WV"))   return AudioFormat::WV;
    return AudioFormat::Unknown;
}

double Track::effectiveReplayGainDb(bool albumMode) const {
    if (albumMode && rgAlbumGain.has_value()) {
        return *rgAlbumGain;
    }
    if (rgTrackGain.has_value()) {
        return *rgTrackGain;
    }
    if (rgAlbumGain.has_value()) {
        return *rgAlbumGain;  // fallback
    }
    return 0.0;
}

QString discTypeToString(DiscType t) {
    switch (t) {
        case DiscType::Physical: return QStringLiteral("physical");
        case DiscType::Folder:   return QStringLiteral("folder");
        case DiscType::Image:    return QStringLiteral("image");
        case DiscType::Remote:   return QStringLiteral("remote");
    }
    return QStringLiteral("folder");
}

DiscType discTypeFromString(const QString& s) {
    if (s == QLatin1String("physical")) return DiscType::Physical;
    if (s == QLatin1String("image"))    return DiscType::Image;
    if (s == QLatin1String("remote"))   return DiscType::Remote;
    return DiscType::Folder;
}

} // namespace soundshelf
