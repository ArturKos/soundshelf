#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Disc.hpp"
#include <memory>

namespace soundshelf {

/// Manager płyt — fizyczne CD, foldery, obrazy, remote
/// TODO: implementacja — patrz CLAUDE.md
class DiscManager : public QObject {
    Q_OBJECT
public:
    explicit DiscManager(QObject* parent = nullptr);
    ~DiscManager() override;

    // TODO: API
};

} // namespace soundshelf
