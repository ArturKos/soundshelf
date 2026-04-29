#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/// Klient zdalnej biblioteki SoundShelf (REST → DB)
/// TODO: implementacja — patrz CLAUDE.md
class NetworkLibrary : public QObject {
    Q_OBJECT
public:
    explicit NetworkLibrary(QObject* parent = nullptr);
    ~NetworkLibrary() override;

    // TODO: API
};

} // namespace soundshelf
