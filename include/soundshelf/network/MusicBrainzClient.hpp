#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/// MusicBrainz Web Service v2 — discid lookup, releases, recordings
/// TODO: implementacja — patrz CLAUDE.md
class MusicBrainzClient : public QObject {
    Q_OBJECT
public:
    explicit MusicBrainzClient(QObject* parent = nullptr);
    ~MusicBrainzClient() override;

    // TODO: API
};

} // namespace soundshelf
