#include "soundshelf/ui/ConverterDialog.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QCheckBox>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

namespace soundshelf {


ConverterDialog::ConverterDialog(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    m_formatBox = new QComboBox(this);
    m_formatBox->addItem(QStringLiteral("MP3 V0"),     int(FormatConverter::Format::Mp3V0));
    m_formatBox->addItem(QStringLiteral("MP3 320k"),   int(FormatConverter::Format::Mp3_320));
    m_formatBox->addItem(QStringLiteral("Ogg Vorbis"), int(FormatConverter::Format::OggVorbis));
    m_formatBox->addItem(QStringLiteral("Opus 128k"),  int(FormatConverter::Format::Opus_128));
    m_formatBox->addItem(QStringLiteral("AAC 256k"),   int(FormatConverter::Format::Aac_256));
    m_formatBox->addItem(QStringLiteral("FLAC"),       int(FormatConverter::Format::Flac));
    m_formatBox->addItem(QStringLiteral("WAV PCM16"),  int(FormatConverter::Format::WavPcm16));

    m_outputDir = new QLineEdit(
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation), this);
    m_overwrite = new QCheckBox(tr("Overwrite existing files"), this);
    form->addRow(tr("Target format:"), m_formatBox);
    form->addRow(tr("Output dir:"),    m_outputDir);
    form->addRow(QString(),            m_overwrite);
    root->addLayout(form);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    root->addWidget(m_progress);

    m_log = new QListWidget(this);
    root->addWidget(m_log, 1);

    auto* btnRow = new QHBoxLayout;
    m_startBtn  = new QPushButton(tr("Start"),  this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setEnabled(false);
    btnRow->addStretch(1);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_cancelBtn);
    root->addLayout(btnRow);

    connect(&m_conv, &FormatConverter::progress,
            this, &ConverterDialog::onConverterProgress);
    connect(&m_conv, &FormatConverter::finished,
            this, &ConverterDialog::onConverterFinished);

    connect(m_startBtn, &QPushButton::clicked, this, [this]() {
        if (m_tracks.isEmpty()) return;
        m_index = 0;
        m_log->clear();
        m_startBtn->setEnabled(false);
        m_cancelBtn->setEnabled(true);
        startNext();
    });
    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_conv.cancel();
        m_index = m_tracks.size();
    });
}

ConverterDialog::~ConverterDialog() = default;

void ConverterDialog::setTracks(const QList<Track>& tracks) { m_tracks = tracks; }

void ConverterDialog::startNext() {
    if (m_index >= m_tracks.size()) {
        m_startBtn->setEnabled(true);
        m_cancelBtn->setEnabled(false);
        m_log->addItem(tr("All conversions finished."));
        return;
    }
    const Track& t = m_tracks[m_index];
    const auto fmt = static_cast<FormatConverter::Format>(
        m_formatBox->currentData().toInt());

    QDir().mkpath(m_outputDir->text());
    const QString stem = QFileInfo(t.filepath).completeBaseName();
    FormatConverter::Job job;
    job.input  = t.filepath;
    job.output = QDir(m_outputDir->text())
        .filePath(stem + QLatin1Char('.') + FormatConverter::extensionForFormat(fmt));
    job.format = fmt;
    job.overwrite = m_overwrite->isChecked();

    m_log->addItem(tr("→ %1").arg(QFileInfo(job.output).fileName()));
    auto r = m_conv.start(job);
    if (!r) {
        m_log->addItem(tr("  failed: %1").arg(r.error().message));
        ++m_index;
        startNext();
    }
}

void ConverterDialog::onConverterProgress(int pct) {
    m_progress->setValue(pct);
}

void ConverterDialog::onConverterFinished(bool ok, const QString& message) {
    m_log->addItem(ok ? tr("  ok") : tr("  fail: %1").arg(message));
    ++m_index;
    startNext();
}

} // namespace soundshelf
