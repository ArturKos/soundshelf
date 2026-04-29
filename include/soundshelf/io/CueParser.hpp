#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf {

/// Parser plików .cue → lista TocEntry
/// TODO: implementacja — patrz CLAUDE.md
class CueParser : public QObject {
    Q_OBJECT
public:
    explicit CueParser(QObject* parent = nullptr);
    ~CueParser() override;

    // TODO: API
};

} // namespace soundshelf
