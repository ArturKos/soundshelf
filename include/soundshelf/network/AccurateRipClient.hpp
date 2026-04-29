#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/// AccurateRip checksum verification
/// TODO: implementacja — patrz CLAUDE.md
class AccurateRipClient : public QObject {
    Q_OBJECT
public:
    explicit AccurateRipClient(QObject* parent = nullptr);
    ~AccurateRipClient() override;

    // TODO: API
};

} // namespace soundshelf
