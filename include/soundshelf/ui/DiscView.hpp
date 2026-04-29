#pragma once

#include <QWidget>

namespace soundshelf {

/// Widok grid płyt (mockup soundshelf_albums_discs_view)
/// TODO: implementacja — patrz mockup w docs/mockups/
class DiscView : public QWidget {
    Q_OBJECT
public:
    explicit DiscView(QWidget* parent = nullptr);
    ~DiscView() override;
};

} // namespace soundshelf
