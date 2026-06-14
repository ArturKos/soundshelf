#include "soundshelf/core/ChromaprintEngine.hpp"

#include "soundshelf/io/PcmDecoder.hpp"

#include <QLoggingCategory>
#include <QFile>

#ifdef SOUNDSHELF_HAVE_CHROMAPRINT
#  include <chromaprint.h>
#endif

Q_LOGGING_CATEGORY(lcCp, "soundshelf.core.chromaprint")

namespace soundshelf {

ChromaprintEngine::ChromaprintEngine(QObject* parent) : QObject(parent) {}
ChromaprintEngine::~ChromaprintEngine() = default;

bool ChromaprintEngine::isAvailable() {
#ifdef SOUNDSHELF_HAVE_CHROMAPRINT
    return true;
#else
    return false;
#endif
}

Result<ChromaprintEngine::Fingerprint>
ChromaprintEngine::fingerprintFile(const QString& audioFile) {
#ifdef SOUNDSHELF_HAVE_CHROMAPRINT
    // libchromaprint can decode via its bundled FFmpeg, but to keep one
    // decoding path (and avoid an extra FFmpeg link) we decode through the
    // shared PcmDecoder (ffmpeg subprocess) and feed raw PCM to fingerprintPcm.
    auto pcm = PcmDecoder::decodeToS16(audioFile, 44100, 2);
    if (!pcm) return Result<Fingerprint>::err(pcm.error().code, pcm.error().message);
    const auto& buf = pcm.value();
    return fingerprintPcm(buf.s16le, buf.sampleRate, buf.channels, buf.totalSamples);
#else
    Q_UNUSED(audioFile);
    return Result<Fingerprint>::err(Error::DependencyMissing,
        QStringLiteral("Chromaprint not compiled in"));
#endif
}

Result<ChromaprintEngine::Fingerprint>
ChromaprintEngine::fingerprintPcm(const QByteArray& pcmS16Le,
                                  int sampleRate,
                                  int channels,
                                  int totalSamples) {
#ifndef SOUNDSHELF_HAVE_CHROMAPRINT
    Q_UNUSED(pcmS16Le); Q_UNUSED(sampleRate);
    Q_UNUSED(channels); Q_UNUSED(totalSamples);
    return Result<Fingerprint>::err(Error::DependencyMissing,
        QStringLiteral("Chromaprint not compiled in"));
#else
    if (channels < 1 || channels > 2 || sampleRate <= 0) {
        return Result<Fingerprint>::err(Error::InvalidArgument,
            QStringLiteral("invalid PCM parameters"));
    }
    ChromaprintContext* ctx = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
    if (!ctx) {
        return Result<Fingerprint>::err(Error::Unknown,
            QStringLiteral("chromaprint_new returned null"));
    }
    if (!chromaprint_start(ctx, sampleRate, channels)) {
        chromaprint_free(ctx);
        return Result<Fingerprint>::err(Error::Unknown,
            QStringLiteral("chromaprint_start failed"));
    }
    const int frames = pcmS16Le.size() / 2;  // 16-bit samples
    if (!chromaprint_feed(ctx,
            reinterpret_cast<const int16_t*>(pcmS16Le.constData()),
            frames)) {
        chromaprint_free(ctx);
        return Result<Fingerprint>::err(Error::Unknown,
            QStringLiteral("chromaprint_feed failed"));
    }
    if (!chromaprint_finish(ctx)) {
        chromaprint_free(ctx);
        return Result<Fingerprint>::err(Error::Unknown,
            QStringLiteral("chromaprint_finish failed"));
    }
    char* fp = nullptr;
    if (!chromaprint_get_fingerprint(ctx, &fp) || !fp) {
        chromaprint_free(ctx);
        return Result<Fingerprint>::err(Error::Unknown,
            QStringLiteral("chromaprint_get_fingerprint failed"));
    }
    Fingerprint out;
    out.fingerprint = QString::fromUtf8(fp);
    out.durationSec = totalSamples / sampleRate;
    chromaprint_dealloc(fp);
    chromaprint_free(ctx);
    qCDebug(lcCp) << "fingerprint length=" << out.fingerprint.size()
                  << "duration=" << out.durationSec << "s";
    return Result<Fingerprint>::ok(std::move(out));
#endif
}

} // namespace soundshelf
