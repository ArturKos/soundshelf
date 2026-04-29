#pragma once

#include <QString>
#include <QImage>
#include <QByteArray>
#include <optional>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/// Wrapper nad TagLib.
/// Czyta i zapisuje ID3v1, ID3v2.3/2.4, Vorbis Comment, FLAC, MP4 atoms.
class TagInfo {
public:
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
    int year = 0;
    QString genre;
    int trackNumber = 0;
    int trackTotal = 0;
    int discNumber = 0;
    int discTotal = 0;
    QString comment;

    // Audio properties
    int durationMs = 0;
    int bitrate = 0;
    int samplerate = 0;
    int channels = 0;

    // Cover art (first APIC frame found)
    QByteArray coverData;
    QString coverMime;

    // ReplayGain (jeśli obecne w tagach)
    std::optional<double> rgTrackGain;
    std::optional<double> rgTrackPeak;
    std::optional<double> rgAlbumGain;
    std::optional<double> rgAlbumPeak;

    // MusicBrainz / AcoustID (jeśli obecne)
    QString acoustid;
    QString mbRecordingId;
    QString mbReleaseId;
    QString mbArtistId;

    /// Wczytuje tagi z pliku. Próbuje ID3v2, fallback na ID3v1, potem Vorbis/FLAC/...
    static Result<TagInfo> fromFile(const QString& path);

    /// Zapisuje tagi do pliku. Domyślnie ID3v2.4 + ID3v1 dla MP3.
    Result<void> saveTo(const QString& path,
                        bool writeId3v1 = true,
                        bool writeId3v24 = true) const;

    /// Aplikuje wartości z TagInfo do struktury Track (bez ID-ków FK).
    void applyToTrack(Track& track) const;
};

} // namespace soundshelf
