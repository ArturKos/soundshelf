#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf {

/// Czytanie obrazów płyty (CUE/BIN/ISO)
/// TODO: implementacja — patrz CLAUDE.md
class ImageReader : public QObject {
    Q_OBJECT
public:
    explicit ImageReader(QObject* parent = nullptr);
    ~ImageReader() override;

    // TODO: API
};

} // namespace soundshelf
