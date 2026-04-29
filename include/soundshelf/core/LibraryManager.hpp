#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"
#include <QStringList>

namespace soundshelf {

/// Wysokopoziomowy manager biblioteki — import, rescan, search facade
/// TODO: implementacja — patrz CLAUDE.md
class LibraryManager : public QObject {
    Q_OBJECT
public:
    explicit LibraryManager(QObject* parent = nullptr);
    ~LibraryManager() override;

    // TODO: API
};

} // namespace soundshelf
