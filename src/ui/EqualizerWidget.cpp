#include "soundshelf/ui/EqualizerWidget.hpp"
#include "soundshelf/core/PlayerEngine.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace soundshelf {

EqualizerWidget::EqualizerWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    loadPresetList();
}

EqualizerWidget::~EqualizerWidget() = default;

void EqualizerWidget::setupUi() {
    auto* root = new QVBoxLayout(this);

    auto* topRow = new QHBoxLayout;
    m_enable = new QCheckBox(tr("Enabled"), this);
    m_presetBox = new QComboBox(this);
    topRow->addWidget(m_enable);
    topRow->addStretch(1);
    topRow->addWidget(new QLabel(tr("Preset:"), this));
    topRow->addWidget(m_presetBox, 1);
    root->addLayout(topRow);

    auto* slidersRow = new QHBoxLayout;
    for (int i = 0; i < PlayerEngine::EQ_BANDS; ++i) {
        auto* col = new QVBoxLayout;
        const double freq = PlayerEngine::EQ_FREQS[i];
        const QString freqLabel = freq >= 1000
            ? QStringLiteral("%1k").arg(freq / 1000.0, 0, 'g', 2)
            : QStringLiteral("%1").arg(freq, 0, 'g', 3);
        col->addWidget(new QLabel(freqLabel, this), 0, Qt::AlignHCenter);

        auto* s = new QSlider(Qt::Vertical, this);
        s->setRange(-12, 12);
        s->setValue(0);
        s->setTickPosition(QSlider::TicksBothSides);
        s->setTickInterval(3);
        connect(s, &QSlider::valueChanged, this, [this, i](int v) {
            if (m_engine) m_engine->setEqualizerBand(i, v);
        });
        col->addWidget(s, 1, Qt::AlignHCenter);
        m_sliders.append(s);
        slidersRow->addLayout(col);
    }
    root->addLayout(slidersRow, 1);

    connect(m_enable, &QCheckBox::toggled, this, [this](bool on) {
        if (m_engine) m_engine->setEqualizerEnabled(on);
    });
    connect(m_presetBox, &QComboBox::currentTextChanged,
            this, [this](const QString& name) {
        if (m_engine && !name.isEmpty()) m_engine->setEqualizerPreset(name);
    });
}

void EqualizerWidget::loadPresetList() {
    m_presetBox->clear();
    QDir dir(QStringLiteral(":/resources/eq_presets"));
    if (!dir.exists()) dir = QDir(QStringLiteral("resources/eq_presets"));
    for (const QString& f : dir.entryList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile fh(dir.filePath(f));
        if (!fh.open(QIODevice::ReadOnly)) continue;
        const auto doc = QJsonDocument::fromJson(fh.readAll());
        const QString name = doc.object().value(QStringLiteral("name"))
                                .toString(QFileInfo(f).completeBaseName());
        m_presetBox->addItem(name);
    }
    if (m_presetBox->count() == 0) m_presetBox->addItem(QStringLiteral("flat"));
}

void EqualizerWidget::attachEngine(PlayerEngine* engine) {
    m_engine = engine;
    if (!engine) return;
    setBandGains(engine->equalizerGains());
    m_enable->setChecked(engine->equalizerEnabled());
}

void EqualizerWidget::setBandGains(const QVector<double>& gains) {
    for (int i = 0; i < m_sliders.size() && i < gains.size(); ++i) {
        QSignalBlocker b(m_sliders[i]);
        m_sliders[i]->setValue(static_cast<int>(gains[i]));
    }
}

QVector<double> EqualizerWidget::bandGains() const {
    QVector<double> out;
    out.reserve(m_sliders.size());
    for (auto* s : m_sliders) out.append(s->value());
    return out;
}

} // namespace soundshelf
