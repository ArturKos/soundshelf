#pragma once

#include <QWidget>

namespace soundshelf {

/// Statystyki odsłuchów + heatmap
/// TODO: implementacja — patrz mockup w docs/mockups/
class StatsWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatsWidget(QWidget* parent = nullptr);
    ~StatsWidget() override;
};

} // namespace soundshelf
