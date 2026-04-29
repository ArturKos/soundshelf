#include "soundshelf/data/FTS5Index.hpp"
#include "soundshelf/data/DatabaseManager.hpp"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcFts, "soundshelf.data.fts")

namespace soundshelf {

FTS5Index::FTS5Index(QObject* parent) : QObject(parent) {}
FTS5Index::~FTS5Index() = default;

QString FTS5Index::prepareQuery(const QString& userQuery) {
    QString trimmed = userQuery.trimmed();
    if (trimmed.isEmpty()) return QString();

    static const QRegularExpression ws(QStringLiteral("\\s+"));
    const QStringList tokens = trimmed.split(ws, Qt::SkipEmptyParts);

    QStringList wrapped;
    wrapped.reserve(tokens.size());
    for (int i = 0; i < tokens.size(); ++i) {
        QString tok = tokens[i];
        // Strip characters that have meaning in FTS5 grammar.
        tok.remove(QLatin1Char('"'));
        tok.remove(QLatin1Char('('));
        tok.remove(QLatin1Char(')'));
        tok.remove(QLatin1Char(':'));
        if (tok.isEmpty()) continue;
        // Last token gets a prefix `*` so partial typing matches.
        if (i + 1 == tokens.size()) {
            wrapped << QStringLiteral("\"%1\"*").arg(tok);
        } else {
            wrapped << QStringLiteral("\"%1\"").arg(tok);
        }
    }
    return wrapped.join(QLatin1Char(' '));
}

Result<QList<int>> FTS5Index::searchTrackIds(const QString& userQuery, int limit) {
    const QString fts = prepareQuery(userQuery);
    if (fts.isEmpty()) return Result<QList<int>>::ok({});

    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT rowid FROM tracks_fts WHERE tracks_fts MATCH ? "
        "ORDER BY bm25(tracks_fts) LIMIT ?"));
    q.addBindValue(fts);
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<int>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<int> ids;
    while (q.next()) ids << q.value(0).toInt();
    return Result<QList<int>>::ok(std::move(ids));
}

Result<QList<int>> FTS5Index::searchDiscIds(const QString& userQuery, int limit) {
    const QString fts = prepareQuery(userQuery);
    if (fts.isEmpty()) return Result<QList<int>>::ok({});

    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT rowid FROM discs_fts WHERE discs_fts MATCH ? "
        "ORDER BY bm25(discs_fts) LIMIT ?"));
    q.addBindValue(fts);
    q.addBindValue(limit);
    if (!q.exec()) {
        return Result<QList<int>>::err(Error::DatabaseError, q.lastError().text());
    }
    QList<int> ids;
    while (q.next()) ids << q.value(0).toInt();
    return Result<QList<int>>::ok(std::move(ids));
}

Result<void> FTS5Index::rebuildAll() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("INSERT INTO tracks_fts(tracks_fts) VALUES('rebuild')"))) {
        return Result<void>::err(Error::DatabaseError,
            QStringLiteral("rebuild tracks_fts: %1").arg(q.lastError().text()));
    }
    if (!q.exec(QStringLiteral("INSERT INTO discs_fts(discs_fts) VALUES('rebuild')"))) {
        return Result<void>::err(Error::DatabaseError,
            QStringLiteral("rebuild discs_fts: %1").arg(q.lastError().text()));
    }
    qCInfo(lcFts) << "FTS rebuild complete";
    return Result<void>::ok();
}

Result<QPair<int, int>> FTS5Index::stats() {
    auto db = DatabaseManager::instance().database();
    QSqlQuery q(db);
    int tracks = 0, discs = 0;
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM tracks_fts")) && q.next()) {
        tracks = q.value(0).toInt();
    }
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM discs_fts")) && q.next()) {
        discs = q.value(0).toInt();
    }
    return Result<QPair<int,int>>::ok({tracks, discs});
}

} // namespace soundshelf
