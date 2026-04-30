#pragma once

#include <QWidget>
#include <QString>

class QPlainTextEdit;
class QLineEdit;
class QPushButton;

namespace soundshelf {

/**
 * @brief Editor for smart-playlist rule trees.
 *
 * Smart playlists are persisted as a JSON document that
 * @ref SmartPlaylistEvaluator interprets. The "proper" UI is a
 * tree-of-rules builder; until that lands the widget exposes the
 * underlying JSON in a code editor with a name field next to it.
 *
 * Saving emits @ref saveRequested with the validated JSON; the host
 * persists it via @ref PlaylistManager::createSmart /
 * @ref PlaylistManager::updateSmartRules.
 */
class SmartPlaylistBuilder : public QWidget {
    Q_OBJECT
public:
    explicit SmartPlaylistBuilder(QWidget* parent = nullptr);
    ~SmartPlaylistBuilder() override;

    /// Loads an existing smart playlist into the editor.
    void setPlaylist(const QString& name, const QString& rulesJson);

    /// Returns the currently displayed values.
    QString name() const;
    QString rulesJson() const;

signals:
    /// Emitted when the user clicks Save and the JSON validates.
    void saveRequested(const QString& name, const QString& rulesJson);

private:
    QLineEdit*      m_nameEdit = nullptr;
    QPlainTextEdit* m_jsonEdit = nullptr;
    QPushButton*    m_save = nullptr;
};

} // namespace soundshelf
