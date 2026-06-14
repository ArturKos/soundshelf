#pragma once

#include <QString>
#include <QByteArray>
#include <cstdint>
#include <functional>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Decodes any audio file to interleaved signed 16-bit little-endian PCM
 *        by spawning the system `ffmpeg(1)` binary.
 *
 * Like @ref FormatConverter this deliberately does **not** link FFmpeg (that
 * would clash with libmpv's bundled copy and bloat the binary) — it shells out
 * to `ffmpeg` on `PATH`. The PCM it produces is the common feedstock for the
 * offline analysers that need raw samples but don't get them from libmpv:
 * @ref ReplayGainAnalyzer (EBU R128) and @ref ChromaprintEngine (AcoustID).
 *
 * Both calls are **synchronous and blocking** — run them on a worker thread
 * (`QtConcurrent::run` / `QThread`), never on the GUI thread.
 */
class PcmDecoder {
public:
    /// Whole-file decode result. @ref totalSamples is the per-channel frame
    /// count, i.e. `s16le.size() / 2 / channels`, so `totalSamples / sampleRate`
    /// yields the duration in seconds.
    struct PcmBuffer {
        QByteArray s16le;        ///< interleaved signed 16-bit little-endian
        int sampleRate   = 0;
        int channels     = 0;
        int totalSamples = 0;    ///< per-channel frame count
    };

    /// Streaming sink: receives interleaved S16 frames as they are decoded.
    /// @p frames is the per-channel frame count in this chunk. Return false to
    /// abort decoding early (the ffmpeg process is killed).
    using SampleSink = std::function<bool(const int16_t* samples, int frames)>;

    /// Path of the `ffmpeg` binary (`PATH` lookup); empty if not found.
    static QString ffmpegPath();

    /// True if a usable `ffmpeg` is available on `PATH`.
    static bool isAvailable();

    /// Decodes @p path fully into an in-memory @ref PcmBuffer.
    static Result<PcmBuffer> decodeToS16(const QString& path,
                                         int targetRate = 44100,
                                         int channels   = 2);

    /// Decodes @p path streaming full frames to @p sink (low memory footprint).
    /// On success writes the total per-channel frame count to @p outTotalSamples
    /// when non-null.
    static Result<void> streamS16(const QString& path,
                                  const SampleSink& sink,
                                  int targetRate = 44100,
                                  int channels   = 2,
                                  int* outTotalSamples = nullptr);
};

} // namespace soundshelf
