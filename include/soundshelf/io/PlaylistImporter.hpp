#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Import M3U / PLS / XSPF → lista filepath'ów.
/// TODO: implementacja
class PlaylistImporter : public QObject {
    Q_OBJECT
public:
    explicit PlaylistImporter(QObject* parent = nullptr);
    ~PlaylistImporter() override;
};

} // namespace soundshelf
