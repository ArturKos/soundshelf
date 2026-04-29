#pragma once

#include <QObject>
#include <QString>
#include <QPainter>
#include <QVector>

namespace soundshelf {

/// Interfejs ABC dla wizualizacji
/// TODO: implementacja — patrz CLAUDE.md
class VisualizationPlugin : public QObject {
    Q_OBJECT
public:
    explicit VisualizationPlugin(QObject* parent = nullptr);
    ~VisualizationPlugin() override;

    // TODO: API
};

} // namespace soundshelf
