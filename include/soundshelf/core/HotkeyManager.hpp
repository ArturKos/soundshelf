#pragma once

#include <QObject>
#include <QString>


namespace soundshelf {

/// Globalne skróty klawiszowe (media keys etc.)
/// TODO: implementacja — patrz CLAUDE.md
class HotkeyManager : public QObject {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    // TODO: API
};

} // namespace soundshelf
