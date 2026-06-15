#include "soundshelf/data/SchemaMigrator.hpp"

#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcMigrate, "soundshelf.migrate")

namespace soundshelf {

SchemaMigrator::SchemaMigrator(QSqlDatabase& db) : m_db(db) {}

QStringList SchemaMigrator::availableMigrations() {
    return {
        QStringLiteral(":/migrations/001_initial.sql"),
        QStringLiteral(":/migrations/002_replaygain.sql"),
        QStringLiteral(":/migrations/003_acoustid.sql"),
        QStringLiteral(":/migrations/004_smart_playlists.sql"),
        QStringLiteral(":/migrations/005_play_history.sql"),
        QStringLiteral(":/migrations/006_bookmarks.sql"),
    };
}

int SchemaMigrator::currentVersion() {
    // Najpierw sprawdź czy tabela istnieje
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name='schema_version'"))) {
        return 0;
    }
    if (!q.next()) return 0;  // tabela nie istnieje

    if (!q.exec(QStringLiteral("SELECT MAX(version) FROM schema_version"))) {
        return 0;
    }
    if (q.next() && !q.value(0).isNull()) {
        return q.value(0).toInt();
    }
    return 0;
}

Result<void> SchemaMigrator::migrate() {
    const int current = currentVersion();
    qCInfo(lcMigrate) << "Schema at version" << current;

    const QStringList migrations = availableMigrations();
    static const QRegularExpression versionRe(QStringLiteral("(\\d+)_"));

    for (const auto& path : migrations) {
        // Wyciągnij numer wersji z nazwy pliku
        const QString filename = path.section('/', -1);
        const auto match = versionRe.match(filename);
        if (!match.hasMatch()) continue;
        const int v = match.captured(1).toInt();

        if (v <= current) continue;  // już zaaplikowana

        qCInfo(lcMigrate) << "Applying migration" << v << path;
        auto r = applyMigration(path);
        if (!r) {
            qCCritical(lcMigrate) << "Migration" << v << "failed:" << r.error().message;
            return r;
        }
    }

    return Result<void>::ok();
}

Result<void> SchemaMigrator::applyMigration(const QString& resourcePath) {
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return Result<void>::err(Error::FileNotFound,
            QStringLiteral("Migration resource not found: %1").arg(resourcePath));
    }

    const QString sql = QString::fromUtf8(f.readAll());

    // SQLite Qt driver nie wspiera multi-statement queries, musimy podzielić.
    // Dzielimy po średnikach na końcu linii (z ostrożnością wobec stringów).
    // Prosta heurystyka: dzielimy po `;\n` lub `;$`.
    QStringList statements;
    QString current;
    bool inTrigger = false;
    const QStringList lines = sql.split(QChar('\n'));

    for (const auto& line : lines) {
        const QString trimmed = line.trimmed();

        // Skip comment-only and empty lines outside of triggers — otherwise
        // a leading comment block gets bundled with the next CREATE and
        // the whole accumulated statement is later filtered as "starts
        // with --", silently dropping a real statement.
        if (!inTrigger && (trimmed.isEmpty()
                           || trimmed.startsWith(QLatin1String("--")))) {
            continue;
        }

        if (trimmed.startsWith(QLatin1String("CREATE TRIGGER"), Qt::CaseInsensitive)) {
            inTrigger = true;
        }

        current += line + QChar('\n');

        if (inTrigger) {
            if (trimmed.endsWith(QLatin1String("END;"), Qt::CaseInsensitive)) {
                statements << current.trimmed();
                current.clear();
                inTrigger = false;
            }
        } else if (trimmed.endsWith(QChar(';'))) {
            statements << current.trimmed();
            current.clear();
        }
    }
    if (!current.trimmed().isEmpty()) statements << current.trimmed();

    QSqlQuery q(m_db);
    for (const auto& stmt : statements) {
        // Pomijamy komentarze
        if (stmt.startsWith(QLatin1String("--"))) continue;
        if (stmt.isEmpty()) continue;

        if (!q.exec(stmt)) {
            return Result<void>::err(Error::DatabaseError,
                QStringLiteral("Migration SQL failed: %1\nStatement: %2")
                    .arg(q.lastError().text(), stmt.left(200)));
        }
    }

    return Result<void>::ok();
}

} // namespace soundshelf
