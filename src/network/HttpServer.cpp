#include "soundshelf/network/HttpServer.hpp"
#include "soundshelf/network/HttpRange.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

#ifdef SOUNDSHELF_HAVE_HTTPSERVER
#  include <QHttpServer>
#  include <QHttpServerResponse>
#  include <QHttpServerRequest>
#  include <QTcpServer>
#endif

Q_LOGGING_CATEGORY(lcHttp, "soundshelf.network.http")

namespace soundshelf {

struct HttpServer::Impl {
#ifdef SOUNDSHELF_HAVE_HTTPSERVER
    QHttpServer server;
#endif
};

HttpServer::HttpServer(QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::isAvailable() {
#ifdef SOUNDSHELF_HAVE_HTTPSERVER
    return true;
#else
    return false;
#endif
}

#ifdef SOUNDSHELF_HAVE_HTTPSERVER
namespace {

bool authorise(const QHttpServerRequest& req, const QString& expected) {
    if (expected.isEmpty()) return false;
    const QByteArray authHdr = req.value(QByteArrayLiteral("Authorization"));
    if (authHdr.startsWith("Bearer ")
        && authHdr.mid(7).trimmed() == expected.toUtf8()) return true;
    const QString tokenParam = req.query().queryItemValue(QStringLiteral("token"));
    return tokenParam == expected;
}

QJsonObject trackToJson(const Track& t) {
    QJsonObject o;
    o[QStringLiteral("id")]          = t.id;
    o[QStringLiteral("title")]       = t.title;
    o[QStringLiteral("artist")]      = t.artist;
    o[QStringLiteral("album")]       = t.album;
    o[QStringLiteral("duration_ms")] = t.durationMs;
    o[QStringLiteral("track_no")]    = t.trackNumber;
    o[QStringLiteral("disc_no")]     = t.discNumber;
    o[QStringLiteral("path")]        = t.filepath;
    return o;
}

} // namespace
#endif

Result<void> HttpServer::start(const QHostAddress& host, quint16 port) {
#ifndef SOUNDSHELF_HAVE_HTTPSERVER
    Q_UNUSED(host); Q_UNUSED(port);
    return Result<void>::err(Error::DependencyMissing,
        QStringLiteral("Qt6::HttpServer not available"));
#else
    if (m_running) return Result<void>::ok();

    auto& server = d->server;

    server.route(QStringLiteral("/api/v1/tracks"),
        [this](const QHttpServerRequest& req) {
            if (!authorise(req, m_token)) {
                return QHttpServerResponse(
                    QHttpServerResponse::StatusCode::Unauthorized);
            }
            const QString q = req.query().queryItemValue(QStringLiteral("q"));
            const int limit = req.query()
                .queryItemValue(QStringLiteral("limit")).toInt();
            auto& dbm = DatabaseManager::instance();
            auto r = q.isEmpty()
                ? dbm.listTracks(limit > 0 ? limit : 100, 0)
                : dbm.searchTracks(q, limit > 0 ? limit : 100);
            if (!r) {
                return QHttpServerResponse(
                    QJsonObject {{ QStringLiteral("error"), r.error().message }},
                    QHttpServerResponse::StatusCode::InternalServerError);
            }
            QJsonArray arr;
            for (const auto& t : r.value()) arr.append(trackToJson(t));
            return QHttpServerResponse(arr);
        });

    server.route(QStringLiteral("/api/v1/tracks/<arg>"),
        [this](int id, const QHttpServerRequest& req) {
            if (!authorise(req, m_token)) {
                return QHttpServerResponse(
                    QHttpServerResponse::StatusCode::Unauthorized);
            }
            auto r = DatabaseManager::instance().getTrack(id);
            if (!r) {
                return QHttpServerResponse(
                    QHttpServerResponse::StatusCode::NotFound);
            }
            return QHttpServerResponse(trackToJson(r.value()));
        });

    server.route(QStringLiteral("/api/v1/stream/<arg>"),
        [this](int id, const QHttpServerRequest& req) -> QHttpServerResponse {
            if (!authorise(req, m_token)) {
                return QHttpServerResponse(
                    QHttpServerResponse::StatusCode::Unauthorized);
            }
            auto r = DatabaseManager::instance().getTrack(id);
            if (!r) {
                return QHttpServerResponse(
                    QHttpServerResponse::StatusCode::NotFound);
            }
            QFile f(r.value().filepath);
            if (!f.open(QIODevice::ReadOnly)) {
                return QHttpServerResponse(
                    QHttpServerResponse::StatusCode::Forbidden);
            }

            using SC = QHttpServerResponse::StatusCode;
            const qint64 totalSize = f.size();
            const QByteArray rangeHdr = req.value(QByteArrayLiteral("Range"));
            const RangeResult rr = HttpRange::parse(rangeHdr, totalSize);

            if (rr.status == RangeStatus::Unsatisfiable) {
                QHttpServerResponse resp(static_cast<SC>(416));
                resp.addHeader("Content-Range",
                    HttpRange::unsatisfiedContentRange(totalSize));
                return resp;
            }

            if (rr.status == RangeStatus::Satisfiable) {
                if (!f.seek(rr.range.start)) {
                    return QHttpServerResponse(SC::InternalServerError);
                }
                const QByteArray chunk = f.read(rr.range.length());
                QHttpServerResponse resp(
                    QByteArrayLiteral("audio/octet-stream"),
                    chunk,
                    static_cast<SC>(206));
                resp.addHeader("Content-Range",
                    HttpRange::contentRange(rr.range, totalSize));
                resp.addHeader("Accept-Ranges", QByteArrayLiteral("bytes"));
                return resp;
            }

            // RangeStatus::None or RangeStatus::Malformed: full 200 with Accept-Ranges.
            const QByteArray bytes = f.readAll();
            QHttpServerResponse resp(
                QByteArrayLiteral("audio/octet-stream"), bytes);
            resp.addHeader("Accept-Ranges", QByteArrayLiteral("bytes"));
            return resp;
        });

    auto* tcp = new QTcpServer(&server);
    if (!tcp->listen(host, port)) {
        const QString err = tcp->errorString();
        delete tcp;
        return Result<void>::err(Error::NetworkError,
            QStringLiteral("Cannot listen on %1:%2 — %3")
                .arg(host.toString()).arg(port).arg(err));
    }
    if (!server.bind(tcp)) {
        return Result<void>::err(Error::NetworkError,
            QStringLiteral("QHttpServer::bind failed"));
    }
    m_running = true;
    qCInfo(lcHttp) << "HTTP server listening on" << host.toString() << port;
    emit started(port);
    return Result<void>::ok();
#endif
}

void HttpServer::stop() {
#ifdef SOUNDSHELF_HAVE_HTTPSERVER
    if (!m_running) return;
    // QHttpServer has no clean stop API in Qt 6.4; deleting the
    // backing Impl drops the QTcpServer it owned.
    d.reset(new Impl);
    m_running = false;
    emit stopped();
#endif
}

} // namespace soundshelf
