#include "soundshelf/network/LyricsClient.hpp"

#include <QUrl>
#include <QUrlQuery>
#include <QJsonObject>

namespace soundshelf {

namespace {
const QUrl kBase{ QStringLiteral("https://lrclib.net/api") };
} // namespace

LyricsClient::LyricsClient(QObject* parent) : QObject(parent) {
    m_rest.setRateLimit(3.0);
}
LyricsClient::~LyricsClient() = default;

QFuture<Result<QJsonDocument>>
LyricsClient::getLyrics(const QString& artist,
                        const QString& title,
                        const QString& album,
                        int durationSec) {
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("artist_name"), artist);
    q.addQueryItem(QStringLiteral("track_name"),  title);
    q.addQueryItem(QStringLiteral("album_name"),  album);
    q.addQueryItem(QStringLiteral("duration"),    QString::number(durationSec));
    return m_rest.getJson(kBase, QStringLiteral("/get"), q);
}

LyricsClient::Lyrics LyricsClient::decode(const QJsonDocument& doc) {
    const QJsonObject obj = doc.object();
    Lyrics l;
    l.plain  = obj.value(QStringLiteral("plainLyrics")).toString();
    l.synced = obj.value(QStringLiteral("syncedLyrics")).toString();
    return l;
}

} // namespace soundshelf
