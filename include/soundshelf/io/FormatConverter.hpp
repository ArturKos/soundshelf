#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Audio-format conversion driven by an external `ffmpeg` binary.
 *
 * The class is a thin `QProcess` wrapper that emits progress and result
 * signals. It explicitly does **not** link FFmpeg — that would conflict
 * with libmpv's bundled FFmpeg and bloat the binary. Instead it spawns
 * the system `ffmpeg(1)`.
 *
 * Conversion preserves all tags via `-map_metadata 0` and copies cover
 * art via `-c:v copy` for containers that allow it.
 *
 * Typical usage:
 * @code
 * FormatConverter conv;
 * Job j {
 *     .input  = "/music/track.flac",
 *     .output = "/tmp/track.mp3",
 *     .format = Format::Mp3V0,
 * };
 * conv.start(j);
 * @endcode
 */
class FormatConverter : public QObject {
    Q_OBJECT
public:
    /// Output target presets understood by this class.
    enum class Format {
        Mp3V0,        ///< -c:a libmp3lame -q:a 0
        Mp3_320,      ///< -c:a libmp3lame -b:a 320k
        OggVorbis,    ///< -c:a libvorbis  -q:a 6
        Opus_128,     ///< -c:a libopus    -b:a 128k
        Aac_256,      ///< -c:a aac        -b:a 256k
        Flac,         ///< -c:a flac       -compression_level 8
        WavPcm16,     ///< -c:a pcm_s16le
    };

    /// One conversion job description.
    struct Job {
        QString input;
        QString output;
        Format  format = Format::Mp3V0;
        int     samplerateOverride = 0;   ///< 0 = keep input
        int     channelsOverride   = 0;   ///< 0 = keep input
        bool    overwrite          = false;
    };

    explicit FormatConverter(QObject* parent = nullptr);
    ~FormatConverter() override;

    /// Returns the path of the ffmpeg binary (`PATH` lookup).
    static QString ffmpegPath();

    /// Returns true if a usable `ffmpeg` is available on `PATH`.
    static bool isAvailable();

    /// Builds the argv that will be passed to ffmpeg for @p job.
    /// Public for testability.
    static QStringList buildArguments(const Job& job);

    /// Starts the conversion. Returns immediately.
    /// Watch @ref finished / @ref progress for the outcome.
    Result<void> start(const Job& job);

    /// True between @ref start and @ref finished.
    bool isRunning() const;

    /// Aborts the running process.
    void cancel();

signals:
    /// Emitted with progress percentage (0–100) parsed from ffmpeg's stderr.
    void progress(int pct);

    /// Emitted once when the job ends. @p ok = true on exit code 0.
    void finished(bool ok, const QString& message);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace soundshelf
