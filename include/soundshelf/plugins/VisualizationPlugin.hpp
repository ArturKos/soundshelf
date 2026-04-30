#pragma once

#include <QObject>
#include <QString>
#include <QPainter>
#include <QVector>

namespace soundshelf {

/**
 * @brief Abstract base class for SoundShelf visualisation plugins.
 *
 * The host paints onto a `QPaintDevice` (typically a `SpectrumWidget`'s
 * back buffer) and calls @ref render with the latest PCM frame and the
 * current paint area. The plugin owns no widgets — it only draws.
 *
 * Two flavours of plugin derive from this class:
 *  - @ref NativeVisPlugin — written in C++ against this API directly.
 *  - @ref WinampVisAdapter — wraps a Winamp `vis_*.dll` and forwards
 *    the PCM/spectrum frames into Winamp's classic vis API.
 *
 * Visualisations should be cheap — they run once per @c spectrumFps Hz
 * tick (default 30 Hz). Anything heavier than a few hundred QPainter
 * calls will drop frames.
 */
class VisualizationPlugin : public QObject {
    Q_OBJECT
public:
    explicit VisualizationPlugin(QObject* parent = nullptr);
    ~VisualizationPlugin() override;

    /// Human-readable plugin name (shown in the visualisation menu).
    virtual QString displayName() const = 0;

    /// Stable identifier (used for settings storage).
    virtual QString id() const = 0;

    /// Optional longer description shown in plugin settings.
    virtual QString description() const { return {}; }

    /// Called when the plugin is selected. May lazily allocate
    /// resources.
    virtual void start(int sampleRate, int channels) {
        m_sampleRate = sampleRate;
        m_channels   = channels;
    }

    /// Called when another plugin takes over.
    virtual void stop() {}

    /// Called once per visualisation tick. @p pcm is interleaved float
    /// in -1..1; @p spectrum is precomputed log-bin levels in 0..1.
    /// One of the two may be empty depending on the plugin's
    /// preferences (see @ref preferredFeed).
    virtual void render(QPainter& painter,
                        const QRectF& area,
                        const QVector<float>& pcm,
                        const QVector<float>& spectrum) = 0;

    /// What kind of feed the plugin wants. Affects what the host
    /// computes for the @ref render call (saves CPU when a plugin
    /// only wants one of the two).
    enum Feed { WantPcm, WantSpectrum, WantBoth };
    virtual Feed preferredFeed() const { return WantSpectrum; }

protected:
    int m_sampleRate = 44100;
    int m_channels   = 2;
};

} // namespace soundshelf
