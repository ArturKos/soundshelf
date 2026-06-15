#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include "soundshelf/io/DiscReader.hpp"
#include "soundshelf/io/CueParser.hpp"

namespace soundshelf {

/**
 * @brief Reads disc images backed by a CUE sheet plus one or more audio containers.
 *
 * Accepts a path that points either at a `.cue` file or at the audio
 * container itself (FLAC/WAV/APE/MP3) accompanied by a sibling CUE.
 * The class delegates parsing to @ref CueParser and timing to @ref TagInfo
 * (for the total-length probe of the underlying file).
 *
 * Two layouts are supported:
 *  - **Single-file**: one container holds all tracks (classic BIN/CUE, FLAC/CUE).
 *  - **Multi-file**: one container per track (EAC per-track, WAV+CUE). Each
 *    referenced FILE is resolved independently relative to the .cue directory.
 *
 * Plain `.iso` data tracks are not yet supported — SoundShelf is concerned
 * with audio CDs only.
 *
 * @note Direct playback is supported (libmpv can seek into the container
 * with `--start=` / `--end=`). Ripping is not supported because the data
 * is already on disk.
 */
class ImageReader : public DiscReader {
public:
    explicit ImageReader(QString path);

    Result<Toc> readToc() override;
    DiscType type() const override { return DiscType::Image; }
    QString source() const override { return m_path; }
    bool supportsRipping() const override { return false; }
    bool supportsDirectPlayback() const override { return true; }

    /// The parsed CUE sheet — populated after @ref readToc.
    const CueParser::CueSheet& sheet() const { return m_sheet; }

    /// Resolved path of the first (or only) audio container.
    /// For multi-file sheets this is the container for track 1.
    QString audioFile() const { return m_audioPath; }

    /** Resolved absolute paths of all audio containers referenced by the sheet,
     *  in the order they appear in the CUE.  For single-file sheets the list
     *  contains exactly one entry (same as @ref audioFile()). */
    QStringList audioFiles() const { return m_audioPaths; }

private:
    Result<QString> resolveCue(const QString& given);
    Result<int> probeAudioDurationMs(const QString& audio);

    QString m_path;
    QString m_cuePath;
    QString m_audioPath;
    QStringList m_audioPaths;
    CueParser::CueSheet m_sheet;
};

} // namespace soundshelf
