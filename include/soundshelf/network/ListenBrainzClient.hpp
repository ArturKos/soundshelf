#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/// ListenBrainz scrobble API
/// TODO: implementacja — patrz CLAUDE.md
class ListenBrainzClient : public QObject {
    Q_OBJECT
public:
    explicit ListenBrainzClient(QObject* parent = nullptr);
    ~ListenBrainzClient() override;

    // TODO: API
};

} // namespace soundshelf
