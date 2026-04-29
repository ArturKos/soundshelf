#pragma once

#include <QWidget>

namespace soundshelf {

/// Okno ustawień (mockup soundshelf_preferences)
/// TODO: implementacja — patrz mockup w docs/mockups/
class PreferencesDialog : public QWidget {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;
};

} // namespace soundshelf
