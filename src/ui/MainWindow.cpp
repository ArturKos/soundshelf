#include "soundshelf/ui/MainWindow.hpp"

#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/core/Translator.hpp"
#include "soundshelf/core/SettingsManager.hpp"
#include "soundshelf/core/LibraryManager.hpp"
#include "soundshelf/core/DiscManager.hpp"
#include "soundshelf/core/PlaylistManager.hpp"
#include "soundshelf/core/DuplicateDetector.hpp"
#include "soundshelf/core/Disc.hpp"
#include "soundshelf/core/Track.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/ui/LibraryView.hpp"
#include "soundshelf/ui/DiscView.hpp"
#include "soundshelf/ui/PlayerWidget.hpp"
#include "soundshelf/ui/SpectrumWidget.hpp"
#include "soundshelf/ui/EqualizerWidget.hpp"
#include "soundshelf/ui/LyricsWidget.hpp"
#include "soundshelf/ui/StatsWidget.hpp"
#include "soundshelf/ui/PreferencesDialog.hpp"
#include "soundshelf/ui/SmartPlaylistBuilder.hpp"
#include "soundshelf/ui/BatchTagEditor.hpp"
#include "soundshelf/ui/DuplicateDialog.hpp"
#include "soundshelf/ui/ConverterDialog.hpp"
#include "soundshelf/ui/DiscReadDialog.hpp"
#include "soundshelf/ui/TrayIcon.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace soundshelf {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("SoundShelfMainWindow"));
    setWindowTitle(tr("SoundShelf"));
    resize(1280, 800);

    m_engine      = new PlayerEngine(this);
    m_library     = new LibraryManager(this);
    m_discMgr     = new DiscManager(this);
    m_playlistMgr = new PlaylistManager(this);

    auto initRes = m_engine->initialize();
    if (!initRes) {
        QMessageBox::warning(this, tr("Audio backend"),
            tr("Cannot initialize libmpv: %1").arg(initRes.error().message));
    }

    setupUi();
    setupMenus();
    setupStatusBar();
    setupTray();
    connectSignals();
    restoreState();

    reloadLibrary();
    reloadDiscs();

    connect(&Translator::instance(), &Translator::localeChanged,
            this, &MainWindow::retranslateUi);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    // Central — table of tracks
    m_libraryView = new LibraryView(this);
    setCentralWidget(m_libraryView);

    // Left dock — disc grid
    auto* discDock = new QDockWidget(tr("Discs"), this);
    discDock->setObjectName(QStringLiteral("DiscDock"));
    m_discView = new DiscView(discDock);
    discDock->setWidget(m_discView);
    discDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, discDock);

    // Right dock — tab with spectrum / EQ / lyrics
    auto* rightDock = new QDockWidget(tr("Now playing"), this);
    rightDock->setObjectName(QStringLiteral("RightDock"));
    m_rightTabs = new QTabWidget(rightDock);
    m_spectrum = new SpectrumWidget(m_rightTabs);
    m_eq       = new EqualizerWidget(m_rightTabs);
    m_lyrics   = new LyricsWidget(m_rightTabs);
    m_rightTabs->addTab(m_spectrum, tr("Spectrum"));
    m_rightTabs->addTab(m_eq,       tr("Equalizer"));
    m_rightTabs->addTab(m_lyrics,   tr("Lyrics"));
    rightDock->setWidget(m_rightTabs);
    rightDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    // Bottom dock — transport bar
    auto* playerDock = new QDockWidget(tr("Player"), this);
    playerDock->setObjectName(QStringLiteral("PlayerDock"));
    playerDock->setFeatures(QDockWidget::DockWidgetMovable);
    m_player = new PlayerWidget(playerDock);
    playerDock->setWidget(m_player);
    playerDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    playerDock->setTitleBarWidget(new QWidget(playerDock));   // hide title bar
    addDockWidget(Qt::BottomDockWidgetArea, playerDock);

    // Wire engine into the relevant widgets.
    m_player->attachEngine(m_engine);
    m_spectrum->attachEngine(m_engine);
    m_eq->attachEngine(m_engine);
}

