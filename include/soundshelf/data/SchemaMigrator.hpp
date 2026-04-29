#pragma once

#include <QString>
#include <QSqlDatabase>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/// Aplikuje migracje SQL z resources (qrc:/migrations/NNN_name.sql).
/// Sprawdza tabelę schema_version i aplikuje brakujące w kolejności.
class SchemaMigrator {
public:
    explicit SchemaMigrator(QSqlDatabase& db);

    /// Sprawdza i aplikuje wszystkie brakujące migracje.
    Result<void> migrate();

    /// Aktualna wersja schematu (0 jeśli pusta baza).
    int currentVersion();

    /// Lista dostępnych migracji w resources.
    static QStringList availableMigrations();

private:
    QSqlDatabase& m_db;

    Result<void> applyMigration(const QString& resourcePath);
};

} // namespace soundshelf
