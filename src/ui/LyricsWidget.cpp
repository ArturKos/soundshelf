#include "soundshelf/ui/LyricsWidget.hpp"
#include "soundshelf/io/LrcParser.hpp"

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
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
    m_doc = LrcDocument{};

    if (m_synced.isEmpty() && m_plain.isEmpty()) {
        m_text->clear();
        m_status->setText(tr("No lyrics found."));
        return;
    }

    if (!m_synced.isEmpty()) {
        m_doc = LrcParser::parse(m_synced);
        QString displayText;
        displayText.reserve(m_synced.size());
        for (const LrcLine& line : m_doc.lines) {
            displayText += line.text;
            displayText += QLatin1Char('\n');
        }
        m_text->setPlainText(displayText);
        m_status->setText(tr("Synced lyrics (LRC)"));
    } else {
        m_text->setPlainText(m_plain);
        m_status->setText(tr("Plain lyrics"));
    }
}

void LyricsWidget::setPositionMs(int ms) {
    if (!m_doc.hasTimedLines()) return;
    const int line = LrcParser::lineIndexForMs(m_doc, ms);
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
