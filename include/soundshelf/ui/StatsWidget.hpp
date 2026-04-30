#pragma once

#include <QWidget>
#include <QList>

class QLabel;
class QListWidget;

namespace soundshelf {

/**
 * @brief Listening statistics — total time, top tracks, weekday bars.
 *
 * Pulls data from @ref PlayHistory and renders three blocks:
 *  1. summary line (total played, distinct tracks, ...)
 *  2. top tracks list
 *  3. weekday bars (drawn in `paintEvent`)
 *
 * Re-fetches everything when @ref refresh is called — typically wired
 * from the main window when the user opens the Stats tab.
 */
class StatsWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatsWidget(QWidget* parent = nullptr);
    ~StatsWidget() override;

    /// Forces a re-read from PlayHistory.
    void refresh();

    /// Window in days for the top-tracks query (0 = all time).
    void setWindowDays(int days) { m_windowDays = days; }

protected:
    void paintEvent(QPaintEvent* ev) override;

private:
    QLabel*      m_summary = nullptr;
    QListWidget* m_topTracks = nullptr;
    QList<int>   m_weekdayBars;
    int          m_windowDays = 30;
};

} // namespace soundshelf
