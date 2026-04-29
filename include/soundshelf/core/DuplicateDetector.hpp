#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Track.hpp"
#include <QList>

namespace soundshelf {

/// Wyszukiwanie duplikatów: MD5, AcoustID, tag matching
/// TODO: implementacja — patrz CLAUDE.md
class DuplicateDetector : public QObject {
    Q_OBJECT
public:
    explicit DuplicateDetector(QObject* parent = nullptr);
    ~DuplicateDetector() override;

    // TODO: API
};

} // namespace soundshelf
