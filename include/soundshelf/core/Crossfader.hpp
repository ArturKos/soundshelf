#pragma once

#include <QObject>
#include <QString>


namespace soundshelf {

/// Crossfade między utworami — druga instancja libmpv
/// TODO: implementacja — patrz CLAUDE.md
class Crossfader : public QObject {
    Q_OBJECT
public:
    explicit Crossfader(QObject* parent = nullptr);
    ~Crossfader() override;

    // TODO: API
};

} // namespace soundshelf
