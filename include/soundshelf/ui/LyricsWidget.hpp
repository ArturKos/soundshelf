#pragma once

#include <QWidget>
#include <QString>
#include <QList>

class QPlainTextEdit;
class QLabel;

namespace soundshelf {

/**
 * @brief Displays plain or synchronised LRC lyrics.
 *
 * For plain lyrics the widget is a scrollable read-only text view;
 * for synced lyrics (LRC with `[mm:ss.cc]` timestamps) calling
 * @ref setPositionMs scrolls to and highlights the active line.
 *
 * The widget itself does not fetch — feed it via @ref setLyrics after
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
    void rebuildSyncedIndex();
    int  lineForMs(int ms) const;

    QLabel*         m_status = nullptr;
    QPlainTextEdit* m_text = nullptr;
    QString         m_plain;
    QString         m_synced;
    QList<int>      m_syncedTimings;  ///< parallel to text lines (ms)
    int             m_currentLine = -1;
};

} // namespace soundshelf
