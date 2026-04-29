#pragma once

#include <QWidget>

namespace soundshelf {

/// 10-band graphic EQ z presetami
/// TODO: implementacja — patrz mockup w docs/mockups/
class EqualizerWidget : public QWidget {
    Q_OBJECT
public:
    explicit EqualizerWidget(QWidget* parent = nullptr);
    ~EqualizerWidget() override;
};

} // namespace soundshelf
