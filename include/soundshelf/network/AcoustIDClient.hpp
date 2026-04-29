#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/// AcoustID REST API — fingerprint → MBID
/// TODO: implementacja — patrz CLAUDE.md
class AcoustIDClient : public QObject {
    Q_OBJECT
public:
    explicit AcoustIDClient(QObject* parent = nullptr);
    ~AcoustIDClient() override;

    // TODO: API
};

} // namespace soundshelf
