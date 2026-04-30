#include "soundshelf/ui/DiscReadDialog.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QFileDialog>

namespace soundshelf {

DiscReadDialog::DiscReadDialog(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_modeBox = new QComboBox(this);
    m_modeBox->addItem(tr("Physical drive"),    int(Mode::Drive));
    m_modeBox->addItem(tr("Folder as one disc"), int(Mode::Folder));
    m_modeBox->addItem(tr("CUE image"),          int(Mode::Image));
    form->addRow(tr("Source type:"), m_modeBox);

    auto* srcRow = new QHBoxLayout;
    m_sourceEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(tr("Browse…"), this);
    srcRow->addWidget(m_sourceEdit, 1);
    srcRow->addWidget(m_browseBtn);
    form->addRow(tr("Source:"), srcRow);
    root->addLayout(form);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);  // indeterminate
    m_progress->setVisible(false);
    root->addWidget(m_progress);

    m_preview = new QListWidget(this);
    root->addWidget(m_preview, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    m_readBtn = new QPushButton(tr("Read"), this);
    btnRow->addWidget(m_readBtn);
    root->addLayout(btnRow);

    connect(m_browseBtn, &QPushButton::clicked, this, [this]() {
        const auto mode = static_cast<Mode>(m_modeBox->currentData().toInt());
        QString picked;
        if (mode == Mode::Folder) {
            picked = QFileDialog::getExistingDirectory(this, tr("Pick folder"));
        } else if (mode == Mode::Image) {
            picked = QFileDialog::getOpenFileName(this, tr("Pick CUE / audio image"),
                QString(), tr("CUE / audio files (*.cue *.flac *.wav *.ape *.wv)"));
        } else {
            picked = QStringLiteral("/dev/sr0");
        }
        if (!picked.isEmpty()) m_sourceEdit->setText(picked);
    });

    connect(m_readBtn, &QPushButton::clicked, this, [this]() {
        const auto mode = static_cast<Mode>(m_modeBox->currentData().toInt());
        m_progress->setVisible(true);
        emit readRequested(mode, m_sourceEdit->text());
    });
}

DiscReadDialog::~DiscReadDialog() = default;

void DiscReadDialog::setPreview(const QStringList& trackTitles) {
    m_progress->setVisible(false);
    m_preview->clear();
    m_preview->addItems(trackTitles);
}

} // namespace soundshelf
