#pragma once

#include <QMainWindow>

class QMenuBar;
class QStatusBar;
class QLabel;
class QTabWidget;

namespace soundshelf {

class LibraryView;
class DiscView;
class PlayerWidget;
class SpectrumWidget;
class EqualizerWidget;
class LyricsWidget;
class TrayIcon;
class SourcesModel;
class PlayerEngine;
class LibraryManager;
class DiscManager;
class PlaylistManager;
class Scrobbler;
class ScrobbleDrainer;
class LastFmClient;
class ListenBrainzClient;
class MusicBrainzClient;
class CoverArtClient;
class DiscEnricher;
class VisualizationFeeder;
class LyricsController;
struct Track;

/**
 * @brief Application shell.
 *
 * Owns @ref PlayerEngine, @ref LibraryManager, @ref DiscManager, and
 * @ref PlaylistManager and wires them to the visual widgets. Layout
 * follows `docs/mockups/`: central library table, left dock disc grid,
 * bottom dock transport bar, right dock tab panel (spectrum / EQ /
 * lyrics).
 *
 * Signal flow:
 *  - LibraryView::trackActivated → PlayerEngine::play
 *  - DiscView::discActivated     → load tracks via DiscManager →
 *                                   LibraryView::setTracks
 *  - LibraryManager::importFinished → reload library
 *  - PlayerEngine::positionChanged  → LyricsWidget, status bar
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onImportFolder();
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
    void onOpenConverter();
    void onTogglePlayPause();
    void onTrayActivated();
    void onTrackActivated(const Track& t);
    void onDiscActivated(int discId);
    void onImportProgress(int pct, const QString& currentPath);
    void onImportFinished(int filesProcessed, int errors);
    void onSourceSelected(int sourceId);

private:
    void setupUi();
    void setupMenus();
    void setupStatusBar();
    void setupTray();
    void retranslateUi();
    void connectSignals();
    void restoreState();
    void saveState();
    void reloadLibrary();
    void reloadDiscs();
    void reloadSources();

    LibraryView*     m_libraryView = nullptr;
    DiscView*        m_discView = nullptr;
    PlayerWidget*    m_player = nullptr;
    SpectrumWidget*  m_spectrum = nullptr;
    EqualizerWidget* m_eq = nullptr;
    LyricsWidget*    m_lyrics = nullptr;
    QTabWidget*      m_rightTabs = nullptr;
    TrayIcon*        m_tray = nullptr;
    QLabel*          m_statusLibCount = nullptr;
    QLabel*          m_statusDbInfo = nullptr;
    SourcesModel*    m_sourcesModel = nullptr;
    int              m_activeSourceId = -1;

    PlayerEngine*       m_engine = nullptr;
    LibraryManager*     m_library = nullptr;
    DiscManager*        m_discMgr = nullptr;
    PlaylistManager*    m_playlistMgr = nullptr;
    Scrobbler*          m_scrobbler = nullptr;
    LastFmClient*       m_lastfm = nullptr;
    ListenBrainzClient* m_listenbrainz = nullptr;
    ScrobbleDrainer*    m_drainer = nullptr;
    MusicBrainzClient*  m_musicbrainz = nullptr;
    CoverArtClient*     m_coverArt = nullptr;
    DiscEnricher*         m_enricher = nullptr;
    VisualizationFeeder*  m_visFeeder = nullptr;
    LyricsController*     m_lyricsController = nullptr;
};

} // namespace soundshelf
