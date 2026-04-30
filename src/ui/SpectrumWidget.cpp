#include "soundshelf/ui/SpectrumWidget.hpp"
#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/plugins/VisualizationPlugin.hpp"

#include <QPainter>

namespace soundshelf {

SpectrumWidget::SpectrumWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(96);
    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window, QColor(20, 20, 20));
    setPalette(p);

    m_tick.setInterval(33);  // ~30 Hz
    connect(&m_tick, &QTimer::timeout, this,
            QOverload<>::of(&SpectrumWidget::update));
    m_tick.start();
}

SpectrumWidget::~SpectrumWidget() {
    if (m_plugin) m_plugin->stop();
}

void SpectrumWidget::setFps(int fps) {
    m_tick.setInterval(qMax(10, 1000 / qMax(1, fps)));
}

void SpectrumWidget::attachEngine(PlayerEngine* engine) {
    m_engine = engine;
}

void SpectrumWidget::setActivePlugin(VisualizationPlugin* plugin) {
    if (m_plugin == plugin) return;
    if (m_plugin) m_plugin->stop();
    m_plugin = plugin;
    if (m_plugin) m_plugin->start(44100, 2);
    update();
}

void SpectrumWidget::paintEvent(QPaintEvent* /*ev*/) {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    if (!m_plugin || !m_engine) return;

    m_lastSpectrum = m_engine->spectrumData(48);
    QVector<float> pcm;  // empty for now — plugin can opt out via Feed
    m_plugin->render(p, QRectF(rect()), pcm, m_lastSpectrum);
}

} // namespace soundshelf
