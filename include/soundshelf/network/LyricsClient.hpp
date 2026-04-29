#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/// LRCLib REST API — lyrics z timestampami
/// TODO: implementacja — patrz CLAUDE.md
class LyricsClient : public QObject {
    Q_OBJECT
public:
    explicit LyricsClient(QObject* parent = nullptr);
    ~LyricsClient() override;

    // TODO: API
};

} // namespace soundshelf
