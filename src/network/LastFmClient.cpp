#include "soundshelf/network/LastFmClient.hpp"

#include <QUrl>
#include <QCryptographicHash>
#include <QMap>
#include <QDateTime>
#include <QUrlQuery>

namespace soundshelf {

namespace {
const QUrl kBase{ QStringLiteral("https://ws.audioscrobbler.com/2.0/") };
} // namespace

LastFmClient::LastFmClient(QObject* parent) : QObject(parent) {
    m_rest.setRateLimit(5.0);
}
LastFmClient::~LastFmClient() = default;

void LastFmClient::setApiCredentials(const QString& apiKey, const QString& sharedSecret) {
    m_apiKey = apiKey;
    m_sharedSecret = sharedSecret;
}

QString LastFmClient::signParams(const QMap<QString, QString>& params,
                                 const QString& sharedSecret) {
    QString concat;
    // QMap iterates in key order — exactly what Last.fm wants.
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        concat += it.key();
        concat += it.value();
    }
    concat += sharedSecret;
    return QString::fromUtf8(QCryptographicHash::hash(
        concat.toUtf8(), QCryptographicHash::Md5).toHex());
}

namespace {

QByteArray buildBody(const QMap<QString, QString>& params, const QString& sig) {
    QUrlQuery body;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        body.addQueryItem(it.key(), it.value());
    }
    body.addQueryItem(QStringLiteral("api_sig"), sig);
    body.addQueryItem(QStringLiteral("format"),  QStringLiteral("json"));
    return body.toString(QUrl::FullyEncoded).toUtf8();
}

} // namespace

QFuture<Result<QJsonDocument>> LastFmClient::updateNowPlaying(const Track& t) {
    QMap<QString, QString> p;
    p[QStringLiteral("method")]   = QStringLiteral("track.updateNowPlaying");
    p[QStringLiteral("track")]    = t.title;
    p[QStringLiteral("artist")]   = t.artist;
    p[QStringLiteral("album")]    = t.album;
    p[QStringLiteral("duration")] = QString::number(t.durationMs / 1000);
    p[QStringLiteral("api_key")]  = m_apiKey;
    p[QStringLiteral("sk")]       = m_sk;
    const QString sig = signParams(p, m_sharedSecret);
    return m_rest.postJson(kBase, QString(),
                           buildBody(p, sig),
                           QStringLiteral("application/x-www-form-urlencoded"));
}

QFuture<Result<QJsonDocument>> LastFmClient::scrobble(const Track& t, qint64 timestamp) {
    QMap<QString, QString> p;
    p[QStringLiteral("method")]      = QStringLiteral("track.scrobble");
    p[QStringLiteral("track")]       = t.title;
    p[QStringLiteral("artist")]      = t.artist;
    p[QStringLiteral("album")]       = t.album;
    p[QStringLiteral("duration")]    = QString::number(t.durationMs / 1000);
    p[QStringLiteral("timestamp")]   = QString::number(timestamp);
    p[QStringLiteral("api_key")]     = m_apiKey;
    p[QStringLiteral("sk")]          = m_sk;
    const QString sig = signParams(p, m_sharedSecret);
    return m_rest.postJson(kBase, QString(),
                           buildBody(p, sig),
                           QStringLiteral("application/x-www-form-urlencoded"));
}

} // namespace soundshelf
