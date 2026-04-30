#include "soundshelf/network/ListenBrainzClient.hpp"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QPromise>
#include <QUrl>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcLb, "soundshelf.network.listenbrainz")

namespace soundshelf {

namespace {
const QUrl kBase{ QStringLiteral("https://api.listenbrainz.org") };
} // namespace

ListenBrainzClient::ListenBrainzClient(QObject* parent) : QObject(parent) {
    m_rest.setRateLimit(5.0);
}
ListenBrainzClient::~ListenBrainzClient() = default;

QByteArray ListenBrainzClient::buildEnvelope(const Track& t,
                                             const QString& listenType,
                                             qint64 timestamp) const {
    QJsonObject metadata;
    metadata[QStringLiteral("artist_name")]  = t.artist;
    metadata[QStringLiteral("track_name")]   = t.title;
    if (!t.album.isEmpty()) metadata[QStringLiteral("release_name")] = t.album;
    QJsonObject additional;
    if (t.durationMs > 0) {
        additional[QStringLiteral("duration_ms")] = t.durationMs;
    }
    if (!t.mbRecordingId.isEmpty()) {
        additional[QStringLiteral("recording_mbid")] = t.mbRecordingId;
    }
    metadata[QStringLiteral("additional_info")] = additional;

    QJsonObject payloadEntry;
    if (listenType == QLatin1String("single")) {
        payloadEntry[QStringLiteral("listened_at")] = QJsonValue(qint64(timestamp));
    }
    payloadEntry[QStringLiteral("track_metadata")] = metadata;

    QJsonObject envelope;
    envelope[QStringLiteral("listen_type")] = listenType;
    envelope[QStringLiteral("payload")]     = QJsonArray { payloadEntry };
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

namespace {

QFuture<Result<QJsonDocument>> postWithToken(const QByteArray& body,
                                             const QString& token,
                                             const QString& path,
                                             const QString& userAgent) {
    auto* nam = new QNetworkAccessManager;
    auto* promise = new QPromise<Result<QJsonDocument>>;
    promise->start();
    auto fut = promise->future();

    QNetworkRequest req;
    QUrl u = kBase;
    u.setPath(u.path() + path);
    req.setUrl(u);
    req.setRawHeader("Authorization", QStringLiteral("Token %1").arg(token).toUtf8());
    req.setRawHeader("Content-Type", "application/json");
    req.setRawHeader("User-Agent", userAgent.toUtf8());
    QNetworkReply* reply = nam->post(req, body);
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     [reply, nam, promise]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            promise->addResult(Result<QJsonDocument>::err(Error::NetworkError,
                QStringLiteral("ListenBrainz HTTP %1: %2")
                    .arg(status).arg(reply->errorString())));
        } else {
            QJsonParseError err;
            auto doc = QJsonDocument::fromJson(reply->readAll(), &err);
            promise->addResult(err.error == QJsonParseError::NoError
                ? Result<QJsonDocument>::ok(std::move(doc))
                : Result<QJsonDocument>::err(Error::InvalidFormat, err.errorString()));
        }
        promise->finish();
        reply->deleteLater();
        nam->deleteLater();
        delete promise;
    });
    return fut;
}

} // namespace

QFuture<Result<QJsonDocument>>
ListenBrainzClient::submitListen(const Track& t, qint64 timestamp) {
    return postWithToken(buildEnvelope(t, QStringLiteral("single"), timestamp),
                         m_token,
                         QStringLiteral("/1/submit-listens"),
                         m_rest.userAgent());
}

QFuture<Result<QJsonDocument>>
ListenBrainzClient::playingNow(const Track& t) {
    return postWithToken(buildEnvelope(t, QStringLiteral("playing_now"), 0),
                         m_token,
                         QStringLiteral("/1/submit-listens"),
                         m_rest.userAgent());
}

} // namespace soundshelf
