#pragma once

#include <QWidget>
#include <QList>
#include "soundshelf/core/Track.hpp"

class QLineEdit;
class QSpinBox;
class QCheckBox;
class QPushButton;

namespace soundshelf {

/**
 * @brief Bulk tag editor — apply common values to many tracks at once.
 *
 * Each editable field is paired with a checkbox that controls whether
 * the field is included in the apply step. Only the *checked* fields
 * are written back; unchecked fields preserve the current per-track
 * tag value.
 */
class BatchTagEditor : public QWidget {
    Q_OBJECT
public:
    /// Field set requested by the user.
    struct Patch {
        bool changeArtist = false;       QString artist;
        bool changeAlbum  = false;       QString album;
        bool changeGenre  = false;       QString genre;
        bool changeYear   = false;       int     year = 0;
        bool changeAlbumArtist = false;  QString albumArtist;
    };

    explicit BatchTagEditor(QWidget* parent = nullptr);
    ~BatchTagEditor() override;

    /// Loads @p tracks. The widget pre-fills fields with values that
    /// are common across all selected tracks.
    void setTracks(const QList<Track>& tracks);

signals:
    void applyRequested(const QList<Track>& tracks, const Patch& p);

private:
    QList<Track> m_tracks;
    QCheckBox*   m_chkArtist = nullptr;       QLineEdit* m_artist = nullptr;
    QCheckBox*   m_chkAlbum = nullptr;        QLineEdit* m_album = nullptr;
    QCheckBox*   m_chkGenre = nullptr;        QLineEdit* m_genre = nullptr;
    QCheckBox*   m_chkYear = nullptr;         QSpinBox*  m_year = nullptr;
    QCheckBox*   m_chkAlbumArtist = nullptr;  QLineEdit* m_albumArtist = nullptr;
    QPushButton* m_apply = nullptr;
};

} // namespace soundshelf
