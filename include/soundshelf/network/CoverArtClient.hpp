#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/// Cover Art Archive — okładki po MBID release'a
/// TODO: implementacja — patrz CLAUDE.md
class CoverArtClient : public QObject {
    Q_OBJECT
public:
    explicit CoverArtClient(QObject* parent = nullptr);
    ~CoverArtClient() override;

    // TODO: API
};

} // namespace soundshelf