void MainWindow::setupMenus() {
    auto* mb = menuBar();

    // File
    auto* fileMenu = mb->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open file…"), QKeySequence::Open, this, [this]{
        const QString path = QFileDialog::getOpenFileName(this, tr("Open audio file"));
        if (!path.isEmpty() && m_engine) m_engine->playFile(path);
    });
    fileMenu->addAction(tr("&Import folder…"), QKeySequence("Ctrl+I"),
                         this, &MainWindow::onImportFolder);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    // Disc
    auto* discMenu = mb->addMenu(tr("&Disc"));
    discMenu->addAction(tr("Add disc from &folder…"), QKeySequence("Ctrl+Shift+A"),
                         this, &MainWindow::onAddDiscFromFolder);
    discMenu->addAction(tr("Read disc from &drive…"), QKeySequence("Ctrl+D"),
                         this, &MainWindow::onAddDiscFromDrive);
    discMenu->addAction(tr("Import disc &image (CUE)…"),
                         this, &MainWindow::onAddDiscFromImage);

    // Library
    auto* libMenu = mb->addMenu(tr("&Library"));
    libMenu->addAction(tr("&Batch tag editor…"), this, &MainWindow::onOpenBatchTagEditor);
    libMenu->addAction(tr("&Duplicate detector…"), this, &MainWindow::onOpenDuplicateDetector);
    libMenu->addAction(tr("&Smart playlist…"),  this, &MainWindow::onOpenSmartPlaylistBuilder);

    // Playback
    auto* pbMenu = mb->addMenu(tr("&Playback"));
    pbMenu->addAction(tr("&Play / Pause"), QKeySequence(Qt::Key_Space),
                       this, &MainWindow::onTogglePlayPause);
    pbMenu->addAction(tr("&Next"), QKeySequence(Qt::Key_Right), this, [this]{
        if (m_playlistMgr->advanceQueue()) {
            const auto& q = m_playlistMgr->queue();
            const int i  = m_playlistMgr->queueIndex();
            if (i >= 0 && i < q.size()) onTrackActivated(q[i]);
        }
    });
    pbMenu->addAction(tr("Pre&vious"), QKeySequence(Qt::Key_Left), this, [this]{
        if (m_playlistMgr->retreatQueue()) {
            const auto& q = m_playlistMgr->queue();
            const int i  = m_playlistMgr->queueIndex();
            if (i >= 0 && i < q.size()) onTrackActivated(q[i]);
        }
    });

    // Tools
    auto* toolsMenu = mb->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("Format &converter…"), this, &MainWindow::onOpenConverter);
    toolsMenu->addAction(tr("&Statistics"),        this, &MainWindow::onOpenStats);

    // View
    auto* viewMenu = mb->addMenu(tr("&View"));
    auto* langMenu = viewMenu->addMenu(tr("&Language"));
    const auto codes = Translator::supportedLocales();
    const auto names = Translator::supportedDisplayNames();
    for (int i = 0; i < codes.size(); ++i) {
        const QString code = codes[i];
        langMenu->addAction(names[i], this, [code]{
            Translator::instance().loadLocale(code);
        });
    }

    // Help
    auto* helpMenu = mb->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Preferences…"), QKeySequence::Preferences,
                         this, &MainWindow::onPreferences);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About SoundShelf"), this, &MainWindow::onAbout);
}

void MainWindow::setupStatusBar() {
    auto* sb = statusBar();
    m_statusLibCount = new QLabel(tr("0 tracks"), sb);
    m_statusDbInfo   = new QLabel(tr("SQLite ready"), sb);
    sb->addWidget(m_statusLibCount, 1);
    sb->addPermanentWidget(m_statusDbInfo);
}

void MainWindow::setupTray() {
    if (!TrayIcon::isAvailable()) return;
    m_tray = new TrayIcon(this);
    m_tray->attachEngine(m_engine);
    connect(m_tray, &TrayIcon::showMainWindowRequested,
            this, &MainWindow::onTrayActivated);
    connect(m_tray, &TrayIcon::quitRequested, qApp, &QApplication::quit);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("SoundShelf"));
    menuBar()->clear();
    setupMenus();
}

