#include "soundshelf/network/MusicBrainzClient.hpp"

#include <QUrl>
#include <QUrlQuery>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMb, "soundshelf.network.musicbrainz")

namespace soundshelf {

namespace {
const QUrl kBase{ QStringLiteral("https://musicbrainz.org/ws/2") };
} // namespace

MusicBrainzClient::MusicBrainzClient(QObject* parent)
    : QObject(parent)
{
    m_rest.setRateLimit(1.0);   // anonymous tier
    m_rest.setUserAgent(QStringLiteral(
        "SoundShelf/0.3 (https://github.com/ArturKos/soundshelf)"));
}

MusicBrainzClient::~MusicBrainzClient() = default;

QFuture<Result<QJsonDocument>> MusicBrainzClient::lookupDiscId(const QString& discId) {
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("inc"), QStringLiteral("artist-credits+recordings"));
    q.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    return m_rest.getJson(kBase, QStringLiteral("/discid/") + discId, q);
}

QFuture<Result<QJsonDocument>> MusicBrainzClient::lookupRelease(const QString& releaseMbid) {
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("inc"),
        QStringLiteral("artists+recordings+release-groups+labels+media"));
    q.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    return m_rest.getJson(kBase, QStringLiteral("/release/") + releaseMbid, q);
}

QFuture<Result<QJsonDocument>>
MusicBrainzClient::searchRelease(const QString& artist, const QString& album, int limit) {
    QUrlQuery q;
    const QString lucene = QStringLiteral("artist:\"%1\" AND release:\"%2\"")
        .arg(artist, album);
    q.addQueryItem(QStringLiteral("query"), lucene);
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    q.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    return m_rest.getJson(kBase, QStringLiteral("/release/"), q);
}

} // namespace soundshelf
