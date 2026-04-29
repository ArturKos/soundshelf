#pragma once

#include <QString>
#include <QList>
#include <QObject>
#include <optional>
#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf {

/**
 * @brief Parser of `.cue` sheet files.
 *
 * A CUE sheet describes the structure of one or more audio data files
 * (commonly a single FLAC/WAV/APE rip of a whole CD) by recording the
 * `TRACK` indices and `INDEX` offsets. SoundShelf treats such an image
 * as one @ref Disc with @ref Track entries that share the same
 * underlying file but carry different `cueOffsetMs` / `cueDurationMs`.
 *
 * Supported CUE keywords: FILE, TRACK, INDEX 00/01, PREGAP, POSTGAP,
 * TITLE, PERFORMER, REM (DATE, GENRE, REPLAYGAIN_*), ISRC, FLAGS.
 *
 * The parser is permissive — unknown keywords are skipped, line endings
 * are normalised, surrounding quotes on string args are stripped.
 *
 * @note The parser does not open the audio file referenced by `FILE`;
 * it only computes timing from `INDEX` offsets and the optional
 * `RUNTIME` rem. Real-world durations come from libmpv at playback
 * or are filled in by the caller via @c TagInfo.
 */
class CueParser : public QObject {
    Q_OBJECT
public:
    /// One TRACK NN AUDIO entry from a CUE sheet.
    struct CueTrack {
        int trackNumber = 0;        ///< 1-based as in the file
        QString title;              ///< from TITLE
        QString performer;          ///< from PERFORMER
        QString isrc;               ///< from ISRC
        long indexZeroFrames = -1;  ///< INDEX 00 — pregap start (75 frames = 1 s)
        long indexOneFrames = -1;   ///< INDEX 01 — actual track start
        std::optional<double> rgTrackGain;
        std::optional<double> rgTrackPeak;
    };

    /// Parsed contents of one CUE sheet.
    struct CueSheet {
        QString file;               ///< FILE "name.flac" WAVE/MP3/BINARY
        QString fileType;           ///< WAVE / MP3 / BINARY / ...
        QString albumTitle;
        QString albumPerformer;
        QString albumGenre;
        int year = 0;
        std::optional<double> rgAlbumGain;
        std::optional<double> rgAlbumPeak;
        QList<CueTrack> tracks;
    };

    explicit CueParser(QObject* parent = nullptr);
    ~CueParser() override;

    /// Reads and parses the given .cue file.
    Result<CueSheet> parseFile(const QString& cuePath);

    /// Parses CUE text from a string buffer (useful for tests).
    Result<CueSheet> parseString(const QString& cueText, const QString& sourceLabel = {});

    /// Converts a parsed CUE sheet into a @ref Toc with millisecond durations
    /// derived from `INDEX 01` deltas. The last track's `endSector`/`durationMs`
    /// are left at 0 unless @p totalAudioMs > 0 — only the caller knows the
    /// total length of the underlying audio file.
    static Toc tocFromSheet(const CueSheet& sheet, int totalAudioMs = 0);

    /// Frames-since-start (75 fps CD-DA) → milliseconds, with rounding.
    static int framesToMs(long frames);
};

} // namespace soundshelf
