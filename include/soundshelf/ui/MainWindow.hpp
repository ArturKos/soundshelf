#pragma once

#include <QMainWindow>

class QMenuBar;
class QStatusBar;

namespace soundshelf {

class LibraryView;
class DiscView;
class PlayerWidget;
class SpectrumWidget;
class EqualizerWidget;
class LyricsWidget;
class TrayIcon;
class PlayerEngine;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onAddDiscFromFolder();
    void onAddDiscFromDrive();
    void onAddDiscFromImage();
    void onPreferences();
    void onAbout();
    void onChangeLanguage();
    void onOpenSmartPlaylistBuilder();
    void onOpenBatchTagEditor();
    void onOpenDuplicateDetector();
    void onOpenStats();
    void onTogglePlayPause();
    void onTrayActivated();

private:
    void setupUi();
    void setupMenus();
    void setupStatusBar();
    void retranslateUi();
    void connectSignals();
    void restoreState();
    void saveState();

    LibraryView* m_libraryView = nullptr;
    DiscView* m_discView = nullptr;
    PlayerWidget* m_player = nullptr;
    SpectrumWidget* m_spectrum = nullptr;
    EqualizerWidget* m_eq = nullptr;
    LyricsWidget* m_lyrics = nullptr;
    TrayIcon* m_tray = nullptr;
    PlayerEngine* m_engine = nullptr;
};

} // namespace soundshelf
