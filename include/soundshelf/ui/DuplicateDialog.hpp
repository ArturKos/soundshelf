#pragma once

#include <QWidget>

namespace soundshelf {

/// Lista duplikatów z opcją resolve
/// TODO: implementacja — patrz mockup w docs/mockups/
class DuplicateDialog : public QWidget {
    Q_OBJECT
public:
    explicit DuplicateDialog(QWidget* parent = nullptr);
    ~DuplicateDialog() override;
};

} // namespace soundshelf
