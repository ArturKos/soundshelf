#include "soundshelf/core/ReplayGainAnalyzer.hpp"

#include <QLoggingCategory>

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

} // namespace

Result<ReplayGainAnalyzer::TrackResult>
ReplayGainAnalyzer::analyseFile(const QString& path) {
#ifndef SOUNDSHELF_HAVE_EBUR128
    Q_UNUSED(path);
    return Result<TrackResult>::err(Error::DependencyMissing,
        QStringLiteral("libebur128 not compiled in"));
#else
    // libebur128 needs PCM, which means we'd be decoding here.
    // The recommended path is to feed it from PlayerEngine's PCM tap
    // (the same callback used for the spectrum/visualisation), or to
    // shell out to `ffmpeg -f s16le ...` and pump samples in.
    // We expose the API and the LUFS→RG conversion; the decode glue
    // is left for the integration with PlayerEngine.
    Q_UNUSED(path);
    return Result<TrackResult>::err(Error::NotImplemented,
        QStringLiteral("PCM-source pipeline pending PlayerEngine PCM tap"));
#endif
}

Result<ReplayGainAnalyzer::AlbumResult>
ReplayGainAnalyzer::analyseAlbum(const QList<QString>& paths) {
#ifndef SOUNDSHELF_HAVE_EBUR128
    Q_UNUSED(paths);
    return Result<AlbumResult>::err(Error::DependencyMissing,
        QStringLiteral("libebur128 not compiled in"));
#else
    AlbumResult album;
    QList<ebur128_state*> states;
    states.reserve(paths.size());
    for (const QString& p : paths) {
        auto tr = analyseFile(p);
        if (!tr) {
            for (auto* s : states) ebur128_destroy(&s);
            return Result<AlbumResult>::err(tr.error().code, tr.error().message);
        }
        album.tracks.append(tr.value());
    }
    if (!album.tracks.isEmpty()) {
        double sumLufs = 0;
        double maxPeak = 0;
        for (const auto& t : album.tracks) {
            sumLufs += t.integratedLufs;
            if (t.trackPeak > maxPeak) maxPeak = t.trackPeak;
        }
        album.integratedLufs = sumLufs / album.tracks.size();
        album.albumGainDb = rg2GainFromLufs(album.integratedLufs);
        album.albumPeak = maxPeak;
    }
    return Result<AlbumResult>::ok(std::move(album));
#endif
}

Result<void> ReplayGainAnalyzer::writeTagsTrack(const QString& path,
                                                const TrackResult& tr) const {
    Q_UNUSED(path);
    Q_UNUSED(tr);
    // Real implementation goes through TagLib's TXXX:replaygain_track_gain
    // (ID3v2) / `REPLAYGAIN_TRACK_GAIN` (Vorbis/FLAC). Placeholder until
    // the analyser is wired to a working PCM source.
    return Result<void>::err(Error::NotImplemented,
        QStringLiteral("RG tag write pending analyser PCM source"));
}

Result<void> ReplayGainAnalyzer::writeTagsAlbum(const QString& path,
                                                const TrackResult& tr,
                                                const AlbumResult& al) const {
    Q_UNUSED(path); Q_UNUSED(tr); Q_UNUSED(al);
    return Result<void>::err(Error::NotImplemented,
        QStringLiteral("RG tag write pending analyser PCM source"));
}

} // namespace soundshelf
