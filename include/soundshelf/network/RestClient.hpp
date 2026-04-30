#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QByteArray>
#include <QPromise>
#include <QFuture>
#include <QJsonDocument>
#include "soundshelf/core/Result.hpp"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace soundshelf {

/**
 * @brief Base class shared by every HTTPS client in the network layer.
 *
 * Provides:
 *  - a single shared `QNetworkAccessManager`
 *  - a fixed User-Agent string per project policy
 *    (`SoundShelf/0.x (https://...)`)
 *  - simple GET / POST helpers that return `QFuture<Result<QJsonDocument>>`
 *  - a polite per-host rate limiter (defaults to 5 req/s, MusicBrainz
 *    asks for 1 req/s and configures a tighter limit)
 *  - automatic retry with exponential backoff for 429 / 5xx responses
 *
 * Subclasses set the base URL and add the endpoint-specific helpers.
 */
class RestClient : public QObject {
    Q_OBJECT
public:
    explicit RestClient(QObject* parent = nullptr);
    ~RestClient() override;

    /// Override the User-Agent per service if needed.
    void setUserAgent(const QString& ua) { m_userAgent = ua; }
    QString userAgent() const { return m_userAgent; }

    /// Maximum requests per second issued through this client.
    void setRateLimit(double reqsPerSecond);
    double rateLimit() const { return m_rateLimit; }

    /// Number of retry attempts on 429/5xx (exponential backoff).
    void setMaxRetries(int n) { m_maxRetries = n; }

    /// Authoritative HTTPS GET. @p baseUrl is joined with @p path,
    /// then @p params are appended as a URL query.
    QFuture<Result<QJsonDocument>> getJson(const QUrl& baseUrl,
                                           const QString& path,
                                           const QUrlQuery& params = {});

    /// HTTPS POST with JSON body.
    QFuture<Result<QJsonDocument>> postJson(const QUrl& baseUrl,
                                            const QString& path,
                                            const QByteArray& body,
                                            const QString& contentType
                                                = QStringLiteral("application/json"));

    /// HTTPS GET that returns the raw body (used for binary downloads
    /// like cover art).
    QFuture<Result<QByteArray>> getBytes(const QUrl& baseUrl,
                                         const QString& path,
                                         const QUrlQuery& params = {});

protected:
    QNetworkAccessManager* nam() const { return m_nam; }

private:
    void   throttle();
    void   sendRequestWithRetry(QNetworkRequest req,
                                int attempt,
                                QPromise<Result<QJsonDocument>>* promise);

    QNetworkAccessManager* m_nam = nullptr;
    QString m_userAgent;
    double  m_rateLimit  = 5.0;
    int     m_maxRetries = 3;
    qint64  m_lastSentAtMs = 0;
};

} // namespace soundshelf
