#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/// Last.fm + ListenBrainz scrobble queue
/// TODO: implementacja — patrz CLAUDE.md
class Scrobbler : public QObject {
    Q_OBJECT
public:
    explicit Scrobbler(QObject* parent = nullptr);
    ~Scrobbler() override;

    // TODO: API
};

} // namespace soundshelf
