#pragma once

#include <QString>
#include <QDateTime>
#include <QImage>
#include <optional>

namespace soundshelf {

/// Audio format enumeration matching DB CHECK constraint.
enum class AudioFormat {
    Unknown,
    MP3,
    FLAC,
    OGG,
    OPUS,
    AAC,
    WAV,
    ALAC,
    APE,
    WV
};

QString audioFormatToString(AudioFormat fmt);
AudioFormat audioFormatFromString(const QString& s);
AudioFormat audioFormatFromFilename(const QString& filename);

/// In-memory representation of a track row.
/// Mirrors the `tracks` table.
struct Track {
    int id = -1;                        ///< -1 means "not yet persisted"
    QString filepath;
    QString filename;
    int discId = -1;
    int artistId = -1;
    int albumArtistId = -1;
    int genreId = -1;

    QString title;
    QString artist;                     ///< denormalised, filled by joins
    QString albumArtist;
    QString album;
    QString genre;

    int trackNumber = 0;
    int discNumber = 0;
    int year = 0;
    int durationMs = 0;
    int bitrate = 0;
    int samplerate = 0;
    int channels = 0;

    AudioFormat format = AudioFormat::Unknown;
    QString codecProfile;

    // ReplayGain
    std::optional<double> rgTrackGain;
    std::optional<double> rgTrackPeak;
    std::optional<double> rgAlbumGain;
    std::optional<double> rgAlbumPeak;

    // Fingerprint / IDs
    QString acoustid;
    QString mbRecordingId;
    QString md5Hash;

    // Cover art (cached thumbnail hash, full image stored separately)
    QByteArray coverHash;

    // Transient — set by TagInfo::applyToTrack, never read back from DB.
    // Used by upsertTrack to seed the album's cover_data on first insert.
    QByteArray coverData;

    // Stats
    int playCount = 0;
    int skipCount = 0;
    double rating = 0.0;
    QString comment;
    bool missing = false;

    QDateTime addedAt;
    QDateTime mtime;
    QDateTime lastPlayed;

    // Optional embedded cue offset (for "one big FLAC + cue")
    std::optional<int> cueOffsetMs;
    std::optional<int> cueDurationMs;

    bool isValid() const { return id >= 0 && !filepath.isEmpty(); }

    /// Effective gain to apply during playback, picking track or album RG
    /// according to mode. Returns 0.0 if no RG data.
    double effectiveReplayGainDb(bool albumMode = false) const;
};

} // namespace soundshelf
