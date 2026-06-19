#include "soundshelf/ui/SpectrumWidget.hpp"
#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/plugins/VisualizationPlugin.hpp"
#include "soundshelf/plugins/WaveformOverviewPlugin.hpp"
#include "soundshelf/plugins/VisualStyle.hpp"

#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QColorDialog>

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
    const VisualStyle st = currentVisualStyle();
    p.setPen(Qt::NoPen);

    for (int i = 0; i < n; ++i) {
        const qreal level = qBound(0.0f, m_lastSpectrum[i], 1.0f);
        const qreal barH = level * (h - 2);
        const QRectF bar(i * w + 1, h - barH, w - 2, barH);
        // Per-bar colour from the active style (rainbow = hue across the
        // spectrum), with a vertical brightness gradient: dim base → bright tip.
        const QColor top = visColor(st, qreal(i) / n, 1.0);
        const QColor base = top.darker(220);
        QLinearGradient grad(0, h, 0, h - barH);
        grad.setColorAt(0.0, base);
        grad.setColorAt(1.0, top);
        p.setBrush(grad);
        p.drawRect(bar);
        if (visWantsGlow(st)) {                 // Neon: soft glow tip
            QColor glow = top; glow.setAlpha(60);
            p.setBrush(glow);
            p.drawRect(QRectF(i * w, h - barH - 3, w, 4));
        }
    }
}

void SpectrumWidget::contextMenuEvent(QContextMenuEvent* ev) {
    QMenu menu(this);
    auto add = [&](const QString& label, VisScheme scheme) {
        QAction* a = menu.addAction(label);
        a->setCheckable(true);
        a->setChecked(currentVisualStyle().scheme == scheme);
        connect(a, &QAction::triggered, this, [scheme]() {
            VisualStyle s = currentVisualStyle();
            s.scheme = scheme;
            setCurrentVisualStyle(s);
        });
    };
    add(tr("Rainbow"),        VisScheme::Rainbow);
    add(tr("Phosphor green"), VisScheme::Phosphor);
    add(tr("Amber"),          VisScheme::Amber);
    add(tr("Ice blue"),       VisScheme::Ice);
    add(tr("Neon (accent)"),  VisScheme::Neon);
    menu.addSeparator();
    QAction* pick = menu.addAction(tr("Choose accent colour…"));
    connect(pick, &QAction::triggered, this, [this]() {
        const QColor c = QColorDialog::getColor(currentVisualStyle().accent, this,
                                                tr("Visualisation accent colour"));
        if (c.isValid()) {
            VisualStyle s = currentVisualStyle();
            s.accent = c;
            s.scheme = VisScheme::Neon;   // accent schemes show the picked colour
            setCurrentVisualStyle(s);
        }
    });
    menu.exec(ev->globalPos());
    update();
}

} // namespace soundshelf
