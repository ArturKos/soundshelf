#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/io/CDDAReader.hpp"
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Ripowanie płyty do plików — format, naming, tags
/// TODO: implementacja — patrz CLAUDE.md
class DiscRipper : public QObject {
    Q_OBJECT
public:
    explicit DiscRipper(QObject* parent = nullptr);
    ~DiscRipper() override;

    // TODO: API
};

} // namespace soundshelf
