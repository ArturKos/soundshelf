#include "soundshelf/ui/BatchTagEditor.hpp"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

namespace soundshelf {

namespace {

template <typename Getter>
QString commonValue(const QList<Track>& tracks, Getter g) {
    if (tracks.isEmpty()) return {};
    const QString first = g(tracks.first());
    for (const auto& t : tracks) if (g(t) != first) return {};
    return first;
}

QPair<QCheckBox*, QLineEdit*> textRow(QWidget* parent, const QString& label) {
    auto* chk = new QCheckBox(label, parent);
    auto* edit = new QLineEdit(parent);
    edit->setEnabled(false);
    QObject::connect(chk, &QCheckBox::toggled, edit, &QLineEdit::setEnabled);
    return { chk, edit };
}

} // namespace

BatchTagEditor::BatchTagEditor(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* form = new QFormLayout;
    auto [chkA, edA] = textRow(this, tr("Artist"));
    m_chkArtist = chkA; m_artist = edA;
    form->addRow(m_chkArtist, m_artist);

    auto [chkAlb, edAlb] = textRow(this, tr("Album"));
    m_chkAlbum = chkAlb; m_album = edAlb;
    form->addRow(m_chkAlbum, m_album);

    auto [chkAA, edAA] = textRow(this, tr("Album artist"));
    m_chkAlbumArtist = chkAA; m_albumArtist = edAA;
    form->addRow(m_chkAlbumArtist, m_albumArtist);

    auto [chkG, edG] = textRow(this, tr("Genre"));
    m_chkGenre = chkG; m_genre = edG;
    form->addRow(m_chkGenre, m_genre);

    m_chkYear = new QCheckBox(tr("Year"), this);
    m_year = new QSpinBox(this);
    m_year->setRange(0, 9999);
    m_year->setEnabled(false);
    connect(m_chkYear, &QCheckBox::toggled, m_year, &QSpinBox::setEnabled);
    form->addRow(m_chkYear, m_year);
    root->addLayout(form);

    m_apply = new QPushButton(tr("Apply to selection"), this);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(m_apply);
    root->addLayout(row);

    connect(m_apply, &QPushButton::clicked, this, [this]() {
        Patch p;
        p.changeArtist      = m_chkArtist->isChecked();      p.artist      = m_artist->text();
        p.changeAlbum       = m_chkAlbum->isChecked();       p.album       = m_album->text();
        p.changeAlbumArtist = m_chkAlbumArtist->isChecked(); p.albumArtist = m_albumArtist->text();
        p.changeGenre       = m_chkGenre->isChecked();       p.genre       = m_genre->text();
        p.changeYear        = m_chkYear->isChecked();        p.year        = m_year->value();
        emit applyRequested(m_tracks, p);
    });
}

BatchTagEditor::~BatchTagEditor() = default;

void BatchTagEditor::setTracks(const QList<Track>& tracks) {
    m_tracks = tracks;
    m_artist->setText      (commonValue(tracks, [](const Track& t) { return t.artist; }));
    m_album->setText       (commonValue(tracks, [](const Track& t) { return t.album;  }));
    m_albumArtist->setText (commonValue(tracks, [](const Track& t) { return t.albumArtist; }));
    m_genre->setText       (commonValue(tracks, [](const Track& t) { return t.genre;  }));
    int yr = -1;
    for (const auto& t : tracks) {
        if (yr < 0) yr = t.year;
        else if (yr != t.year) { yr = 0; break; }
    }
    m_year->setValue(qMax(0, yr));
}

} // namespace soundshelf