void MainWindow::connectSignals() {
    if (m_engine) {
        connect(m_engine, &PlayerEngine::error, this, [this](const QString& msg) {
            statusBar()->showMessage(msg, 5000);
        });
        connect(m_engine, &PlayerEngine::positionChanged,
                m_lyrics, &LyricsWidget::setPositionMs);
    }
    if (m_libraryView) {
        connect(m_libraryView, &LibraryView::trackActivated,
                this, &MainWindow::onTrackActivated);
    }
    if (m_discView) {
        connect(m_discView, &DiscView::discActivated,
                this, [this](const Disc& d) { onDiscActivated(d.id); });
    }
    if (m_player) {
        connect(m_player, &PlayerWidget::requestNext, this, [this]{
            if (m_playlistMgr->advanceQueue()) onTrackActivated(
                m_playlistMgr->queue()[m_playlistMgr->queueIndex()]);
        });
        connect(m_player, &PlayerWidget::requestPrev, this, [this]{
            if (m_playlistMgr->retreatQueue()) onTrackActivated(
                m_playlistMgr->queue()[m_playlistMgr->queueIndex()]);
        });
    }
    if (m_library) {
        connect(m_library, &LibraryManager::importProgress,
                this, &MainWindow::onImportProgress);
        connect(m_library, &LibraryManager::importFinished,
                this, &MainWindow::onImportFinished);
    }
    if (m_discMgr) {
        connect(m_discMgr, &DiscManager::discAdded,   this, [this](int){ reloadDiscs(); });
        connect(m_discMgr, &DiscManager::discUpdated, this, [this](int){ reloadDiscs(); });
    }
}

void MainWindow::restoreState() {
    QSettings s;
    restoreGeometry(s.value(QStringLiteral("MainWindow/geometry")).toByteArray());
    QMainWindow::restoreState(s.value(QStringLiteral("MainWindow/state")).toByteArray());
}

void MainWindow::saveState() {
    QSettings s;
    s.setValue(QStringLiteral("MainWindow/geometry"), saveGeometry());
    s.setValue(QStringLiteral("MainWindow/state"), QMainWindow::saveState());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveState();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QMainWindow::changeEvent(event);
}

void MainWindow::reloadLibrary() {
    auto r = DatabaseManager::instance().listTracks(5000, 0);
    if (!r) {
        statusBar()->showMessage(tr("Cannot list tracks: %1").arg(r.error().message), 5000);
        return;
    }
    m_libraryView->setTracks(r.value());
    m_statusLibCount->setText(tr("%1 tracks").arg(r.value().size()));
}

void MainWindow::reloadDiscs() {
    auto r = DatabaseManager::instance().listDiscs(DiscType::Folder, 1000);
    if (!r) return;
    QList<Disc> all = r.value();
    if (auto p = DatabaseManager::instance().listDiscs(DiscType::Physical, 1000); p) all += p.value();
    if (auto i = DatabaseManager::instance().listDiscs(DiscType::Image,    1000); i) all += i.value();
    m_discView->setDiscs(all);
}

void MainWindow::onTrackActivated(const Track& t) {
    if (!m_engine) return;
    auto r = m_engine->play(t);
    if (!r) {
        statusBar()->showMessage(tr("Cannot play: %1").arg(r.error().message), 5000);
    }
}

void MainWindow::onDiscActivated(int discId) {
    auto r = DatabaseManager::instance().tracksByDisc(discId);
    if (!r) return;
    m_libraryView->setTracks(r.value());
    if (!r.value().isEmpty()) m_playlistMgr->setQueue(r.value(), 0);
}

void MainWindow::onImportFolder() {
    const QString folder = QFileDialog::getExistingDirectory(this,
        tr("Pick a folder to import"));
    if (folder.isEmpty()) return;
    auto r = m_library->importFolder(folder);
    if (!r) {
        statusBar()->showMessage(tr("Import failed: %1").arg(r.error().message), 5000);
    } else {
        statusBar()->showMessage(tr("Importing %1…").arg(folder), 3000);
    }
}

void MainWindow::onImportProgress(int pct, const QString& currentPath) {
    statusBar()->showMessage(tr("Imported %1%: %2").arg(pct).arg(currentPath), 1500);
}

void MainWindow::onImportFinished(int filesProcessed, int errors) {
    statusBar()->showMessage(
        tr("Import finished: %1 files, %2 errors").arg(filesProcessed).arg(errors),
        5000);
    reloadLibrary();
}

void MainWindow::onAddDiscFromFolder() {
    const QString folder = QFileDialog::getExistingDirectory(this,
        tr("Select disc folder"));
    if (folder.isEmpty()) return;
    auto r = m_discMgr->addFromFolder(folder);
    if (!r) {
        QMessageBox::warning(this, tr("Disc"),
            tr("Cannot add disc: %1").arg(r.error().message));
    }
}

