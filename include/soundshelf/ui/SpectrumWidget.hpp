#pragma once

#include <QWidget>

namespace soundshelf {

/// Wizualizator spektrum FFT (jak Winamp)
/// TODO: implementacja — patrz mockup w docs/mockups/
class SpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget* parent = nullptr);
    ~SpectrumWidget() override;
};

} // namespace soundshelf
