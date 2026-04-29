#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Export playlist → M3U / PLS / XSPF.
/// TODO: implementacja
class PlaylistExporter : public QObject {
    Q_OBJECT
public:
    explicit PlaylistExporter(QObject* parent = nullptr);
    ~PlaylistExporter() override;
};

} // namespace soundshelf
