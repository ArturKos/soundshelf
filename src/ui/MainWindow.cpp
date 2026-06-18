#include "soundshelf/ui/MainWindow.hpp"

#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/core/Translator.hpp"
#include "soundshelf/core/SettingsManager.hpp"
#include "soundshelf/core/LibraryManager.hpp"
#include "soundshelf/core/DiscManager.hpp"
#include "soundshelf/core/PlaylistManager.hpp"
#include "soundshelf/core/Scrobbler.hpp"
#include "soundshelf/core/ScrobbleDrainer.hpp"
#include "soundshelf/network/LastFmClient.hpp"
#include "soundshelf/network/ListenBrainzClient.hpp"
#include "soundshelf/network/MusicBrainzClient.hpp"
#include "soundshelf/network/CoverArtClient.hpp"
#include "soundshelf/network/LyricsClient.hpp"
#include "soundshelf/core/DiscEnricher.hpp"
#include "soundshelf/core/DuplicateDetector.hpp"
#include "soundshelf/core/Disc.hpp"
#include "soundshelf/core/Track.hpp"
#include "soundshelf/data/DatabaseManager.hpp"
#include "soundshelf/data/FTS5Index.hpp"
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
#include "soundshelf/ui/SourcesModel.hpp"
#include "soundshelf/core/VisualizationFeeder.hpp"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFutureWatcher>
#include <QInputDialog>
#include <QJsonDocument>
#include <QDockWidget>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
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

    m_engine       = new PlayerEngine(this);
    m_library      = new LibraryManager(this);
    m_discMgr      = new DiscManager(this);
    m_playlistMgr  = new PlaylistManager(this);
    m_scrobbler    = new Scrobbler(this);
    m_lastfm       = new LastFmClient(this);
    m_listenbrainz = new ListenBrainzClient(this);

    // Pull credentials from SettingsManager and bring up the drainer.
    auto& settings = SettingsManager::instance();
    m_listenbrainz->setUserToken(settings.listenBrainzToken());
    m_lastfm->setSessionKey(settings.lastFmSessionToken());
    // (Last.fm api key/secret should also come from settings; for now
    // the user has to set them via setApiCredentials elsewhere.)
    m_scrobbler->setListenBrainzEnabled(!settings.listenBrainzToken().isEmpty());
    m_scrobbler->setLastFmEnabled(!settings.lastFmSessionToken().isEmpty());
    m_drainer = new ScrobbleDrainer(m_scrobbler, m_lastfm, m_listenbrainz, this);
    m_drainer->start();

    m_musicbrainz  = new MusicBrainzClient(this);
    m_coverArt     = new CoverArtClient(this);
    m_lyricsClient = new LyricsClient(this);
    m_enricher     = new DiscEnricher(m_musicbrainz, m_coverArt, this);

    auto initRes = m_engine->initialize();
    if (!initRes) {
        QMessageBox::warning(this, tr("Audio backend"),
            tr("Cannot initialize libmpv: %1").arg(initRes.error().message));
    }

    setupUi();

    m_visFeeder = new VisualizationFeeder(this);
    m_visFeeder->attachEngine(m_engine);

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

    // Left dock — library sources panel
    auto* sourcesDock = new QDockWidget(tr("Sources"), this);
    sourcesDock->setObjectName(QStringLiteral("SourcesDock"));
    m_sourcesModel = new SourcesModel(this);
    auto* sourcesView = new QListView(sourcesDock);
    sourcesView->setModel(m_sourcesModel);
    sourcesView->setEditTriggers(QAbstractItemView::DoubleClicked);
    sourcesView->setContextMenuPolicy(Qt::CustomContextMenu);
    sourcesDock->setWidget(sourcesView);
    sourcesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, sourcesDock);

    // Wire selection → source filter
    connect(sourcesView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
        if (!current.isValid()) return;
        onSourceSelected(m_sourcesModel->sourceIdAt(current.row()));
    });

    // Context menu: Rename / Remove
    connect(sourcesView, &QListView::customContextMenuRequested,
            this, [this, sourcesView](const QPoint& pos) {
        const QModelIndex idx = sourcesView->indexAt(pos);
        if (!idx.isValid() || idx.row() == 0) return;  // skip "All music"
        QMenu menu(this);
        menu.addAction(tr("Rename…"), this, [this, sourcesView, idx]() {
            const QString current = m_sourcesModel->data(idx, Qt::DisplayRole).toString();
            bool ok = false;
            const QString newLabel = QInputDialog::getText(
                this, tr("Rename source"), tr("New name:"),
                QLineEdit::Normal, current, &ok);
            if (ok && !newLabel.trimmed().isEmpty())
                m_sourcesModel->setData(idx, newLabel.trimmed(), Qt::EditRole);
        });
        menu.addSeparator();
        menu.addAction(tr("Remove source"), this, [this, sourcesView, idx]() {
            if (QMessageBox::question(this, tr("Remove source"),
                    tr("Remove this source from the library? Tracks will remain in the database."),
                    QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
                return;
            const int row = idx.row();
            const int removedId = m_sourcesModel->sourceIdAt(row);  // capture before removal
            if (m_sourcesModel->removeAt(row)) {
                // If the active source was removed, fall back to "All music"
                if (m_activeSourceId == removedId)
                    onSourceSelected(-1);
            }
        });
        menu.exec(sourcesView->viewport()->mapToGlobal(pos));
    });

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

        // Scrobbler hooks — feed every Player event into the threshold
        // tracker. The actual queue insert happens inside Scrobbler when
        // onTrackEnded fires with completed=true (or after the
        // 50%/4-min watermark).
        if (m_scrobbler) {
            connect(m_engine, &PlayerEngine::trackChanged,
                    m_scrobbler, &Scrobbler::onTrackStarted);
            connect(m_engine, &PlayerEngine::positionChanged,
                    m_scrobbler, &Scrobbler::onPositionTick);
            connect(m_engine, &PlayerEngine::trackEnded,
                    m_scrobbler, &Scrobbler::onTrackEnded);
        }

        // Lyrics: on track change, pull from cache; on miss go to
        // LRCLib and stash the response back into the lyrics table.
        connect(m_engine, &PlayerEngine::trackChanged, this,
                [this](const Track& t) {
            if (!m_lyrics || t.id < 0) return;
            auto& dbm = DatabaseManager::instance();
            if (auto cached = dbm.getLyrics(t.id); cached) {
                m_lyrics->setLyrics(cached.value().plain, cached.value().synced);
                return;
            }
            // Cache miss — try LRCLib if we have what it needs.
            if (!m_lyricsClient || t.title.isEmpty() || t.artist.isEmpty()) {
                m_lyrics->setLyrics(QString(), QString());
                return;
            }
            const int trackId = t.id;
            const QString album = t.album;
            auto fut = m_lyricsClient->getLyrics(t.artist, t.title, album,
                                                  qMax(0, t.durationMs / 1000));
            auto* w = new QFutureWatcher<Result<QJsonDocument>>(this);
            connect(w, &QFutureWatcher<Result<QJsonDocument>>::finished, this,
                    [this, w, trackId]() {
                const auto r = w->result();
                w->deleteLater();
                if (!r) return;
                const auto decoded = LyricsClient::decode(r.value());
                m_lyrics->setLyrics(decoded.plain, decoded.synced);
                DatabaseManager::LyricsRow row;
                row.plain  = decoded.plain;
                row.synced = decoded.synced;
                row.source = decoded.source;
                DatabaseManager::instance().setLyrics(trackId, row);
            });
            w->setFuture(fut);
        });

        // Auto-advance: when libmpv reports end-of-file, the track ended
        // naturally — pick the next entry from the runtime queue and play.
        connect(m_engine, &PlayerEngine::trackEnded, this,
                [this](const Track&, int /*playedMs*/, bool /*completed*/) {
            if (!m_playlistMgr->advanceQueue()) return;
            const auto& q = m_playlistMgr->queue();
            const int i = m_playlistMgr->queueIndex();
            if (i >= 0 && i < q.size()) onTrackActivated(q[i]);
        });
    }
    if (m_libraryView) {
        connect(m_libraryView, &LibraryView::trackActivated,
                this, &MainWindow::onTrackActivated);
        connect(m_libraryView, &LibraryView::searchRequested, this,
                [this](const QString& q) {
            if (q.isEmpty()) { reloadLibrary(); return; }
            FTS5Index fts;
            auto idsR = fts.searchTrackIds(q, 500);
            if (!idsR) return;
            QList<Track> rows;
            auto& dbm = DatabaseManager::instance();
            for (int id : idsR.value()) {
                if (auto t = dbm.getTrack(id); t) rows.append(t.value());
            }
            m_libraryView->setTracks(rows);
            m_statusLibCount->setText(tr("%1 matches").arg(rows.size()));
        });
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
        connect(m_discMgr, &DiscManager::discAdded, this, [this](int discId){
            reloadDiscs();
            if (!m_enricher) return;
            // Two enrichment paths: physical/CUE discs carry tocDiscId
            // and resolve via MB's /discid endpoint; folder imports
            // fall back to a free-text search by (artist, album).
            auto d = DatabaseManager::instance().getDisc(discId);
            if (!d) return;
            const Disc& disc = d.value();
            if (!disc.tocDiscId.isEmpty()) {
                m_enricher->enrichByDiscId(discId);
            } else if (!disc.artist.isEmpty() && !disc.title.isEmpty()) {
                m_enricher->enrichByMetadata(discId, disc.artist, disc.title);
            }
        });
        connect(m_discMgr, &DiscManager::discUpdated, this, [this](int){ reloadDiscs(); });
        if (m_enricher) {
            connect(m_enricher, &DiscEnricher::enrichmentFinished, this,
                    [this](int /*discId*/, bool ok, const QString& msg) {
                if (ok && !msg.isEmpty()) {
                    statusBar()->showMessage(
                        tr("Disc enriched: %1").arg(msg), 5000);
                    reloadDiscs();
                }
            });
        }
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
    auto& dbm = DatabaseManager::instance();
    QList<Track> tracks;
    if (m_activeSourceId >= 0) {
        auto r = dbm.tracksBySource(m_activeSourceId);
        if (!r) {
            statusBar()->showMessage(
                tr("Cannot list tracks: %1").arg(r.error().message), 5000);
            return;
        }
        tracks = r.value();
    } else {
        auto r = dbm.listTracks(5000, 0);
        if (!r) {
            statusBar()->showMessage(
                tr("Cannot list tracks: %1").arg(r.error().message), 5000);
            return;
        }
        tracks = r.value();
    }
    m_libraryView->setTracks(tracks);
    m_statusLibCount->setText(tr("%1 tracks").arg(tracks.size()));
}

void MainWindow::reloadSources() {
    if (m_sourcesModel) m_sourcesModel->reload();
}

void MainWindow::onSourceSelected(int sourceId) {
    m_activeSourceId = sourceId;
    reloadLibrary();
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

    // Push the visible library list as the playback queue, anchored at
    // the activated track. This way auto-advance has a list to walk.
    if (m_libraryView) {
        const auto& visible = m_libraryView->tracks();
        int startIdx = -1;
        for (int i = 0; i < visible.size(); ++i) {
            if (visible[i].id == t.id) { startIdx = i; break; }
        }
        if (startIdx >= 0 && (m_playlistMgr->queue().isEmpty()
            || m_playlistMgr->queue()[m_playlistMgr->queueIndex()].id != t.id)) {
            m_playlistMgr->setQueue(visible, startIdx);
        }
    }

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
    reloadSources();
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
