#pragma once

#include <QObject>
#include <QString>
#include <QHostAddress>
#include <memory>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Headless mode REST server (Qt 6.4+ `QHttpServer`).
 *
 * Exposes the routes consumed by @ref NetworkLibrary :
 *  - `GET  /api/v1/tracks?q=...&limit=...`
 *  - `GET  /api/v1/tracks/<id>`
 *  - `GET  /api/v1/discs`
 *  - `GET  /api/v1/stream/<id>` — streams audio bytes from disk
 *  - `POST /api/v1/player/play` — control endpoints
 *
 * Authentication is bearer-token only — every request must carry a
 * matching `token=` query parameter (or `Authorization: Bearer ...`
 * header). The token is generated at first start and stored in
 * SettingsManager.
 *
 * On builds without `Qt6::HttpServer` (`SOUNDSHELF_HAVE_HTTPSERVER`
 * undefined) the class compiles to no-op stubs so the rest of the
 * application still links.
 */
class HttpServer : public QObject {
    Q_OBJECT
public:
    explicit HttpServer(QObject* parent = nullptr);
    ~HttpServer() override;

    /// Static auth token configured at startup.
    void setBearerToken(const QString& token) { m_token = token; }

    /// Starts listening on @p host:@p port. Idempotent.
    Result<void> start(const QHostAddress& host = QHostAddress::AnyIPv4,
                       quint16 port = 8080);

    /// Stops the server.
    void stop();

    bool isRunning() const { return m_running; }

    /// True if the build has Qt6::HttpServer support.
    static bool isAvailable();

signals:
    void started(quint16 port);
    void stopped();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    QString m_token;
    bool    m_running = false;
};

} // namespace soundshelf
