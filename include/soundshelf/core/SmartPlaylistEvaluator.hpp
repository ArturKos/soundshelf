#pragma once

#include <QString>
#include <QJsonObject>
#include <QVariantList>
#include "soundshelf/core/Result.hpp"

class QSqlQuery;

namespace soundshelf {

/// Konwertuje JSON z regułami smart playlist na zapytanie SQL.
/// Format JSON udokumentowany w DECISIONS.md ADR-007.
class SmartPlaylistEvaluator {
public:
    static constexpr int SCHEMA_VERSION = 1;

    /// Wynik kompilacji — gotowe SQL z parametrami.
    struct CompiledQuery {
        QString sql;                ///< SELECT t.* FROM tracks t WHERE ...
        QVariantList parameters;    ///< Bind parameters (kolejność jak w sql)
    };

    /// Kompiluje JSON na SQL.
    /// Akceptuje JSON typu:
    ///   { "match": "all"|"any",
    ///     "rules": [ { "field": "...", "op": "...", "value": ... }, ... ],
    ///     "limit": 50,
    ///     "order_by": "random"|"year"|"added"|"title" }
    static Result<CompiledQuery> compile(const QJsonObject& rules);

    /// Sanity-check — czy taki JSON da się skompilować.
    static bool validate(const QJsonObject& rules, QString* errorMsg = nullptr);

    /// Pomocniczo: lista pól obsługiwanych przez UI.
    static QStringList supportedFields();

    /// Pomocniczo: lista operatorów dla danego pola.
    static QStringList operatorsFor(const QString& field);
};

} // namespace soundshelf
