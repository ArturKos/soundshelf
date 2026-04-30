#include "soundshelf/ui/PreferencesDialog.hpp"
#include "soundshelf/core/SettingsManager.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace soundshelf {

namespace {

QWidget* makeLibraryPage(PreferencesDialog* self,
                        QLineEdit*& root, QCheckBox*& watch) {
    auto* page = new QWidget(self);
    auto* form = new QFormLayout(page);
    root = new QLineEdit(page);
    watch = new QCheckBox(self->tr("Auto-update library on file changes"), page);
    form->addRow(self->tr("Library root:"), root);
    form->addRow(QString(), watch);
    return page;
}

QWidget* makeAudioPage(PreferencesDialog* self,
                       QCheckBox*& rg, QCheckBox*& alb,
                       QCheckBox*& gap, QSpinBox*& xfade) {
    auto* page = new QWidget(self);
    auto* form = new QFormLayout(page);
    rg    = new QCheckBox(self->tr("Apply ReplayGain"), page);
    alb   = new QCheckBox(self->tr("Album-mode ReplayGain"), page);
    gap   = new QCheckBox(self->tr("Gapless playback"), page);
    xfade = new QSpinBox(page);
    xfade->setRange(0, 10000);
    xfade->setSuffix(QStringLiteral(" ms"));
    form->addRow(QString(), rg);
    form->addRow(QString(), alb);
    form->addRow(QString(), gap);
    form->addRow(self->tr("Crossfade:"), xfade);
    return page;
}

QWidget* makeNetworkPage(PreferencesDialog* self,
                         QLineEdit*& lastfm, QLineEdit*& lb, QLineEdit*& acid) {
    auto* page = new QWidget(self);
    auto* form = new QFormLayout(page);
    lastfm = new QLineEdit(page); lastfm->setEchoMode(QLineEdit::Password);
    lb     = new QLineEdit(page); lb->setEchoMode(QLineEdit::Password);
    acid   = new QLineEdit(page); acid->setEchoMode(QLineEdit::Password);
    form->addRow(self->tr("Last.fm session token:"), lastfm);
    form->addRow(self->tr("ListenBrainz token:"),    lb);
    form->addRow(self->tr("AcoustID API key:"),      acid);
    return page;
}

QWidget* makeLookPage(PreferencesDialog* self,
                      QComboBox*& theme, QComboBox*& locale) {
    auto* page = new QWidget(self);
    auto* form = new QFormLayout(page);
    theme = new QComboBox(page);
    theme->addItems({
        QStringLiteral("modern_dark"), QStringLiteral("amber_crt"),
        QStringLiteral("phosphor"),    QStringLiteral("light")
    });
    locale = new QComboBox(page);
    locale->addItems({ QStringLiteral("en"), QStringLiteral("pl"),
                       QStringLiteral("de"), QStringLiteral("fr") });
    form->addRow(self->tr("Theme:"),  theme);
    form->addRow(self->tr("Locale:"), locale);
    return page;
}

} // namespace

PreferencesDialog::PreferencesDialog(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* split = new QHBoxLayout;
    m_pageList = new QListWidget(this);
    m_pageList->setFixedWidth(160);
    split->addWidget(m_pageList);
    m_pages = new QStackedWidget(this);
    split->addWidget(m_pages, 1);
    root->addLayout(split, 1);

    buildPages();
    loadFromSettings();

    auto* row = new QHBoxLayout;
    row->addStretch(1);
    m_saveBtn = new QPushButton(tr("Save"), this);
    row->addWidget(m_saveBtn);
    root->addLayout(row);

    connect(m_pageList, &QListWidget::currentRowChanged,
            m_pages, &QStackedWidget::setCurrentIndex);
    connect(m_saveBtn, &QPushButton::clicked, this, [this]() {
        saveToSettings();
        emit settingsApplied();
    });
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::buildPages() {
    m_pageList->addItem(tr("Library"));
    m_pages->addWidget(makeLibraryPage(this, m_libraryRoot, m_watchFolders));

    m_pageList->addItem(tr("Audio"));
    m_pages->addWidget(makeAudioPage(this, m_replayGain, m_albumMode,
                                     m_gapless, m_crossfadeMs));

    m_pageList->addItem(tr("Network"));
    m_pages->addWidget(makeNetworkPage(this, m_lastfmKey, m_lbToken, m_acoustidKey));

    m_pageList->addItem(tr("Look & feel"));
    m_pages->addWidget(makeLookPage(this, m_themeBox, m_localeBox));

    m_pageList->setCurrentRow(0);
}

void PreferencesDialog::loadFromSettings() {
    auto& s = SettingsManager::instance();
    m_libraryRoot->setText(s.musicLibraryPath());
    m_watchFolders->setChecked(s.autoUpdateOnFileChanges());
    m_replayGain->setChecked(s.replayGainEnabled());
    m_albumMode->setChecked(s.replayGainAlbumMode());
    m_gapless->setChecked(s.gaplessEnabled());
    m_crossfadeMs->setValue(s.crossfadeMs());
    m_lastfmKey->setText(s.lastFmSessionToken());
    m_lbToken->setText(s.listenBrainzToken());
    m_themeBox->setCurrentText(s.theme());
    m_localeBox->setCurrentText(s.locale());
}

void PreferencesDialog::saveToSettings() {
    auto& s = SettingsManager::instance();
    s.setMusicLibraryPath(m_libraryRoot->text());
    s.setAutoUpdateOnFileChanges(m_watchFolders->isChecked());
    s.setReplayGainEnabled(m_replayGain->isChecked());
    s.setReplayGainAlbumMode(m_albumMode->isChecked());
    s.setGaplessEnabled(m_gapless->isChecked());
    s.setCrossfadeMs(m_crossfadeMs->value());
    s.setLastFmSessionToken(m_lastfmKey->text());
    s.setListenBrainzToken(m_lbToken->text());
    s.setTheme(m_themeBox->currentText());
    s.setLocale(m_localeBox->currentText());
}

} // namespace soundshelf
