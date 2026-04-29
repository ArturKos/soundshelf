#pragma once

#include <QWidget>

namespace soundshelf {

/// Builder smart playlist (mockup soundshelf_smart_playlist)
/// TODO: implementacja — patrz mockup w docs/mockups/
class SmartPlaylistBuilder : public QWidget {
    Q_OBJECT
public:
    explicit SmartPlaylistBuilder(QWidget* parent = nullptr);
    ~SmartPlaylistBuilder() override;
};

} // namespace soundshelf
