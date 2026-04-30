#pragma once

#include <QWidget>

class QLineEdit;
class QComboBox;
class QPushButton;
class QListWidget;
class QProgressBar;

namespace soundshelf {

/**
 * @brief Dialog driving "Read disc from drive / folder / CUE image".
 *
 * The user picks a source mode (Drive / Folder / CUE image), points
 * at the source, and presses Read. The dialog forwards the request
 * to the host via @ref readRequested; the host calls
 * @ref DiscManager and may push the parsed track list back via
 * @ref setPreview before the user confirms.
 */
class DiscReadDialog : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Drive, Folder, Image };

    explicit DiscReadDialog(QWidget* parent = nullptr);
    ~DiscReadDialog() override;

    /// Populates the preview list (called by the host after readToc).
    void setPreview(const QStringList& trackTitles);

signals:
    /// Emitted when the user clicks Read.
    void readRequested(Mode mode, const QString& source);

private:
    QComboBox*    m_modeBox = nullptr;
    QLineEdit*    m_sourceEdit = nullptr;
    QPushButton*  m_browseBtn = nullptr;
    QPushButton*  m_readBtn = nullptr;
    QListWidget*  m_preview = nullptr;
    QProgressBar* m_progress = nullptr;
};

} // namespace soundshelf
