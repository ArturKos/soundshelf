#pragma once

#include <QString>
#include "soundshelf/plugins/VisualizationPlugin.hpp"

namespace soundshelf {

/**
 * @brief Built-in spectrum-bar visualisation.
 *
 * The default visualisation that ships with SoundShelf — vertical
 * bars with a falling-peaks decay, painted in the active theme's
 * accent colour. Cheap to draw and works on every platform.
 *
 * Acts as a reference implementation for native (in-process)
 * plugins. Future native plugins (waveform, oscilloscope, retro
 * "vis tower"...) follow the same pattern.
 */
class NativeVisPlugin : public VisualizationPlugin {
    Q_OBJECT
public:
    explicit NativeVisPlugin(QObject* parent = nullptr);
    ~NativeVisPlugin() override;

    QString displayName() const override { return QStringLiteral("Spectrum bars"); }
    QString id() const override { return QStringLiteral("soundshelf.native.bars"); }
    QString description() const override {
        return QStringLiteral("Classic logarithmic frequency-band bars with peak decay.");
    }

    Feed preferredFeed() const override { return WantSpectrum; }

    void start(int sampleRate, int channels) override;
    void stop() override;
    void render(QPainter& painter,
                const QRectF& area,
                const QVector<float>& pcm,
                const QVector<float>& spectrum) override;

private:
    QVector<float> m_peaks;        ///< per-bar peak (falls between frames)
};

} // namespace soundshelf
