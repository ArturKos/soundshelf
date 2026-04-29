#include "soundshelf/ui/MainWindow.hpp"

#include "soundshelf/core/PlayerEngine.hpp"
#include "soundshelf/core/Translator.hpp"
#include "soundshelf/core/SettingsManager.hpp"

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

    m_engine = new PlayerEngine(this);
    auto initRes = m_engine->initialize();
    if (!initRes) {
        QMessageBox::critical(this, tr("Audio backend"),
            tr("Cannot initialize libmpv: %1").arg(initRes.error().message));
    }

    setupUi();
    setupMenus();
    setupStatusBar();
    connectSignals();
    restoreState();

    connect(&Translator::instance(), &Translator::localeChanged,
            this, &MainWindow::retranslateUi);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    // TODO: pełny layout — patrz mockup soundshelf_main_full
    // Centralny widget = stos LibraryView/DiscView z TrackList,
    // dock left = sidebar z drzewem,
    // dock right = SpectrumWidget + EqualizerWidget + LyricsWidget,
    // dock bottom = PlayerWidget.

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* placeholder = new QLabel(tr("Library view goes here"), central);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    setCentralWidget(central);

    // TODO: m_libraryView = new LibraryView(...); setCentralWidget(m_libraryView);
    // TODO: docks z right panel widgetami
}

void MainWindow::setupMenus() {
    auto* mb = menuBar();

    // File
    auto* fileMenu = mb->addMenu(tr("&File"));
    auto* actOpen = fileMenu->addAction(tr("&Open file…"),
                                         QKeySequence::Open, this, [this]{
        const QString path = QFileDialog::getOpenFileName(this, tr("Open audio file"));
        if (!path.isEmpty() && m_engine) m_engine->playFile(path);
    });
    Q_UNUSED(actOpen);
    fileMenu->addAction(tr("Import folder…"), QKeySequence("Ctrl+I"), this, [this]{
        // TODO: LibraryManager::importFolder()
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    // Disc
    auto* discMenu = mb->addMenu(tr("&Disc"));
    discMenu->addAction(tr("Add disc from &folder…"), QKeySequence("Ctrl+Shift+A"),
                         this, &MainWindow::onAddDiscFromFolder);
    discMenu->addAction(tr("Read disc from &drive…"), QKeySequence("Ctrl+D"),
                         this, &MainWindow::onAddDiscFromDrive);
    discMenu->addAction(tr("Import disc &image (CUE/ISO)…"),
                         this, &MainWindow::onAddDiscFromImage);
    discMenu->addSeparator();
    discMenu->addAction(tr("&Lookup on MusicBrainz"), QKeySequence("Ctrl+M"));
    discMenu->addAction(tr("Lookup on freedb / GnuDB"));
    discMenu->addSeparator();
    discMenu->addAction(tr("&Play selected disc"));
    discMenu->addAction(tr("&Rip to library…"));
    discMenu->addAction(tr("&Edit disc metadata"), QKeySequence("Ctrl+E"));

    // Library
    auto* libMenu = mb->addMenu(tr("&Library"));
    libMenu->addAction(tr("&Batch tag editor…"), this, &MainWindow::onOpenBatchTagEditor);
    libMenu->addAction(tr("&Duplicate detector…"), this, &MainWindow::onOpenDuplicateDetector);
    libMenu->addAction(tr("&Smart playlist…"), this, &MainWindow::onOpenSmartPlaylistBuilder);

    // Playback
    auto* pbMenu = mb->addMenu(tr("&Playback"));
    pbMenu->addAction(tr("&Play / Pause"), QKeySequence(Qt::Key_Space),
                       this, &MainWindow::onTogglePlayPause);
    pbMenu->addAction(tr("&Next track"), QKeySequence::MoveToNextWord);
    pbMenu->addAction(tr("Pre&vious track"), QKeySequence::MoveToPreviousWord);
    pbMenu->addSeparator();
    pbMenu->addAction(tr("Toggle &repeat"));
    pbMenu->addAction(tr("Toggle &shuffle"));

    // Tools
    auto* toolsMenu = mb->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("Format &converter…"));
    toolsMenu->addAction(tr("&ReplayGain analyzer…"));
    toolsMenu->addAction(tr("&Fingerprint library (AcoustID)…"));
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("&Statistics"), this, &MainWindow::onOpenStats);

    // View
    auto* viewMenu = mb->addMenu(tr("&View"));
    auto* langMenu = viewMenu->addMenu(tr("&Language"));
    const auto codes = Translator::supportedLocales();
    const auto names = Translator::supportedDisplayNames();
    for (int i = 0; i < codes.size(); ++i) {
        const QString code = codes[i];
        langMenu->addAction(names[i], this, [this, code]{
            Translator::instance().loadLocale(code);
        });
    }
    viewMenu->addSeparator();
    viewMenu->addAction(tr("Toggle &equalizer"), QKeySequence("Ctrl+E"));
    viewMenu->addAction(tr("Toggle &visualization"), QKeySequence("Ctrl+V"));

    // Network
    auto* netMenu = mb->addMenu(tr("&Network"));
    netMenu->addAction(tr("Manage remote &sources…"));
    netMenu->addAction(tr("Sync with MusicBrainz…"));
    netMenu->addAction(tr("Last.fm scrobble &settings…"));
    netMenu->addAction(tr("ListenBrainz scrobble settings…"));

    // Plugins
    auto* plugMenu = mb->addMenu(tr("&Plugins"));
    plugMenu->addAction(tr("Manage &plugins…"));
    plugMenu->addAction(tr("&Install plugin…"));

    // Help
    auto* helpMenu = mb->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Preferences…"), QKeySequence::Preferences,
                         this, &MainWindow::onPreferences);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About SoundShelf"), this, &MainWindow::onAbout);
}

