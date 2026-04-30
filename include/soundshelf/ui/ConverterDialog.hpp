#pragma once

#include <QWidget>
#include <QList>
#include "soundshelf/io/FormatConverter.hpp"
#include "soundshelf/core/Track.hpp"

class QComboBox;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QListWidget;
class QCheckBox;

namespace soundshelf {

/**
 * @brief Format-conversion job dialog.
 *
 * Lets the user pick a target format + output folder + naming pattern,
 * then drives @ref FormatConverter sequentially over the selected
 * tracks. Per-track progress is displayed via a progress bar; the
 * outcome of each track is appended to the log list.
 */
class ConverterDialog : public QWidget {
    Q_OBJECT
public:
    explicit ConverterDialog(QWidget* parent = nullptr);
    ~ConverterDialog() override;

    /// Loads the tracks the user wants to convert.
    void setTracks(const QList<Track>& tracks);

private slots:
    void startNext();
    void onConverterProgress(int pct);
    void onConverterFinished(bool ok, const QString& message);

private:
    QList<Track>    m_tracks;
    int             m_index = 0;
    FormatConverter m_conv;

    QListWidget*    m_log = nullptr;
    QComboBox*      m_formatBox = nullptr;
    QLineEdit*      m_outputDir = nullptr;
    QCheckBox*      m_overwrite = nullptr;
    QProgressBar*   m_progress = nullptr;
    QPushButton*    m_startBtn = nullptr;
    QPushButton*    m_cancelBtn = nullptr;
};

} // namespace soundshelf
