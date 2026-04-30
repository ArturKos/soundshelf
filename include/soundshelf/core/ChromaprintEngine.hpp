#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Computes Chromaprint / AcoustID acoustic fingerprints.
 *
 * Wraps `libchromaprint`. The library decodes audio (or accepts raw
 * PCM) and produces a compact, robust fingerprint that AcoustID's web
 * service can map to a MusicBrainz Recording id.
 *
 * The implementation here exposes the two flavours `libchromaprint`
 * supports:
 *  - @ref fingerprintFile — uses chromaprint's built-in audio decoder
 *    (which links to FFmpeg). Easy, but couples the build to FFmpeg.
 *  - @ref fingerprintPcm — accepts an interleaved S16 PCM buffer the
 *    caller produces (e.g. via libmpv's PCM tap). Lighter, gives full
 *    control over decoding.
 *
 * On builds without `SOUNDSHELF_HAVE_CHROMAPRINT` both methods return
 * @c Error::DependencyMissing immediately.
 */
class ChromaprintEngine : public QObject {
    Q_OBJECT
public:
    /// Audio fingerprint plus the source duration (seconds) — both
    /// values are required by AcoustID's `/v2/lookup` endpoint.
    struct Fingerprint {
        QString fingerprint;   ///< chromaprint base64-ish string
        int     durationSec = 0;
    };

    explicit ChromaprintEngine(QObject* parent = nullptr);
    ~ChromaprintEngine() override;

    static bool isAvailable();

    /// Decodes @p audioFile end-to-end and returns its fingerprint.
    Result<Fingerprint> fingerprintFile(const QString& audioFile);

    /// Builds a fingerprint from raw interleaved 16-bit PCM data.
    /// @param pcmS16Le  interleaved S16 little-endian
    /// @param sampleRate 44100, 48000, ...
    /// @param channels   1 or 2
    /// @param totalSamples  total **per-channel** samples in the input
    Result<Fingerprint> fingerprintPcm(const QByteArray& pcmS16Le,
                                       int sampleRate,
                                       int channels,
                                       int totalSamples);
};

} // namespace soundshelf
