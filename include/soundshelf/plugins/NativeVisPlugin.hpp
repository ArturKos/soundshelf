#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/plugins/VisualizationPlugin.hpp"

namespace soundshelf {

/// Natywne pluginy SoundShelf (przyszłość)
/// TODO: implementacja — patrz CLAUDE.md
class NativeVisPlugin : public QObject {
    Q_OBJECT
public:
    explicit NativeVisPlugin(QObject* parent = nullptr);
    ~NativeVisPlugin() override;

    // TODO: API
};

} // namespace soundshelf
