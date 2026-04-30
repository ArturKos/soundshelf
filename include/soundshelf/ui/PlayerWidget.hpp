#pragma once

#include <QWidget>
#include "soundshelf/core/Track.hpp"
#include "soundshelf/core/PlayerEngine.hpp"

class QLabel;
class QPushButton;
class QSlider;

namespace soundshelf {

/**
 * @brief Bottom transport bar — play / pause / next / prev / seek / volume.
 *
 * The widget is purely a view: it never reaches into the audio
 * backend itself. It accepts a @ref PlayerEngine via @ref attachEngine
 * and binds the engine's signals to the relevant labels and sliders.
 * User interactions are forwarded back to the engine.
 *
 * Layout matches the mockup in `docs/mockups/`: cover thumbnail,
 * title + artist, transport controls in the middle, time / volume
 * on the right.
 */
class PlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlayerWidget(QWidget* parent = nullptr);
    ~PlayerWidget() override;

    /// Wires the engine — signals to UI, UI events to engine.
    void attachEngine(PlayerEngine* engine);

signals:
    void requestNext();
    void requestPrev();

private slots:
    void onState(PlayerState state);
    void onPosition(int posMs);
    void onDuration(int durMs);
    void onTrack(const Track& t);
    void onVolume(double pct);

private:
    static QString fmtTime(int ms);

    PlayerEngine* m_engine = nullptr;
    QLabel*       m_coverLabel = nullptr;
    QLabel*       m_titleLabel = nullptr;
    QLabel*       m_artistLabel = nullptr;
    QLabel*       m_timeLabel = nullptr;
    QPushButton*  m_prevBtn = nullptr;
    QPushButton*  m_playBtn = nullptr;
    QPushButton*  m_nextBtn = nullptr;
    QSlider*      m_seek = nullptr;
    QSlider*      m_volume = nullptr;
    int           m_durationMs = 0;
};

} // namespace soundshelf
