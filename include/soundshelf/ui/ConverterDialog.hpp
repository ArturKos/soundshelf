#pragma once

#include <QWidget>

namespace soundshelf {

/// Konwersja formatów (FFmpeg wrapper)
/// TODO: implementacja — patrz mockup w docs/mockups/
class ConverterDialog : public QWidget {
    Q_OBJECT
public:
    explicit ConverterDialog(QWidget* parent = nullptr);
    ~ConverterDialog() override;
};

} // namespace soundshelf
