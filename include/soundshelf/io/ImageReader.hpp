#pragma once

#include <QString>
#include <memory>
#include "soundshelf/io/DiscReader.hpp"
#include "soundshelf/io/CueParser.hpp"

namespace soundshelf {

/**
 * @brief Reads disc images backed by a CUE sheet plus an audio container.
 *
 * Accepts a path that points either at a `.cue` file or at the audio
 * container itself (FLAC/WAV/APE/MP3) accompanied by a sibling CUE.
 * The class delegates parsing to @ref CueParser and timing to @ref TagInfo
 * (for the total-length probe of the underlying file).
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

    /// Resolved path of the audio container (FILE field in the CUE).
    QString audioFile() const { return m_audioPath; }

private:
    Result<QString> resolveCue(const QString& given);
    Result<int> probeAudioDurationMs(const QString& audio);

    QString m_path;
    QString m_cuePath;
    QString m_audioPath;
    CueParser::CueSheet m_sheet;
};

} // namespace soundshelf
