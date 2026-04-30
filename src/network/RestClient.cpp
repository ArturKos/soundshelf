#include "soundshelf/network/RestClient.hpp"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcRest, "soundshelf.network.rest")

namespace soundshelf {

RestClient::RestClient(QObject* parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this)),
      m_userAgent(QStringLiteral("SoundShelf/0.3 (https://github.com/ArturKos/soundshelf)"))
{}

RestClient::~RestClient() = default;

void RestClient::setRateLimit(double reqsPerSecond) {
    m_rateLimit = qMax(0.1, reqsPerSecond);
}

void RestClient::throttle() {
    const qint64 minSpacingMs = static_cast<qint64>(1000.0 / m_rateLimit);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 diff = now - m_lastSentAtMs;
    if (m_lastSentAtMs > 0 && diff < minSpacingMs) {
        QThread::msleep(static_cast<unsigned long>(minSpacingMs - diff));
    }
    m_lastSentAtMs = QDateTime::currentMSecsSinceEpoch();
}

namespace {

QUrl join(const QUrl& base, const QString& path, const QUrlQuery& q) {
    QUrl u = base;
    QString joined = base.path();
    if (!joined.endsWith(QLatin1Char('/')) && !path.startsWith(QLatin1Char('/'))) {
        joined += QLatin1Char('/');
    }
    joined += path;
    u.setPath(joined);
    if (!q.isEmpty()) u.setQuery(q);
    return u;
}

Result<QJsonDocument> jsonFromReply(QNetworkReply* reply) {
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        return Result<QJsonDocument>::err(Error::NetworkError,
            QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString()));
    }
    if (status >= 400) {
        return Result<QJsonDocument>::err(Error::NetworkError,
            QStringLiteral("HTTP %1: %2").arg(status).arg(QString::fromUtf8(body.left(256))));
    }
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError) {
        return Result<QJsonDocument>::err(Error::InvalidFormat,
            QStringLiteral("JSON parse: %1").arg(err.errorString()));
    }
    return Result<QJsonDocument>::ok(std::move(doc));
}

} // namespace

QFuture<Result<QJsonDocument>>
RestClient::getJson(const QUrl& baseUrl, const QString& path, const QUrlQuery& params) {
    auto* promise = new QPromise<Result<QJsonDocument>>;
    promise->start();
    auto fut = promise->future();

    throttle();
    QNetworkRequest req(join(baseUrl, path, params));
    req.setRawHeader("User-Agent", m_userAgent.toUtf8());
    req.setRawHeader("Accept", "application/json");
    sendRequestWithRetry(req, 0, promise);
    return fut;
}

QFuture<Result<QJsonDocument>>
RestClient::postJson(const QUrl& baseUrl,
                     const QString& path,
                     const QByteArray& body,
                     const QString& contentType) {
    auto* promise = new QPromise<Result<QJsonDocument>>;
    promise->start();
    auto fut = promise->future();

    throttle();
    QNetworkRequest req(join(baseUrl, path, {}));
    req.setRawHeader("User-Agent", m_userAgent.toUtf8());
    req.setRawHeader("Content-Type", contentType.toUtf8());
    req.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [promise, reply]() {
        promise->addResult(jsonFromReply(reply));
        promise->finish();
        reply->deleteLater();
        delete promise;
    });
    return fut;
}

QFuture<Result<QByteArray>>
RestClient::getBytes(const QUrl& baseUrl, const QString& path, const QUrlQuery& params) {
    auto* promise = new QPromise<Result<QByteArray>>;
    promise->start();
    auto fut = promise->future();

    throttle();
    QNetworkRequest req(join(baseUrl, path, params));
    req.setRawHeader("User-Agent", m_userAgent.toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [promise, reply]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            promise->addResult(Result<QByteArray>::err(Error::NetworkError,
                QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString())));
        } else if (status >= 400) {
            promise->addResult(Result<QByteArray>::err(Error::NetworkError,
                QStringLiteral("HTTP %1").arg(status)));
        } else {
            promise->addResult(Result<QByteArray>::ok(reply->readAll()));
        }
        promise->finish();
        reply->deleteLater();
        delete promise;
    });
    return fut;
}

void RestClient::sendRequestWithRetry(QNetworkRequest req,
                                      int attempt,
                                      QPromise<Result<QJsonDocument>>* promise) {
    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, req, attempt, promise]() {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool retryable = (status == 429) || (status >= 500 && status < 600);
        if (retryable && attempt < m_maxRetries) {
            const int backoffMs = 250 * (1 << attempt);  // 250, 500, 1000
            qCWarning(lcRest) << "HTTP" << status << "retry in" << backoffMs << "ms";
            reply->deleteLater();
            QTimer::singleShot(backoffMs, this, [this, req, attempt, promise]() {
                sendRequestWithRetry(req, attempt + 1, promise);
            });
            return;
        }
        promise->addResult(jsonFromReply(reply));
        promise->finish();
        reply->deleteLater();
        delete promise;
    });
}

} // namespace soundshelf
