#include "soundshelf/network/RemoteClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcRemote, "soundshelf.network.remote")

namespace soundshelf {

RemoteClient::RemoteClient(QString baseUrl, QString token, QObject* parent)
    : QObject(parent)
    , m_baseUrl(QUrl(baseUrl))
    , m_token(std::move(token))
{}

QUrlQuery RemoteClient::buildListQuery(const QString& query, int limit, const QString& token)
{
    QUrlQuery q;
    if (!query.isEmpty())
        q.addQueryItem(QStringLiteral("q"), query);
    if (limit > 0)
        q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    if (!token.isEmpty())
        q.addQueryItem(QStringLiteral("token"), token);
    return q;
}

RemoteTrack RemoteClient::trackFromJson(const QJsonObject& o)
{
    RemoteTrack t;
    t.id          = o.value(QStringLiteral("id")).toInt();
    t.title       = o.value(QStringLiteral("title")).toString();
    t.artist      = o.value(QStringLiteral("artist")).toString();
    t.album       = o.value(QStringLiteral("album")).toString();
    t.durationMs  = o.value(QStringLiteral("duration_ms")).toInteger();
    t.trackNumber = o.value(QStringLiteral("track_no")).toInt();
    t.discNumber  = o.value(QStringLiteral("disc_no")).toInt();
    t.filepath    = o.value(QStringLiteral("path")).toString();
    return t;
}

Result<QList<RemoteTrack>> RemoteClient::parseTrackList(const QByteArray& json)
{
    if (json.isEmpty()) {
        return Result<QList<RemoteTrack>>::err(Error::InvalidFormat,
            QStringLiteral("Empty response"));
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) {
        return Result<QList<RemoteTrack>>::err(Error::InvalidFormat,
            QStringLiteral("JSON parse error: %1").arg(err.errorString()));
    }
    if (!doc.isArray()) {
        return Result<QList<RemoteTrack>>::err(Error::InvalidFormat,
            QStringLiteral("Expected JSON array"));
    }
    QList<RemoteTrack> out;
    const QJsonArray arr = doc.array();
    out.reserve(arr.size());
    for (const QJsonValue& v : arr)
        out.append(trackFromJson(v.toObject()));
    return Result<QList<RemoteTrack>>::ok(std::move(out));
}

Result<RemoteTrack> RemoteClient::parseTrack(const QByteArray& json)
{
    if (json.isEmpty()) {
        return Result<RemoteTrack>::err(Error::InvalidFormat,
            QStringLiteral("Empty response"));
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) {
        return Result<RemoteTrack>::err(Error::InvalidFormat,
            QStringLiteral("JSON parse error: %1").arg(err.errorString()));
    }
    if (!doc.isObject()) {
        return Result<RemoteTrack>::err(Error::InvalidFormat,
            QStringLiteral("Expected JSON object"));
    }
    return Result<RemoteTrack>::ok(trackFromJson(doc.object()));
}

QString RemoteClient::streamUrl(int id) const
{
    QUrl url = m_baseUrl;
    QString p = m_baseUrl.path();
    while (p.endsWith(QLatin1Char('/')))
        p.chop(1);
    url.setPath(p + QStringLiteral("/api/v1/stream/%1").arg(id));
    if (!m_token.isEmpty()) {
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("token"), m_token);
        url.setQuery(q);
    }
    return url.toString();
}

Result<QList<RemoteTrack>> RemoteClient::listTracks(const QString& query, int limit)
{
    qCDebug(lcRemote) << "listTracks query=" << query << "limit=" << limit;
    const QUrlQuery params = buildListQuery(query, limit, m_token);
    const auto r = m_rest.getJson(m_baseUrl, QStringLiteral("/api/v1/tracks"), params).result();
    if (!r) {
        const bool is401 = r.error().message.contains(QStringLiteral("401"));
        return Result<QList<RemoteTrack>>::err(
            is401 ? Error::AuthenticationFailed : Error::NetworkError,
            r.error().message);
    }
    return parseTrackList(r.value().toJson(QJsonDocument::Compact));
}

Result<RemoteTrack> RemoteClient::track(int id)
{
    qCDebug(lcRemote) << "track id=" << id;
    QUrlQuery q;
    if (!m_token.isEmpty())
        q.addQueryItem(QStringLiteral("token"), m_token);
    const auto r = m_rest.getJson(
        m_baseUrl,
        QStringLiteral("/api/v1/tracks/%1").arg(id),
        q).result();
    if (!r) {
        const bool is401 = r.error().message.contains(QStringLiteral("401"));
        return Result<RemoteTrack>::err(
            is401 ? Error::AuthenticationFailed : Error::NetworkError,
            r.error().message);
    }
    return parseTrack(r.value().toJson(QJsonDocument::Compact));
}

} // namespace soundshelf
