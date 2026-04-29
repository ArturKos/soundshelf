#pragma once

#include <QWidget>

namespace soundshelf {

/// Wyświetlanie lyrics z synchronizacją
/// TODO: implementacja — patrz mockup w docs/mockups/
class LyricsWidget : public QWidget {
    Q_OBJECT
public:
    explicit LyricsWidget(QWidget* parent = nullptr);
    ~LyricsWidget() override;
};

} // namespace soundshelf
