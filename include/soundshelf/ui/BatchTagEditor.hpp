#pragma once

#include <QWidget>

namespace soundshelf {

/// Masowy edytor tagów (mockup soundshelf_batch_tag_dupes)
/// TODO: implementacja — patrz mockup w docs/mockups/
class BatchTagEditor : public QWidget {
    Q_OBJECT
public:
    explicit BatchTagEditor(QWidget* parent = nullptr);
    ~BatchTagEditor() override;
};

} // namespace soundshelf
