#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/core/Track.hpp"

namespace soundshelf {

/// Analiza loudness EBU R128 → ReplayGain track + album
/// TODO: implementacja — patrz CLAUDE.md
class ReplayGainAnalyzer : public QObject {
    Q_OBJECT
public:
    explicit ReplayGainAnalyzer(QObject* parent = nullptr);
    ~ReplayGainAnalyzer() override;

    // TODO: API
};

} // namespace soundshelf
