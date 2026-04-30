#include "soundshelf/ui/StatsWidget.hpp"
#include "soundshelf/data/PlayHistory.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>

namespace soundshelf {

StatsWidget::StatsWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    m_summary = new QLabel(tr("Loading statistics…"), this);
    QFont f = m_summary->font();
    f.setBold(true);
    m_summary->setFont(f);
    root->addWidget(m_summary);

    root->addWidget(new QLabel(tr("Top tracks:"), this));
    m_topTracks = new QListWidget(this);
    root->addWidget(m_topTracks, 1);

    root->addWidget(new QLabel(tr("Listens per weekday:"), this));
    setMinimumHeight(360);
}

StatsWidget::~StatsWidget() = default;

void StatsWidget::refresh() {
    PlayHistory hist;
    auto top = hist.topTracks(15, m_windowDays);
    auto wk  = hist.playsPerWeekday(m_windowDays);
    auto tot = hist.totalPlayedMs(m_windowDays);

    m_topTracks->clear();
    if (top) {
        for (const auto& a : top.value()) {
            m_topTracks->addItem(QStringLiteral("%1 — %2 plays")
                .arg(a.label).arg(a.playCount));
        }
    }
    m_weekdayBars = wk ? wk.value() : QList<int>(7, 0);

    if (tot) {
        const qint64 ms = tot.value();
        m_summary->setText(tr("Total listened: %1 h %2 m (window: %3 days)")
            .arg(ms / 3600000)
            .arg((ms / 60000) % 60)
            .arg(m_windowDays));
    } else {
        m_summary->setText(tr("Cannot read statistics: %1").arg(tot.error().message));
    }
    update();
}

void StatsWidget::paintEvent(QPaintEvent* ev) {
    QWidget::paintEvent(ev);
    if (m_weekdayBars.isEmpty()) return;
    QPainter p(this);

    const int barsTop = height() - 70;
    const int barsBot = height() - 10;
    const int barsW   = width()  - 20;
    int maxV = 1;
    for (int v : m_weekdayBars) maxV = qMax(maxV, v);

    const QStringList days {
        QStringLiteral("Mon"), QStringLiteral("Tue"), QStringLiteral("Wed"),
        QStringLiteral("Thu"), QStringLiteral("Fri"), QStringLiteral("Sat"),
        QStringLiteral("Sun"),
    };
    const qreal slot = barsW / qreal(m_weekdayBars.size());
    for (int i = 0; i < m_weekdayBars.size(); ++i) {
        const qreal x = 10 + i * slot;
        const qreal h = (barsBot - barsTop) * (m_weekdayBars[i] / qreal(maxV));
        p.fillRect(QRectF(x + 4, barsBot - h, slot - 8, h),
                   QColor(80, 160, 220));
        p.drawText(QRectF(x, barsBot, slot, 12),
                   Qt::AlignHCenter, days.value(i));
    }
}

} // namespace soundshelf
