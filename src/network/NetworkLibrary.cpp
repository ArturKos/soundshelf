#include "soundshelf/network/NetworkLibrary.hpp"

#include <QUrlQuery>

namespace soundshelf {

NetworkLibrary::NetworkLibrary(QObject* parent) : QObject(parent) {}
NetworkLibrary::~NetworkLibrary() = default;

QFuture<Result<QJsonDocument>>
NetworkLibrary::searchTracks(const QString& query, int limit) {
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"),     query);
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    if (!m_token.isEmpty()) q.addQueryItem(QStringLiteral("token"), m_token);
    return m_rest.getJson(m_base, QStringLiteral("/api/v1/tracks"), q);
}

QFuture<Result<QJsonDocument>> NetworkLibrary::getTrack(int id) {
    QUrlQuery q;
    if (!m_token.isEmpty()) q.addQueryItem(QStringLiteral("token"), m_token);
    return m_rest.getJson(m_base,
        QStringLiteral("/api/v1/tracks/%1").arg(id), q);
}

QFuture<Result<QJsonDocument>> NetworkLibrary::listDiscs(int limit) {
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    if (!m_token.isEmpty()) q.addQueryItem(QStringLiteral("token"), m_token);
    return m_rest.getJson(m_base, QStringLiteral("/api/v1/discs"), q);
}

QUrl NetworkLibrary::streamUrl(int trackId) const {
    QUrl u = m_base;
    u.setPath(u.path() + QStringLiteral("/api/v1/stream/%1").arg(trackId));
    if (!m_token.isEmpty()) {
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("token"), m_token);
        u.setQuery(q);
    }
    return u;
}

} // namespace soundshelf