void MainWindow::onAddDiscFromDrive() {
    auto* dlg = new DiscReadDialog;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(Qt::Window);
    dlg->setWindowTitle(tr("Read disc from drive"));
    connect(dlg, &DiscReadDialog::readRequested, this,
            [this, dlg](DiscReadDialog::Mode mode, const QString& src) {
        Result<int> res = Result<int>::err(Error::InvalidArgument, QString());
        switch (mode) {
            case DiscReadDialog::Mode::Drive:  res = m_discMgr->addFromCdda(src);   break;
            case DiscReadDialog::Mode::Folder: res = m_discMgr->addFromFolder(src); break;
            case DiscReadDialog::Mode::Image:  res = m_discMgr->addFromImage(src);  break;
        }
        if (!res) {
            QMessageBox::warning(dlg, tr("Disc"),
                tr("Cannot read disc: %1").arg(res.error().message));
            return;
        }
        if (auto t = DatabaseManager::instance().tracksByDisc(res.value()); t) {
            QStringList titles;
            for (const auto& tr : t.value()) titles << tr.title;
            dlg->setPreview(titles);
        }
    });
    dlg->show();
}

void MainWindow::onAddDiscFromImage() {
    const QString path = QFileDialog::getOpenFileName(this,
        tr("Select CUE / audio image"), {},
        tr("CUE / audio (*.cue *.flac *.wav *.ape *.wv)"));
    if (path.isEmpty()) return;
    auto r = m_discMgr->addFromImage(path);
    if (!r) {
        QMessageBox::warning(this, tr("Disc"),
            tr("Cannot import image: %1").arg(r.error().message));
    }
}

void MainWindow::onPreferences() {
    auto* dlg = new PreferencesDialog;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(Qt::Window);
    dlg->setWindowTitle(tr("Preferences"));
    dlg->show();
}

void MainWindow::onAbout() {
    QMessageBox::about(this, tr("About SoundShelf"),
        tr("<h3>SoundShelf 0.3.0</h3>"
           "<p>Cross-platform audio catalog and player.</p>"
           "<p>Built with Qt 6, libmpv, TagLib, libcdio.</p>"
           "<p>License: GPL v3</p>"));
}

void MainWindow::onChangeLanguage() {}

void MainWindow::onOpenSmartPlaylistBuilder() {
    auto* dlg = new SmartPlaylistBuilder;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(Qt::Window);
    dlg->setWindowTitle(tr("Smart playlist"));
    connect(dlg, &SmartPlaylistBuilder::saveRequested,
            this, [this](const QString& name, const QString& json) {
        if (auto r = m_playlistMgr->createSmart(name, json); !r) {
            QMessageBox::warning(this, tr("Smart playlist"),
                tr("Save failed: %1").arg(r.error().message));
        }
    });
    dlg->show();
}

void MainWindow::onOpenBatchTagEditor() {
    auto* dlg = new BatchTagEditor;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(Qt::Window);
    dlg->setWindowTitle(tr("Batch tag editor"));
    QList<Track> selected;
    for (int id : m_libraryView->selectedTrackIds()) {
        if (auto t = DatabaseManager::instance().getTrack(id); t) selected << t.value();
    }
    dlg->setTracks(selected);
    dlg->show();
}

void MainWindow::onOpenDuplicateDetector() {
    auto* dlg = new DuplicateDialog;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(Qt::Window);
    dlg->setWindowTitle(tr("Duplicate detector"));
    DuplicateDetector det;
    if (auto r = det.findDuplicates(); r) dlg->setGroups(r.value());
    dlg->show();
}

void MainWindow::onOpenStats() {
    auto* dlg = new StatsWidget;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(Qt::Window);
    dlg->setWindowTitle(tr("Statistics"));
    dlg->refresh();
    dlg->show();
}

void MainWindow::onOpenConverter() {
    auto* dlg = new ConverterDialog;
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags(Qt::Window);
    dlg->setWindowTitle(tr("Format converter"));
    QList<Track> selected;
    for (int id : m_libraryView->selectedTrackIds()) {
        if (auto t = DatabaseManager::instance().getTrack(id); t) selected << t.value();
    }
    dlg->setTracks(selected);
    dlg->show();
}

void MainWindow::onTogglePlayPause() {
    if (!m_engine) return;
    if (m_engine->state() == PlayerState::Playing) m_engine->pause();
    else                                            m_engine->resume();
}

void MainWindow::onTrayActivated() {
    if (isVisible()) hide();
    else { show(); raise(); activateWindow(); }
}

} // namespace soundshelf
