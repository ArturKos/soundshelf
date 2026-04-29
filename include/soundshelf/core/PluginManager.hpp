#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"
#include "soundshelf/plugins/VisualizationPlugin.hpp"
#include <QList>

namespace soundshelf {

/// Ładowanie i zarządzanie pluginami (natywne + Winamp vis)
/// TODO: implementacja — patrz CLAUDE.md
class PluginManager : public QObject {
    Q_OBJECT
public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    // TODO: API
};

} // namespace soundshelf
