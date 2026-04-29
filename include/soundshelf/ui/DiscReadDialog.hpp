#pragma once

#include <QWidget>

namespace soundshelf {

/// Dialog 'Read disc from drive' (mockup soundshelf_read_disc_dialog)
/// TODO: implementacja — patrz mockup w docs/mockups/
class DiscReadDialog : public QWidget {
    Q_OBJECT
public:
    explicit DiscReadDialog(QWidget* parent = nullptr);
    ~DiscReadDialog() override;
};

} // namespace soundshelf
