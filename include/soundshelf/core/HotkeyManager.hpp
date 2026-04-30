#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QKeySequence>
#include "soundshelf/core/Result.hpp"

class QShortcut;
class QWidget;

namespace soundshelf {

/**
 * @brief Manages in-app media hotkeys and Qt::ApplicationShortcut bindings.
 *
 * Uses `QShortcut` with `Qt::ApplicationShortcut`, which captures keys
 * whenever any of SoundShelf's windows have focus. True system-wide
 * media-key capture is delegated to platform integrations:
 *  - Linux: @ref MprisAdapter (KDE/GNOME forward media keys to MPRIS)
 *  - Windows: SMTC (System Media Transport Controls) — TODO
 *
 * Logical action names: `playPause`, `next`, `prev`, `stop`,
 * `volumeUp`, `volumeDown`, `mute`, `seekForward`, `seekBackward`,
 * `toggleMainWindow`.
 */
class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    /// Attaches the in-app shortcuts to @p host. Required once at startup.
    Result<void> attachTo(QWidget* host);

    /// Binds @p sequence to the named action; replaces any prior binding.
    Result<void> bind(const QString& action, const QKeySequence& sequence);

    /// Removes a previously bound action.
    void unbind(const QString& action);

    /// Returns the current binding for @p action, or an invalid sequence.
    QKeySequence binding(const QString& action) const;

    /// All bound action names.
    QStringList actions() const;

signals:
    /// Emitted when the user activates an action via its hotkey.
    void actionTriggered(const QString& action);

private:
    QWidget*                     m_host = nullptr;
    QHash<QString, QShortcut*>   m_shortcuts;
    QHash<QString, QKeySequence> m_bindings;
};

} // namespace soundshelf
