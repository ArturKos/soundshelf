#pragma once

#include <QString>
#include <QList>
#include <QMap>
#include <QObject>
#include <optional>
#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf {

/**
 * @brief Parser of `.cue` sheet files.
 *
 * A CUE sheet describes the structure of one or more audio data files.
 * SoundShelf supports two layouts:
 *  - **Single-file**: one FILE line, every TRACK resides in that container.
 *  - **Multi-file**: one FILE line per TRACK (EAC per-track, WAV+CUE, APE+CUE, …);
 *    each @ref CueTrack carries the name of its own container and INDEX offsets
 *    are file-relative (typically 00:00:00 per track).
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
        /** Audio container this track belongs to (relative name as in the FILE directive).
         *  Empty for single-file sheets (use @ref CueSheet::file instead).
         *  Set to the owning FILE for multi-file sheets, allowing callers to map
         *  each track to its individual container. */
        QString file;
        /** File-type string of the owning FILE directive (e.g. @c "WAVE", @c "MP3").
         *  Empty for single-file sheets; mirrors the FILE type for multi-file sheets. */
        QString fileType;
    };

    /// Parsed contents of one CUE sheet.
    struct CueSheet {
        QString file;               ///< First FILE encountered (name.flac, name.wav, …)
        QString fileType;           ///< WAVE / MP3 / BINARY / … (from the first FILE)
        QString albumTitle;
        QString albumPerformer;
        QString albumGenre;
        int year = 0;
        std::optional<double> rgAlbumGain;
        std::optional<double> rgAlbumPeak;
        QList<CueTrack> tracks;

        /** Returns @c true when the sheet declares more than one distinct FILE container.
         *
         *  For such sheets each @ref CueTrack::file is set to its owning container;
         *  use @ref CueParser::tocFromSheet(const CueSheet&, const QMap<QString,int>&)
         *  to compute accurate per-track durations from individual file lengths.
         *  For single-file sheets this always returns @c false and @c CueTrack::file
         *  is empty for every track. */
        bool isMultiFile() const {
            QString first;
            for (const auto& t : tracks) {
                if (t.file.isEmpty()) continue;
                if (first.isEmpty()) { first = t.file; continue; }
                if (t.file != first) return true;
            }
            return false;
        }
    };

    explicit CueParser(QObject* parent = nullptr);
    ~CueParser() override;

    /// Reads and parses the given .cue file.
    Result<CueSheet> parseFile(const QString& cuePath);

    /// Parses CUE text from a string buffer (useful for tests).
    Result<CueSheet> parseString(const QString& cueText, const QString& sourceLabel = {});

    /** Converts a single-file CUE sheet into a @ref Toc with millisecond durations
     *  derived from `INDEX 01` deltas.  The last track's duration is left at 0
     *  unless @p totalAudioMs > 0 — only the caller knows the total audio length.
     *  @note For multi-file sheets use the @c QMap overload instead. */
    static Toc tocFromSheet(const CueSheet& sheet, int totalAudioMs = 0);

    /** Converts a multi-file CUE sheet into a @ref Toc using per-file total durations.
     *
     *  Each key in @p perFileDurationMs is the FILE name as it appears in the sheet
     *  (i.e. @c CueTrack::file, which may be relative).  For a track that is the
     *  last (or only) track in its file the duration runs to that file's end;
     *  tracks sharing a file use `INDEX 01` deltas within that file.
     *
     *  @param sheet             A sheet for which @ref CueSheet::isMultiFile returns @c true.
     *  @param perFileDurationMs Map from FILE name → total audio length in milliseconds.
     *  @return                  @ref Toc with one entry per track, in track order. */
    static Toc tocFromSheet(const CueSheet& sheet, const QMap<QString, int>& perFileDurationMs);

    /// Frames-since-start (75 fps CD-DA) → milliseconds, with rounding.
    static int framesToMs(long frames);
};

} // namespace soundshelf
