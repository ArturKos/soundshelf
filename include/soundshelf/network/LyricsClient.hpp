#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QJsonDocument>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/**
 * @brief Client for the LRCLib lyrics service (`https://lrclib.net/api`).
 *
 * Returns plain and synchronised lyrics by `(artist, title, album,
 * durationSec)` quadruple. The duration is essential — LRCLib uses it
 * to disambiguate cover versions and live recordings.
 *
 * Anonymous, no API key required. The service asks for a polite
 * `User-Agent` identifying the application.
 *
 * The result is the raw JSON document returned by LRCLib — relevant
 * fields are `plainLyrics` (string), `syncedLyrics` (string, LRC
 * format) and `instrumental` (bool). @ref decode is a small helper
 * that extracts a strongly-typed @ref Lyrics struct.
 */
class LyricsClient : public QObject {
    Q_OBJECT
public:
    /// Plain + synced text extracted from LRCLib's response.
    struct Lyrics {
        QString plain;
        QString synced;
        QString source = QStringLiteral("lrclib");
    };

    explicit LyricsClient(QObject* parent = nullptr);
    ~LyricsClient() override;

    /// Looks up lyrics by metadata. @p durationSec must match within
    /// ~2 s for LRCLib to return a hit.
    QFuture<Result<QJsonDocument>> getLyrics(const QString& artist,
                                             const QString& title,
                                             const QString& album,
                                             int durationSec);

    /// Decodes a LRCLib `/get` response into the strongly-typed struct.
    static Lyrics decode(const QJsonDocument& doc);

private:
    RestClient m_rest;
};

} // namespace soundshelf
