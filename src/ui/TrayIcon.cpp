#include "soundshelf/ui/TrayIcon.hpp"
#include "soundshelf/core/PlayerEngine.hpp"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QStyle>

namespace soundshelf {

TrayIcon::TrayIcon(QObject* parent) : QObject(parent) {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    m_tray->setToolTip(QStringLiteral("SoundShelf"));

    m_menu = new QMenu();
    m_playPauseAct = m_menu->addAction(tr("Play / Pause"));
    auto* nextAct  = m_menu->addAction(tr("Next"));
    auto* prevAct  = m_menu->addAction(tr("Previous"));
    auto* stopAct  = m_menu->addAction(tr("Stop"));
    m_menu->addSeparator();
    auto* showAct  = m_menu->addAction(tr("Show / Hide window"));
    m_menu->addSeparator();
    auto* quitAct  = m_menu->addAction(tr("Quit"));

    connect(showAct, &QAction::triggered, this, &TrayIcon::showMainWindowRequested);
    connect(quitAct, &QAction::triggered, this, &TrayIcon::quitRequested);

    connect(m_playPauseAct, &QAction::triggered, this, [this]() {
        if (!m_engine) return;
        if (m_engine->state() == PlayerState::Playing) m_engine->pause();
        else                                            m_engine->resume();
    });
    connect(stopAct, &QAction::triggered, this, [this]() {
        if (m_engine) m_engine->stop();
    });
    connect(nextAct, &QAction::triggered, this, &TrayIcon::showMainWindowRequested);
    connect(prevAct, &QAction::triggered, this, &TrayIcon::showMainWindowRequested);

    m_tray->setContextMenu(m_menu);
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) emit showMainWindowRequested();
    });
}

TrayIcon::~TrayIcon() {
    delete m_menu;
}

bool TrayIcon::isAvailable() {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::attachEngine(PlayerEngine* engine) {
    m_engine = engine;
    if (!engine) return;
    connect(engine, &PlayerEngine::trackChanged, this, [this](const Track& t) {
        setNowPlaying(t.title, t.artist);
    });
}

void TrayIcon::setNowPlaying(const QString& title, const QString& artist) {
    if (!m_tray) return;
    const QString tip = artist.isEmpty()
        ? title : QStringLiteral("%1 — %2").arg(artist, title);
    m_tray->setToolTip(tip.isEmpty() ? QStringLiteral("SoundShelf") : tip);
}

} // namespace soundshelf
