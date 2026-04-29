#pragma once

#include <QWidget>

namespace soundshelf {

/// QSystemTrayIcon z menu kontekstowym
/// TODO: implementacja — patrz mockup w docs/mockups/
class TrayIcon : public QWidget {
    Q_OBJECT
public:
    explicit TrayIcon(QWidget* parent = nullptr);
    ~TrayIcon() override;
};

} // namespace soundshelf
