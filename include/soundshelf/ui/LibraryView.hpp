#pragma once

#include <QWidget>

namespace soundshelf {

/// Główna lista utworów (QTableView + QSortFilterProxyModel)
/// TODO: implementacja — patrz mockup w docs/mockups/
class LibraryView : public QWidget {
    Q_OBJECT
public:
    explicit LibraryView(QWidget* parent = nullptr);
    ~LibraryView() override;
};

} // namespace soundshelf
