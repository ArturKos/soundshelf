#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/**
 * @brief EBU R128 / ReplayGain 2.0 loudness analysis using libebur128.
 *
 * The analyzer measures integrated loudness (LUFS) and true peak,
 * then converts to the ReplayGain 2.0 reference level of -18 LUFS.
 *
 * Two flavours:
 *  - **track gain** — one value per file, usable on its own.
 *  - **album gain** — a single level for an entire album, computed
 *    from the union of all per-track measurements so that relative
 *    loudness between tracks is preserved.
 *
 * Both flavours can be written back to the track's tags via TagLib —
 * standard keys are `REPLAYGAIN_TRACK_GAIN` / `REPLAYGAIN_TRACK_PEAK`
 * (and `_ALBUM_*`) on Vorbis/FLAC/OGG and `TXXX:replaygain_*` frames
 * on ID3v2.
 */
class ReplayGainAnalyzer : public QObject {
    Q_OBJECT
public:
    /// Per-track measurement.
    struct TrackResult {
        double trackGainDb = 0.0;  ///< -18 LUFS reference
        double trackPeak  = 0.0;   ///< 0..1 linear
        double integratedLufs = 0.0;
    };

    /// Per-album measurement (union of multiple tracks).
    struct AlbumResult {
        double albumGainDb = 0.0;
        double albumPeak   = 0.0;
        double integratedLufs = 0.0;
        QList<TrackResult> tracks;  ///< parallel to the input file list
    };

    explicit ReplayGainAnalyzer(QObject* parent = nullptr);
    ~ReplayGainAnalyzer() override;

    static bool isAvailable();

    /// Analyses one file and returns its track-gain measurement.
    /// Synchronous — callers should run on a worker thread for big files.
    Result<TrackResult> analyseFile(const QString& path);

    /// Analyses several files and computes the album result.
    /// Files are decoded sequentially in this thread.
    Result<AlbumResult> analyseAlbum(const QList<QString>& paths);

    /// Persists the @p tr values into the file's tags (TagLib).
    Result<void> writeTagsTrack(const QString& path, const TrackResult& tr) const;
    Result<void> writeTagsAlbum(const QString& path,
                                const TrackResult& tr,
                                const AlbumResult& al) const;
};

} // namespace soundshelf
