#pragma once

#include <QWidget>
#include <QVector>
#include <QTimer>

namespace soundshelf {

class PlayerEngine;
class VisualizationPlugin;

/**
 * @brief Hosts the active visualisation plugin and drives its tick.
 *
 * Owns a 30 Hz `QTimer`. On each tick the widget asks
 * @ref PlayerEngine for the latest spectrum data, then forwards it to
 * the active @ref VisualizationPlugin's `render` method via the
 * widget's `paintEvent`.
 *
 * The active plugin can be swapped at runtime via @ref setActivePlugin —
 * the widget calls `stop()` on the previous plugin and `start()` on
 * the new one with the engine's sample rate / channel count.
 */
class SpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget* parent = nullptr);
    ~SpectrumWidget() override;

    void attachEngine(PlayerEngine* engine);
    void setActivePlugin(VisualizationPlugin* plugin);
    VisualizationPlugin* activePlugin() const { return m_plugin; }

    /// Tick rate (Hz). Defaults to 30.
    void setFps(int fps);

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    PlayerEngine*        m_engine = nullptr;
    VisualizationPlugin* m_plugin = nullptr;
    QTimer               m_tick;
    QVector<float>       m_lastSpectrum;
};

} // namespace soundshelf
