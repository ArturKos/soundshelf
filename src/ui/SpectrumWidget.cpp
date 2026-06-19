#include "soundshelf/ui/SpectrumWidget.hpp"
#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/plugins/VisualizationPlugin.hpp"
#include "soundshelf/plugins/WaveformOverviewPlugin.hpp"

#include <QPainter>
#include <QMouseEvent>

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
    if (!m_engine) return;

    const int bars = qMax(8, width() / 8);
    m_lastSpectrum = m_engine->spectrumData(bars);

    // An active Winamp/native plugin takes over the whole surface.
    if (m_plugin) {
        // Pass the live PCM frame so WantPcm plugins (e.g. OscilloscopePlugin) work.
        const QVector<float> pcm = m_engine->visualizationPcm();
        m_plugin->render(p, QRectF(rect()), pcm, m_lastSpectrum);
        return;
    }

    drawBuiltinBars(p);
}

void SpectrumWidget::handleWaveformSeek(double x) {
    auto* wf = qobject_cast<WaveformOverviewPlugin*>(m_plugin);
    if (wf)
        wf->handleClick(x, QRectF(rect()));
}

void SpectrumWidget::mousePressEvent(QMouseEvent* ev) {
    handleWaveformSeek(ev->position().x());
}

void SpectrumWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (ev->buttons() & Qt::LeftButton)
        handleWaveformSeek(ev->position().x());
}

void SpectrumWidget::drawBuiltinBars(QPainter& p) {
    const int n = m_lastSpectrum.size();
    if (n <= 0) return;

    const qreal w = qreal(width()) / n;
    const qreal h = height();

    // Retro phosphor-green gradient, brighter towards the top.
    QLinearGradient grad(0, h, 0, 0);
    grad.setColorAt(0.0, QColor(0, 120, 0));
    grad.setColorAt(0.6, QColor(0, 220, 60));
    grad.setColorAt(1.0, QColor(180, 255, 120));

    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    for (int i = 0; i < n; ++i) {
        const qreal level = qBound(0.0f, m_lastSpectrum[i], 1.0f);
        const qreal barH = level * (h - 2);
        const QRectF bar(i * w + 1, h - barH, w - 2, barH);
        p.drawRect(bar);
    }
}

} // namespace soundshelf
