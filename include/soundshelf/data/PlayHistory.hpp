#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Tabela play_history — INSERT, query stats
/// TODO: implementacja — patrz CLAUDE.md
class PlayHistory : public QObject {
    Q_OBJECT
public:
    explicit PlayHistory(QObject* parent = nullptr);
    ~PlayHistory() override;

    // TODO: API
};

} // namespace soundshelf
