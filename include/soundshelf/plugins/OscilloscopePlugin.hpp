#pragma once

#include <QPolygonF>
#include <QRectF>
#include <QVector>

#include "soundshelf/plugins/VisualizationPlugin.hpp"

namespace soundshelf {

/**
 * @brief Oscilloscope visualisation — draws a phosphor-green waveform from live PCM.
 *
 * Renders a single continuous line tracing the raw waveform amplitude across
 * the current PCM frame. When fewer than 2 samples are available the plugin
 * draws a flat horizontal centre line so the display is never blank.
 *
 * This is a purely @c WantPcm plugin; spectrum data is ignored.
 */
class OscilloscopePlugin : public VisualizationPlugin {
    Q_OBJECT
public:
    explicit OscilloscopePlugin(QObject* parent = nullptr);
    ~OscilloscopePlugin() override;

    /// @return Human-readable plugin name for display in the visualisation menu.
    QString displayName() const override { return QStringLiteral("Oscilloscope"); }
    /// @return Stable identifier used for settings storage.
    QString id()          const override { return QStringLiteral("soundshelf.native.scope"); }
    /// @return Longer description shown in plugin settings.
    QString description() const override {
        return QStringLiteral("Phosphor-green waveform oscilloscope — shows raw PCM amplitude.");
    }

    Feed preferredFeed() const override { return WantPcm; }

    /**
     * @brief Draws the waveform (or a flat idle line) into @p area.
     *
     * @param painter  Active painter on the SpectrumWidget's back-buffer.
     * @param area     Target paint rectangle.
     * @param pcm      Mono float samples in [−1, 1] from the current PCM frame.
     * @param spectrum Ignored — this plugin uses PCM only.
     */
    void render(QPainter& painter,
                const QRectF& area,
                const QVector<float>& pcm,
                const QVector<float>& spectrum) override;

    /**
     * @brief Builds a QPolygonF polyline from a PCM frame.
     *
     * Pure static function — no Qt widget or engine dependency; safe to call
     * directly from unit tests.
     *
     * For @p n = @c pcm.size() samples the mapping is:
     *   @code
     *   x[i] = area.left() + i * area.width() / (n - 1)
     *   y[i] = area.center().y() - clamp(pcm[i], -1, 1) * area.height() / 2
     *   @endcode
     *
     * @param pcm  Mono float samples in [−1, 1].
     * @param area Target paint area.
     * @return Polygon with one point per sample, or an empty polygon when
     *         @p pcm has fewer than 2 elements.
     */
    static QPolygonF buildPolyline(const QVector<float>& pcm, const QRectF& area);
};

} // namespace soundshelf
