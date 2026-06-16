#pragma once

#include <QWidget>
#include <QString>
#include "soundshelf/io/LrcParser.hpp"

class QPlainTextEdit;
class QLabel;

namespace soundshelf {

/**
 * @brief Displays plain or synchronised LRC lyrics.
 *
 * For plain lyrics the widget is a scrollable read-only text view.
 * For synced lyrics (LRC with `[mm:ss.cc]` timestamps), calling
 * @ref setPositionMs scrolls to and highlights the active line.
 * LRC parsing is delegated to @ref LrcParser (I/O layer).
 *
 * The widget does not fetch lyrics — feed it via @ref setLyrics after
 * a successful @ref LyricsClient lookup.
 */
class LyricsWidget : public QWidget {
    Q_OBJECT
public:
    explicit LyricsWidget(QWidget* parent = nullptr);
    ~LyricsWidget() override;

    /// Replaces the displayed lyrics. Either argument may be empty;
    /// when both are empty the widget shows a placeholder.
    void setLyrics(const QString& plain, const QString& synced);

    /// Highlights the active line for the given playback position.
    /// Has no effect when only plain lyrics are loaded.
    void setPositionMs(int ms);

private:
    QLabel*         m_status = nullptr;
    QPlainTextEdit* m_text = nullptr;
    QString         m_plain;
    QString         m_synced;
    LrcDocument     m_doc;           ///< Parsed synced-lyrics document (empty when plain-only).
    int             m_currentLine = -1;
};

} // namespace soundshelf
