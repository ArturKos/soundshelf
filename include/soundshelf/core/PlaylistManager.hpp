#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Zarządzanie playlistami statycznymi i smart
/// TODO: implementacja — patrz CLAUDE.md
class PlaylistManager : public QObject {
    Q_OBJECT
public:
    explicit PlaylistManager(QObject* parent = nullptr);
    ~PlaylistManager() override;

    // TODO: API
};

} // namespace soundshelf