void MainWindow::setupStatusBar() {
    auto* sb = statusBar();
    auto* libCount = new QLabel(tr("0 tracks"), sb);
    auto* dbInfo = new QLabel(tr("SQLite ready"), sb);
    sb->addWidget(libCount, 1);
    sb->addPermanentWidget(dbInfo);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("SoundShelf"));
    // TODO: ponowne tłumaczenie wszystkich elementów (w pełnej implementacji)
    menuBar()->clear();
    setupMenus();
}

void MainWindow::connectSignals() {
    if (!m_engine) return;
    connect(m_engine, &PlayerEngine::error, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 5000);
    });
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
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::onAddDiscFromFolder() {
    const QString folder = QFileDialog::getExistingDirectory(this,
        tr("Select disc folder"));
    if (folder.isEmpty()) return;
    // TODO: DiscManager::addFromFolder(folder)
    QMessageBox::information(this, tr("Disc"),
        tr("Folder added: %1\n(TODO: implement DiscManager integration)").arg(folder));
}

void MainWindow::onAddDiscFromDrive() {
    // TODO: dialog wyboru napędu, potem CDDAReader + dialog "Read disc from drive"
    QMessageBox::information(this, tr("Disc"),
        tr("TODO: open Read disc dialog"));
}

void MainWindow::onAddDiscFromImage() {
    const QString path = QFileDialog::getOpenFileName(this,
        tr("Select CUE / ISO file"), {}, tr("Disc images (*.cue *.iso *.bin)"));
    if (path.isEmpty()) return;
    // TODO: DiscManager::addFromImage(path)
}

void MainWindow::onPreferences() {
    QMessageBox::information(this, tr("Preferences"),
        tr("TODO: open PreferencesDialog (see mockup soundshelf_preferences)"));
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
    QMessageBox::information(this, tr("Smart playlist"),
        tr("TODO: open SmartPlaylistBuilder dialog"));
}
void MainWindow::onOpenBatchTagEditor() {
    QMessageBox::information(this, tr("Batch tag editor"),
        tr("TODO: open BatchTagEditor"));
}
void MainWindow::onOpenDuplicateDetector() {
    QMessageBox::information(this, tr("Duplicates"),
        tr("TODO: open DuplicateDialog"));
}
void MainWindow::onOpenStats() {
    QMessageBox::information(this, tr("Statistics"),
        tr("TODO: open StatsWidget"));
}

void MainWindow::onTogglePlayPause() {
    if (!m_engine) return;
    if (m_engine->state() == PlayerState::Playing) m_engine->pause();
    else m_engine->resume();
}

void MainWindow::onTrayActivated() {
    if (isVisible()) hide();
    else show();
}

} // namespace soundshelf
