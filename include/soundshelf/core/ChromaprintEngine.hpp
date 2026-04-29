#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Generowanie AcoustID fingerprint z PCM
/// TODO: implementacja — patrz CLAUDE.md
class ChromaprintEngine : public QObject {
    Q_OBJECT
public:
    explicit ChromaprintEngine(QObject* parent = nullptr);
    ~ChromaprintEngine() override;

    // TODO: API
};

} // namespace soundshelf
