#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/// Last.fm scrobble API + auth
/// TODO: implementacja — patrz CLAUDE.md
class LastFmClient : public QObject {
    Q_OBJECT
public:
    explicit LastFmClient(QObject* parent = nullptr);
    ~LastFmClient() override;

    // TODO: API
};

} // namespace soundshelf
