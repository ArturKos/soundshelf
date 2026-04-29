#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Konwersja formatów audio przez FFmpeg (QProcess wrapper).
/// TODO: implementacja — patrz CLAUDE.md
class FormatConverter : public QObject {
    Q_OBJECT
public:
    explicit FormatConverter(QObject* parent = nullptr);
    ~FormatConverter() override;
};

} // namespace soundshelf
