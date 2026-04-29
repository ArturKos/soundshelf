#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/plugins/VisualizationPlugin.hpp"
class QLibrary;

namespace soundshelf {

/// Adapter Winamp visualization plugins (vis_*.dll)
/// TODO: implementacja — patrz CLAUDE.md
class WinampVisAdapter : public QObject {
    Q_OBJECT
public:
    explicit WinampVisAdapter(QObject* parent = nullptr);
    ~WinampVisAdapter() override;

    // TODO: API
};

} // namespace soundshelf
