#include "soundshelf/core/HotkeyManager.hpp"

#include <QShortcut>
#include <QWidget>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcHk, "soundshelf.core.hotkey")

namespace soundshelf {

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent) {
    // Defaults — match the table in CLAUDE.md.
    m_bindings = {
        { QStringLiteral("playPause"),       QKeySequence(Qt::Key_Space) },
        { QStringLiteral("next"),            QKeySequence(Qt::Key_Right) },
        { QStringLiteral("prev"),            QKeySequence(Qt::Key_Left) },
        { QStringLiteral("stop"),            QKeySequence(Qt::Key_Period) },
        { QStringLiteral("volumeUp"),        QKeySequence(Qt::Key_Up) },
        { QStringLiteral("volumeDown"),      QKeySequence(Qt::Key_Down) },
        { QStringLiteral("mute"),            QKeySequence(Qt::Key_M) },
        { QStringLiteral("seekForward"),     QKeySequence(Qt::CTRL | Qt::Key_Right) },
        { QStringLiteral("seekBackward"),    QKeySequence(Qt::CTRL | Qt::Key_Left) },
        { QStringLiteral("toggleMainWindow"),QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Space) },
    };
}

HotkeyManager::~HotkeyManager() = default;

Result<void> HotkeyManager::attachTo(QWidget* host) {
    if (!host) {
        return Result<void>::err(Error::InvalidArgument, QStringLiteral("null host"));
    }
    qDeleteAll(m_shortcuts);
    m_shortcuts.clear();
    m_host = host;

    for (auto it = m_bindings.constBegin(); it != m_bindings.constEnd(); ++it) {
        auto* sc = new QShortcut(it.value(), host);
        sc->setContext(Qt::ApplicationShortcut);
        const QString action = it.key();
        connect(sc, &QShortcut::activated, this, [this, action]() {
            qCDebug(lcHk) << "action" << action;
            emit actionTriggered(action);
        });
        m_shortcuts.insert(action, sc);
    }
    return Result<void>::ok();
}

Result<void> HotkeyManager::bind(const QString& action, const QKeySequence& sequence) {
    m_bindings.insert(action, sequence);
    if (m_host) {
        if (auto* old = m_shortcuts.value(action)) old->deleteLater();
        auto* sc = new QShortcut(sequence, m_host);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, this, [this, action]() {
            emit actionTriggered(action);
        });
        m_shortcuts.insert(action, sc);
    }
    return Result<void>::ok();
}

void HotkeyManager::unbind(const QString& action) {
    m_bindings.remove(action);
    if (auto* sc = m_shortcuts.take(action)) sc->deleteLater();
}

QKeySequence HotkeyManager::binding(const QString& action) const {
    return m_bindings.value(action);
}

QStringList HotkeyManager::actions() const {
    return m_bindings.keys();
}

} // namespace soundshelf
