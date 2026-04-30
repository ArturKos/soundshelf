#include "soundshelf/plugins/NativeVisPlugin.hpp"

#include <QPainter>
#include <QLinearGradient>

namespace soundshelf {

NativeVisPlugin::NativeVisPlugin(QObject* parent) : VisualizationPlugin(parent) {}
NativeVisPlugin::~NativeVisPlugin() = default;

void NativeVisPlugin::start(int sampleRate, int channels) {
    VisualizationPlugin::start(sampleRate, channels);
    m_peaks.clear();
}

void NativeVisPlugin::stop() {
    m_peaks.clear();
}

void NativeVisPlugin::render(QPainter& painter,
                             const QRectF& area,
                             const QVector<float>& /*pcm*/,
                             const QVector<float>& spectrum) {
    if (spectrum.isEmpty() || area.width() <= 0 || area.height() <= 0) return;

    if (m_peaks.size() != spectrum.size()) {
        m_peaks.resize(spectrum.size());
        m_peaks.fill(0.0f);
    }

    const int n = spectrum.size();
    const qreal barWidth = area.width() / n;
    const qreal gap = barWidth > 4 ? 1.0 : 0.0;

    QLinearGradient grad(area.bottomLeft(), area.topLeft());
    grad.setColorAt(0.0, QColor(0,  220, 80));
    grad.setColorAt(0.7, QColor(220, 200, 60));
    grad.setColorAt(1.0, QColor(220, 60, 60));
    painter.setBrush(grad);
    painter.setPen(Qt::NoPen);

    for (int i = 0; i < n; ++i) {
        const float v = qBound(0.0f, spectrum[i], 1.0f);
        if (v > m_peaks[i]) m_peaks[i] = v;
        else                m_peaks[i] = qMax(0.0f, m_peaks[i] - 0.01f); // decay

        const qreal x = area.left() + i * barWidth;
        const qreal h = area.height() * v;
        painter.drawRect(QRectF(x + gap, area.bottom() - h, barWidth - gap * 2, h));

        // peak indicator
        const qreal py = area.bottom() - area.height() * m_peaks[i];
        painter.fillRect(QRectF(x + gap, py - 1, barWidth - gap * 2, 2),
                         QColor(255, 255, 255, 200));
    }
}

} // namespace soundshelf
