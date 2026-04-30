#include "soundshelf/ui/LyricsWidget.hpp"

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace soundshelf {

LyricsWidget::LyricsWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_status = new QLabel(tr("No lyrics loaded."), this);
    m_status->setStyleSheet(QStringLiteral("color:#888; padding:2px;"));
    root->addWidget(m_status);

    m_text = new QPlainTextEdit(this);
    m_text->setReadOnly(true);
    root->addWidget(m_text, 1);
}

LyricsWidget::~LyricsWidget() = default;

void LyricsWidget::setLyrics(const QString& plain, const QString& synced) {
    m_plain  = plain;
    m_synced = synced;
    m_currentLine = -1;
    rebuildSyncedIndex();

    if (m_synced.isEmpty() && m_plain.isEmpty()) {
        m_text->clear();
        m_status->setText(tr("No lyrics found."));
        return;
    }
    if (!m_synced.isEmpty()) {
        static const QRegularExpression re(
            QStringLiteral("^\\[\\d+:\\d+(?:\\.\\d+)?\\]\\s*"));
        QString stripped;
        for (QString line : m_synced.split(QLatin1Char('\n'))) {
            line.remove(re);
            stripped += line;
            stripped += QLatin1Char('\n');
        }
        m_text->setPlainText(stripped);
        m_status->setText(tr("Synced lyrics (LRC)"));
    } else {
        m_text->setPlainText(m_plain);
        m_status->setText(tr("Plain lyrics"));
    }
}

void LyricsWidget::rebuildSyncedIndex() {
    m_syncedTimings.clear();
    if (m_synced.isEmpty()) return;

    static const QRegularExpression re(
        QStringLiteral("^\\[(\\d+):(\\d+)(?:\\.(\\d+))?\\]"));
    for (const QString& line : m_synced.split(QLatin1Char('\n'))) {
        const auto m = re.match(line);
        if (m.hasMatch()) {
            const int min = m.captured(1).toInt();
            const int sec = m.captured(2).toInt();
            const int cs  = m.captured(3).leftJustified(2, QLatin1Char('0'), true).toInt();
            m_syncedTimings.append(((min * 60) + sec) * 1000 + cs * 10);
        } else {
            m_syncedTimings.append(-1);
        }
    }
}

int LyricsWidget::lineForMs(int ms) const {
    int best = -1;
    for (int i = 0; i < m_syncedTimings.size(); ++i) {
        const int t = m_syncedTimings[i];
        if (t >= 0 && t <= ms) best = i;
    }
    return best;
}

void LyricsWidget::setPositionMs(int ms) {
    if (m_syncedTimings.isEmpty()) return;
    const int line = lineForMs(ms);
    if (line == m_currentLine) return;
    m_currentLine = line;
    if (line < 0) return;

    QTextCursor c(m_text->document()->findBlockByNumber(line));
    if (c.isNull()) return;
    c.select(QTextCursor::LineUnderCursor);
    m_text->setTextCursor(c);
    m_text->ensureCursorVisible();
}

} // namespace soundshelf
