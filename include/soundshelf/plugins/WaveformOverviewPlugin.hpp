#pragma once

#include <functional>
#include <QObject>
#include <QVector>
#include <QRectF>

#include "soundshelf/plugins/VisualizationPlugin.hpp"
#include "soundshelf/core/Track.hpp"
#include "soundshelf/io/PcmDecoder.hpp"
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

class PlayerEngine;

/**
 * @brief Full-track waveform overview visualisation with click-to-seek.
 *
 * On every track change the plugin decodes the audio file once on a worker
 * thread (@c QtConcurrent::run — the GUI thread is never blocked), bins the
 * resulting samples into a per-pixel min/max amplitude envelope, and caches
 * it. The cached envelope is then drawn as vertical amplitude bars with a
 * playback-cursor overlay on each @ref render call.
 *
 * Clicking or dragging anywhere on the hosting @ref SpectrumWidget calls
 * @ref handleClick which maps the x coordinate to a millisecond position and
 * forwards it to @ref PlayerEngine::seekMs.
 *
 * An injectable @ref DecodeFn seam (the same pattern as @ref VisualizationFeeder)
 * allows unit tests to supply a synthetic buffer without spawning @c ffmpeg.
 */
class WaveformOverviewPlugin : public VisualizationPlugin {
    Q_OBJECT
public:
    /// Signature of the PCM decoder used internally; injectable for tests.
    using DecodeFn = std::function<Result<PcmDecoder::PcmBuffer>(const QString&)>;

    /// Per-pixel amplitude envelope bin produced by @ref computeEnvelope.
    struct MinMax {
        float min = 0.0f; ///< Minimum amplitude in this bin (≤ 0 for real audio).
        float max = 0.0f; ///< Maximum amplitude in this bin (≥ 0 for real audio).
    };

    explicit WaveformOverviewPlugin(QObject* parent = nullptr);
    ~WaveformOverviewPlugin() override;

    /// @return Human-readable plugin name for display in the visualisation menu.
    QString displayName() const override { return QStringLiteral("Waveform overview"); }
    /// @return Stable identifier used for settings storage.
    QString id()          const override { return QStringLiteral("soundshelf.native.waveform"); }
    /// @return Longer description shown in plugin settings.
    QString description() const override {
        return QStringLiteral("Full-track amplitude overview with click-to-seek.");
    }

    Feed preferredFeed() const override { return WantSpectrum; }

    /**
     * @brief Wires the plugin to @p engine for seeking and track-change events.
     *
     * Connects to @ref PlayerEngine::trackChanged so the envelope is
     * recomputed automatically. Must be called before the first render.
     * Ownership of @p engine is NOT transferred.
     *
     * @param engine Non-null pointer to the active player engine.
     */
    void setEngine(PlayerEngine* engine);

    /**
     * @brief Overrides the PCM decoder (intended for unit tests).
     *
     * The production default calls @c PcmDecoder::decodeToS16(path, 44100, 2).
     * Passing a default-constructed (empty) @c std::function restores the default.
     *
     * @param fn Callable matching @ref DecodeFn. Must be non-empty.
     */
    void setDecoder(DecodeFn fn);

    /**
     * @brief Draws the waveform envelope and playback cursor.
     *
     * When the envelope has not been computed yet (e.g. immediately after a
     * track change) a dim placeholder centre line is drawn instead.
     *
     * @param painter  Active painter on the SpectrumWidget's back-buffer.
     * @param area     Target paint rectangle.
     * @param pcm      Ignored — this plugin uses the pre-computed envelope.
     * @param spectrum Ignored — this plugin uses the pre-computed envelope.
     */
    void render(QPainter& painter,
                const QRectF& area,
                const QVector<float>& pcm,
                const QVector<float>& spectrum) override;

    /**
     * @brief Seeks to the playback position corresponding to widget x-coordinate @p x.
     *
     * Uses @ref xToMs with the current track duration and calls
     * @ref PlayerEngine::seekMs.  Direction: UI → Core only.
     *
     * @param x    Widget-local x coordinate (pixels).
     * @param area Current paint area (same QRectF as passed to render).
     */
    void handleClick(double x, const QRectF& area);

    // ── Pure static helpers — testable without engine or file I/O ─────────

    /**
     * @brief Computes a per-bin min/max amplitude envelope from an S16LE PCM buffer.
     *
     * Downmixes interleaved S16LE channels to mono float in [−1, 1] by averaging,
     * divides the total frame count into @p bins contiguous ranges, and records the
     * minimum and maximum amplitude per bin. The last bin absorbs any remainder frames.
     *
     * @param pcm  Source buffer from @ref PcmDecoder::decodeToS16.
     * @param bins Target bin count (typically the widget width in pixels).
     * @return QVector<MinMax> of exactly @p bins elements, or empty when
     *         @p pcm.s16le is empty, @p pcm.totalSamples == 0, or @p bins <= 0.
     */
    static QVector<MinMax> computeEnvelope(const PcmDecoder::PcmBuffer& pcm, int bins);

    /**
     * @brief Maps a widget x-coordinate to a playback position in milliseconds.
     *
     * @p x is clamped to [@p area.left(), @p area.right()] before mapping.
     *
     * @param x          Widget-local x coordinate (pixels).
     * @param area       Widget paint area.
     * @param durationMs Total track duration in milliseconds.
     * @return Integer in [0, @p durationMs]. Returns 0 when @p durationMs <= 0.
     */
    static int xToMs(double x, const QRectF& area, int durationMs);

    /**
     * @brief Maps a playback position in milliseconds to a widget x-coordinate.
     *
     * @p ms is clamped to [0, @p durationMs] before mapping.
     *
     * @param ms         Playback position in milliseconds.
     * @param area       Widget paint area.
     * @param durationMs Total track duration in milliseconds.
     *                   Returns @c area.left() when @p durationMs <= 0.
     * @return X coordinate (pixels) in [@p area.left(), @p area.right()].
     */
    static double msToX(int ms, const QRectF& area, int durationMs);

private slots:
    void onTrackChanged(const soundshelf::Track& track);

private:
    void startDecoding(const QString& path);
    void clearEnvelope();

    PlayerEngine*   m_engine      = nullptr;
    DecodeFn        m_decoder;
    int             m_decodeEpoch = 0;
    int             m_durationMs  = 0;
    QVector<MinMax> m_envelope;
};

} // namespace soundshelf
