#include "soundshelf/ui/PlayerWidget.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QFileInfo>
#include <QSignalBlocker>
#include "soundshelf/ui/SpectrumWidget.hpp"
#include "soundshelf/plugins/WaveformOverviewPlugin.hpp"

namespace soundshelf {

PlayerWidget::PlayerWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(72);

    m_coverLabel = new QLabel(this);
    m_coverLabel->setFixedSize(56, 56);
    m_coverLabel->setStyleSheet(QStringLiteral("background:#222; border:1px solid #444;"));

    m_titleLabel = new QLabel(tr("(nothing playing)"), this);
    QFont f = m_titleLabel->font();
    f.setBold(true);
    m_titleLabel->setFont(f);
    m_artistLabel = new QLabel(QString(), this);

    auto* meta = new QVBoxLayout;
    meta->setContentsMargins(8, 4, 8, 4);
    meta->addWidget(m_titleLabel);
    meta->addWidget(m_artistLabel);

    m_prevBtn = new QPushButton(tr("⏮"), this);
    m_playBtn = new QPushButton(tr("▶"),  this);
    m_nextBtn = new QPushButton(tr("⏭"), this);
    for (auto* b : { m_prevBtn, m_playBtn, m_nextBtn }) b->setFixedWidth(36);

    // Interactive waveform seek bar (replaces the plain slider): shows the
    // track's amplitude envelope, a playback cursor, and seeks on click/drag.
    m_waveSeek = new SpectrumWidget(this);
    m_waveSeek->setMinimumHeight(40);
    m_waveSeek->setMaximumHeight(56);

    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), this);

    m_volume = new QSlider(Qt::Horizontal, this);
    m_volume->setRange(0, 100);
    m_volume->setValue(80);
    m_volume->setFixedWidth(120);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 4);
    root->addWidget(m_coverLabel);
    root->addLayout(meta, 1);
    root->addWidget(m_prevBtn);
    root->addWidget(m_playBtn);
    root->addWidget(m_nextBtn);
    root->addWidget(m_waveSeek, 2);
    root->addWidget(m_timeLabel);
    root->addWidget(new QLabel(QStringLiteral("🔊"), this));
    root->addWidget(m_volume);

    connect(m_prevBtn, &QPushButton::clicked, this, &PlayerWidget::requestPrev);
    connect(m_nextBtn, &QPushButton::clicked, this, &PlayerWidget::requestNext);
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        if (!m_engine) return;
        if (m_engine->state() == PlayerState::Playing) m_engine->pause();
        else                                            m_engine->resume();
    });
    // Seeking is handled by the waveform widget itself (click/drag → seekMs).
    connect(m_volume, &QSlider::valueChanged, this, [this](int v) {
        if (m_engine) m_engine->setVolume(v);
    });
}

PlayerWidget::~PlayerWidget() = default;

QString PlayerWidget::fmtTime(int ms) {
    if (ms <= 0) return QStringLiteral("0:00");
    const int s = ms / 1000;
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

void PlayerWidget::attachEngine(PlayerEngine* engine) {
    m_engine = engine;
    if (!engine) return;
    // Drive the waveform seek bar from the engine: it renders the cursor at the
    // current position and seeks on click/drag via WaveformOverviewPlugin.
    m_waveSeek->attachEngine(engine);
    auto* wave = new WaveformOverviewPlugin(this);
    wave->setEngine(engine);
    m_waveSeek->setActivePlugin(wave);
    connect(engine, &PlayerEngine::stateChanged,    this, &PlayerWidget::onState);
    connect(engine, &PlayerEngine::positionChanged, this, &PlayerWidget::onPosition);
    connect(engine, &PlayerEngine::durationChanged, this, &PlayerWidget::onDuration);
    connect(engine, &PlayerEngine::trackChanged,    this, &PlayerWidget::onTrack);
    connect(engine, &PlayerEngine::volumeChanged,   this, &PlayerWidget::onVolume);
}

void PlayerWidget::onState(PlayerState state) {
    m_playBtn->setText(state == PlayerState::Playing
        ? QStringLiteral("⏸") : QStringLiteral("▶"));
}

void PlayerWidget::onPosition(int posMs) {
    // The waveform cursor tracks the engine position itself; we only keep the
    // numeric time label in sync here.
    m_timeLabel->setText(QStringLiteral("%1 / %2")
        .arg(fmtTime(posMs), fmtTime(m_durationMs)));
}

void PlayerWidget::onDuration(int durMs) {
    m_durationMs = durMs;
    m_timeLabel->setText(QStringLiteral("%1 / %2")
        .arg(fmtTime(0), fmtTime(durMs)));
}

void PlayerWidget::onTrack(const Track& t) {
    m_titleLabel->setText(t.title.isEmpty()
        ? QFileInfo(t.filepath).completeBaseName() : t.title);
    m_artistLabel->setText(t.artist);
}

void PlayerWidget::onVolume(double pct) {
    QSignalBlocker b(m_volume);
    m_volume->setValue(static_cast<int>(pct));
}

} // namespace soundshelf
