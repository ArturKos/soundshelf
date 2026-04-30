#pragma once

#include <QWidget>

class QListWidget;
class QStackedWidget;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;
class QPushButton;

namespace soundshelf {

/**
 * @brief Preferences page (Library / Audio / Network / Themes).
 *
 * Left-hand category list, right-hand `QStackedWidget`. Each page
 * binds a small subset of the @ref SettingsManager keys; the OK
 * button writes them all back in one pass.
 *
 * The dialog reads / writes @ref SettingsManager directly — this is
 * the one place where tight coupling is acceptable, since it *is*
 * the settings UI.
 */
class PreferencesDialog : public QWidget {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override;

signals:
    /// Emitted after the user clicks Save and the values are persisted.
    void settingsApplied();

private:
    void buildPages();
    void loadFromSettings();
    void saveToSettings();

    QListWidget*    m_pageList = nullptr;
    QStackedWidget* m_pages = nullptr;
    QPushButton*    m_saveBtn = nullptr;

    // Library
    QLineEdit*  m_libraryRoot = nullptr;
    QCheckBox*  m_watchFolders = nullptr;

    // Audio
    QCheckBox*  m_replayGain = nullptr;
    QCheckBox*  m_albumMode = nullptr;
    QCheckBox*  m_gapless = nullptr;
    QSpinBox*   m_crossfadeMs = nullptr;

    // Network
    QLineEdit*  m_lastfmKey = nullptr;
    QLineEdit*  m_lbToken = nullptr;
    QLineEdit*  m_acoustidKey = nullptr;

    // Look & feel
    QComboBox*  m_themeBox = nullptr;
    QComboBox*  m_localeBox = nullptr;
};

} // namespace soundshelf
