#pragma once

#include <QWidget>

namespace soundshelf {

/// Pasek odtwarzacza na dole głównego okna
/// TODO: implementacja — patrz mockup w docs/mockups/
class PlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlayerWidget(QWidget* parent = nullptr);
    ~PlayerWidget() override;
};

} // namespace soundshelf
