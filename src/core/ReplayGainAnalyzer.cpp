#include "soundshelf/core/ReplayGainAnalyzer.hpp"

#include "soundshelf/io/PcmDecoder.hpp"
#include "soundshelf/io/TagInfo.hpp"

#include <QLoggingCategory>
#include <cmath>

#ifdef SOUNDSHELF_HAVE_EBUR128
#  include <ebur128.h>
#endif

Q_LOGGING_CATEGORY(lcRg, "soundshelf.core.replaygain")

namespace soundshelf {

ReplayGainAnalyzer::ReplayGainAnalyzer(QObject* parent) : QObject(parent) {}
ReplayGainAnalyzer::~ReplayGainAnalyzer() = default;

bool ReplayGainAnalyzer::isAvailable() {
#ifdef SOUNDSHELF_HAVE_EBUR128
    return true;
#else
    return false;
#endif
}

namespace {

/// Reference level for ReplayGain 2.0 is -18 LUFS.
double rg2GainFromLufs(double lufs) {
    return -18.0 - lufs;
}

/// Loudness below this is treated as silence (libebur128 returns -inf/-HUGE).
constexpr double kSilenceLufs = -70.0;

// Decode rate/format fed to libebur128. ReplayGain is insensitive to a fixed
// 44.1 kHz resample, and a uniform format keeps the pipeline simple.
constexpr int kRate = 44100;
constexpr int kChannels = 2;

#ifdef SOUNDSHELF_HAVE_EBUR128
/// Largest per-channel sample peak (0..1 linear) across all channels.
double maxSamplePeak(ebur128_state* st, int channels) {
    double peak = 0.0;
    for (int c = 0; c < channels; ++c) {
        double p = 0.0;
        if (ebur128_sample_peak(st, static_cast<unsigned>(c), &p) == EBUR128_SUCCESS)
            peak = std::max(peak, p);
    }
    return peak;
}
#endif

} // namespace

Result<ReplayGainAnalyzer::TrackResult>
ReplayGainAnalyzer::analyseFile(const QString& path) {
#ifndef SOUNDSHELF_HAVE_EBUR128
    Q_UNUSED(path);
    return Result<TrackResult>::err(Error::DependencyMissing,
        QStringLiteral("libebur128 not compiled in"));
#else
    ebur128_state* st = ebur128_init(kChannels, kRate,
        EBUR128_MODE_I | EBUR128_MODE_SAMPLE_PEAK);
    if (!st) {
        return Result<TrackResult>::err(Error::Unknown,
            QStringLiteral("ebur128_init failed"));
    }

    auto fed = PcmDecoder::streamS16(path,
        [st](const int16_t* samples, int frames) {
            return ebur128_add_frames_short(st, samples,
                static_cast<size_t>(frames)) == EBUR128_SUCCESS;
        }, kRate, kChannels);
    if (!fed) {
        ebur128_destroy(&st);
        return Result<TrackResult>::err(fed.error().code, fed.error().message);
    }

    double lufs = kSilenceLufs;
    ebur128_loudness_global(st, &lufs);
    if (!std::isfinite(lufs) || lufs < kSilenceLufs) lufs = kSilenceLufs;

    TrackResult tr;
    tr.integratedLufs = lufs;
    tr.trackGainDb    = (lufs <= kSilenceLufs) ? 0.0 : rg2GainFromLufs(lufs);
    tr.trackPeak      = maxSamplePeak(st, kChannels);
    ebur128_destroy(&st);

    qCDebug(lcRg) << path << "LUFS=" << tr.integratedLufs
                  << "gain=" << tr.trackGainDb << "peak=" << tr.trackPeak;
    return Result<TrackResult>::ok(tr);
#endif
}

Result<ReplayGainAnalyzer::AlbumResult>
ReplayGainAnalyzer::analyseAlbum(const QList<QString>& paths) {
#ifndef SOUNDSHELF_HAVE_EBUR128
    Q_UNUSED(paths);
    return Result<AlbumResult>::err(Error::DependencyMissing,
        QStringLiteral("libebur128 not compiled in"));
#else
    if (paths.isEmpty()) {
        return Result<AlbumResult>::err(Error::InvalidArgument,
            QStringLiteral("no files to analyse"));
    }

    AlbumResult album;
    QList<ebur128_state*> states;
    states.reserve(paths.size());

    auto cleanup = [&states]() { for (auto* s : states) ebur128_destroy(&s); };

    double maxPeak = 0.0;
    for (const QString& p : paths) {
        ebur128_state* st = ebur128_init(kChannels, kRate,
            EBUR128_MODE_I | EBUR128_MODE_SAMPLE_PEAK);
        if (!st) { cleanup(); return Result<AlbumResult>::err(Error::Unknown,
            QStringLiteral("ebur128_init failed")); }

        auto fed = PcmDecoder::streamS16(p,
            [st](const int16_t* samples, int frames) {
                return ebur128_add_frames_short(st, samples,
                    static_cast<size_t>(frames)) == EBUR128_SUCCESS;
            }, kRate, kChannels);
        if (!fed) {
            ebur128_destroy(&st); cleanup();
            return Result<AlbumResult>::err(fed.error().code, fed.error().message);
        }
        states.append(st);

        double lufs = kSilenceLufs;
        ebur128_loudness_global(st, &lufs);
        if (!std::isfinite(lufs) || lufs < kSilenceLufs) lufs = kSilenceLufs;

        TrackResult tr;
        tr.integratedLufs = lufs;
        tr.trackGainDb    = (lufs <= kSilenceLufs) ? 0.0 : rg2GainFromLufs(lufs);
        tr.trackPeak      = maxSamplePeak(st, kChannels);
        maxPeak = std::max(maxPeak, tr.trackPeak);
        album.tracks.append(tr);
    }

    // Album loudness is computed over the union of all measurements, not the
    // arithmetic mean of per-track LUFS (which would be wrong).
    double albumLufs = kSilenceLufs;
    ebur128_loudness_global_multiple(states.data(),
        static_cast<size_t>(states.size()), &albumLufs);
    if (!std::isfinite(albumLufs) || albumLufs < kSilenceLufs) albumLufs = kSilenceLufs;

    album.integratedLufs = albumLufs;
    album.albumGainDb    = (albumLufs <= kSilenceLufs) ? 0.0 : rg2GainFromLufs(albumLufs);
    album.albumPeak      = maxPeak;

    cleanup();
    qCDebug(lcRg) << "album LUFS=" << album.integratedLufs
                  << "gain=" << album.albumGainDb << "peak=" << album.albumPeak;
    return Result<AlbumResult>::ok(std::move(album));
#endif
}

Result<void> ReplayGainAnalyzer::writeTagsTrack(const QString& path,
                                                const TrackResult& tr) const {
    auto info = TagInfo::fromFile(path);
    if (!info) return Result<void>::err(info.error().code, info.error().message);
    TagInfo t = info.value();
    t.rgTrackGain = tr.trackGainDb;
    t.rgTrackPeak = tr.trackPeak;
    return t.saveTo(path);
}

Result<void> ReplayGainAnalyzer::writeTagsAlbum(const QString& path,
                                                const TrackResult& tr,
                                                const AlbumResult& al) const {
    auto info = TagInfo::fromFile(path);
    if (!info) return Result<void>::err(info.error().code, info.error().message);
    TagInfo t = info.value();
    t.rgTrackGain = tr.trackGainDb;
    t.rgTrackPeak = tr.trackPeak;
    t.rgAlbumGain = al.albumGainDb;
    t.rgAlbumPeak = al.albumPeak;
    return t.saveTo(path);
}

} // namespace soundshelf
