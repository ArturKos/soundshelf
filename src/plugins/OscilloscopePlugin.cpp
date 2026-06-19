#include "soundshelf/plugins/OscilloscopePlugin.hpp"
#include "soundshelf/plugins/VisualStyle.hpp"

#include <QPainter>
#include <QLinearGradient>
#include <QLoggingCategory>
#include <algorithm>

Q_LOGGING_CATEGORY(lcScope, "soundshelf.plugins.vis")

namespace soundshelf {

OscilloscopePlugin::OscilloscopePlugin(QObject* parent) : VisualizationPlugin(parent) {}
OscilloscopePlugin::~OscilloscopePlugin() = default;

QPolygonF OscilloscopePlugin::buildPolyline(const QVector<float>& pcm, const QRectF& area)
{
    const int n = pcm.size();
    if (n < 2) return {};

    QPolygonF poly;
    poly.reserve(n);

    const qreal cy     = area.center().y();
    const qreal halfH  = area.height() / 2.0;
    const qreal xScale = area.width() / static_cast<qreal>(n - 1);

    for (int i = 0; i < n; ++i) {
        const float s = std::clamp(pcm[i], -1.0f, 1.0f);
        const qreal x = area.left() + i * xScale;
        const qreal y = cy - static_cast<qreal>(s) * halfH;
        poly.append(QPointF(x, y));
    }
    return poly;
}

void OscilloscopePlugin::render(QPainter& painter,
                                const QRectF& area,
                                const QVector<float>& pcm,
                                const QVector<float>& /*spectrum*/)
{
    if (area.width() <= 0 || area.height() <= 0)
        return;

    painter.setRenderHint(QPainter::Antialiasing);

    // Horizontal colour gradient across the trace from the active style.
    const VisualStyle st = currentVisualStyle();
    QLinearGradient grad(area.left(), 0, area.right(), 0);
    for (int i = 0; i <= 6; ++i)
        grad.setColorAt(i / 6.0, visColor(st, i / 6.0, 1.0));
    QPen pen(QBrush(grad), 1.6);

    if (pcm.size() < 2) {
        // Flat centre line when idle or no PCM available
        const qreal cy = area.center().y();
        painter.setPen(pen);
        painter.drawLine(QPointF(area.left(), cy), QPointF(area.right(), cy));
        qCDebug(lcScope) << "Idle oscilloscope (no PCM frame)";
        return;
    }

    const QPolygonF poly = buildPolyline(pcm, area);
    if (poly.isEmpty()) return;

    // Neon: a soft, thick translucent underlay gives a glow halo.
    if (visWantsGlow(st)) {
        QColor glow = visEffectiveAccent(st);
        glow.setAlpha(70);
        QPen glowPen(glow, 5.0);
        glowPen.setCapStyle(Qt::RoundCap);
        painter.setPen(glowPen);
        painter.drawPolyline(poly);
    }
    painter.setPen(pen);
    painter.drawPolyline(poly);
}

} // namespace soundshelf
