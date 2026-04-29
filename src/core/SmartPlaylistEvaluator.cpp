#include "soundshelf/core/SmartPlaylistEvaluator.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSmart, "soundshelf.smart")

namespace soundshelf {

namespace {

/// Mapowanie nazwy pola na kolumnę SQL (lub join expression).
struct FieldMapping {
    QString sqlExpr;
    bool isString = false;
    bool isNumeric = false;
    bool isDate = false;
    bool isList = false;       ///< np. format IN (...)
};

FieldMapping mapField(const QString& field) {
    FieldMapping m;
    if (field == QLatin1String("title"))       { m.sqlExpr = QStringLiteral("t.title");                       m.isString = true;  return m; }
    if (field == QLatin1String("artist"))      { m.sqlExpr = QStringLiteral("a.name");                        m.isString = true;  return m; }
    if (field == QLatin1String("album"))       { m.sqlExpr = QStringLiteral("d.title");                       m.isString = true;  return m; }
    if (field == QLatin1String("genre"))       { m.sqlExpr = QStringLiteral("g.name");                        m.isString = true;  return m; }
    if (field == QLatin1String("year"))        { m.sqlExpr = QStringLiteral("t.year");                        m.isNumeric = true; return m; }
    if (field == QLatin1String("bitrate"))     { m.sqlExpr = QStringLiteral("t.bitrate");                     m.isNumeric = true; return m; }
    if (field == QLatin1String("duration_ms")) { m.sqlExpr = QStringLiteral("t.duration_ms");                 m.isNumeric = true; return m; }
    if (field == QLatin1String("play_count"))  { m.sqlExpr = QStringLiteral("t.play_count");                  m.isNumeric = true; return m; }
    if (field == QLatin1String("rating"))      { m.sqlExpr = QStringLiteral("t.rating");                      m.isNumeric = true; return m; }
    if (field == QLatin1String("format"))      { m.sqlExpr = QStringLiteral("t.format");                      m.isString = true; m.isList = true; return m; }
    if (field == QLatin1String("added_at"))    { m.sqlExpr = QStringLiteral("t.added_at");                    m.isDate = true;    return m; }
    if (field == QLatin1String("last_played")) { m.sqlExpr = QStringLiteral("t.last_played");                 m.isDate = true;    return m; }
    if (field == QLatin1String("track_gain"))  { m.sqlExpr = QStringLiteral("t.rg_track_gain");               m.isNumeric = true; return m; }
    return m;
}

QString opToSqlNumeric(const QString& op) {
    if (op == QLatin1String("eq"))   return QStringLiteral("=");
    if (op == QLatin1String("neq"))  return QStringLiteral("!=");
    if (op == QLatin1String("lt"))   return QStringLiteral("<");
    if (op == QLatin1String("lte"))  return QStringLiteral("<=");
    if (op == QLatin1String("gt"))   return QStringLiteral(">");
    if (op == QLatin1String("gte"))  return QStringLiteral(">=");
    return {};
}

} // anonymous

QStringList SmartPlaylistEvaluator::supportedFields() {
    return {
        QStringLiteral("title"),
        QStringLiteral("artist"),
        QStringLiteral("album"),
        QStringLiteral("genre"),
        QStringLiteral("year"),
        QStringLiteral("bitrate"),
        QStringLiteral("duration_ms"),
        QStringLiteral("play_count"),
        QStringLiteral("rating"),
        QStringLiteral("format"),
        QStringLiteral("added_at"),
        QStringLiteral("last_played"),
        QStringLiteral("track_gain"),
    };
}

QStringList SmartPlaylistEvaluator::operatorsFor(const QString& field) {
    const auto m = mapField(field);
    if (m.isString && !m.isList) return { "eq", "neq", "contains", "starts_with", "ends_with" };
    if (m.isString && m.isList)  return { "in", "not_in" };
    if (m.isNumeric)             return { "eq", "neq", "lt", "lte", "gt", "gte", "between" };
    if (m.isDate)                return { "in_last", "before", "after" };
    return {};
}

bool SmartPlaylistEvaluator::validate(const QJsonObject& rules, QString* errorMsg) {
    auto fail = [errorMsg](const QString& msg) {
        if (errorMsg) *errorMsg = msg;
        return false;
    };
    if (!rules.contains("rules") || !rules["rules"].isArray())
        return fail(QStringLiteral("Missing 'rules' array"));

    const auto match = rules.value("match").toString(QStringLiteral("all"));
    if (match != QLatin1String("all") && match != QLatin1String("any"))
        return fail(QStringLiteral("'match' must be 'all' or 'any'"));

    const auto arr = rules["rules"].toArray();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        const QString field = o.value("field").toString();
        const QString op    = o.value("op").toString();
        if (field.isEmpty() || op.isEmpty())
            return fail(QStringLiteral("Rule missing 'field' or 'op'"));
        if (mapField(field).sqlExpr.isEmpty())
            return fail(QStringLiteral("Unknown field: %1").arg(field));
    }
    return true;
}

Result<SmartPlaylistEvaluator::CompiledQuery>
SmartPlaylistEvaluator::compile(const QJsonObject& rules) {
    QString err;
    if (!validate(rules, &err)) {
        return Result<CompiledQuery>::err(Error::InvalidArgument, err);
    }

    const QString matchKw = rules.value("match").toString(QStringLiteral("all"))
                            == QLatin1String("any") ? QStringLiteral(" OR ")
                                                    : QStringLiteral(" AND ");

    const auto rulesArr = rules["rules"].toArray();
    QStringList whereParts;
    QVariantList params;

    for (const auto& v : rulesArr) {
        const auto r = v.toObject();
        const QString field = r["field"].toString();
        const QString op    = r["op"].toString();
        const QJsonValue val = r["value"];
        const auto m = mapField(field);

        // String operators
        if (m.isString) {
            if (op == QLatin1String("contains")) {
                whereParts << QStringLiteral("%1 LIKE ?").arg(m.sqlExpr);
                params << QStringLiteral("%%%1%%").arg(val.toString());
            } else if (op == QLatin1String("starts_with")) {
                whereParts << QStringLiteral("%1 LIKE ?").arg(m.sqlExpr);
                params << QStringLiteral("%1%%").arg(val.toString());
            } else if (op == QLatin1String("ends_with")) {
                whereParts << QStringLiteral("%1 LIKE ?").arg(m.sqlExpr);
                params << QStringLiteral("%%%1").arg(val.toString());
            } else if (op == QLatin1String("eq")) {
                whereParts << QStringLiteral("%1 = ?").arg(m.sqlExpr);
                params << val.toString();
            } else if (op == QLatin1String("neq")) {
                whereParts << QStringLiteral("%1 != ?").arg(m.sqlExpr);
                params << val.toString();
            } else if (op == QLatin1String("in") || op == QLatin1String("not_in")) {
                if (!val.isArray()) {
                    return Result<CompiledQuery>::err(Error::InvalidArgument,
                        QStringLiteral("'in' requires array value"));
                }
                const auto arr = val.toArray();
                QStringList placeholders;
                for (const auto& item : arr) {
                    placeholders << QStringLiteral("?");
                    params << item.toString();
                }
                const QString notKw = (op == QLatin1String("not_in"))
                    ? QStringLiteral("NOT ") : QString();
                whereParts << QStringLiteral("%1 %2IN (%3)")
                    .arg(m.sqlExpr, notKw, placeholders.join(", "));
            }
            continue;
        }

        // Numeric operators
        if (m.isNumeric) {
            if (op == QLatin1String("between")) {
                if (!val.isArray() || val.toArray().size() != 2) {
                    return Result<CompiledQuery>::err(Error::InvalidArgument,
                        QStringLiteral("'between' needs [min, max]"));
                }
                whereParts << QStringLiteral("%1 BETWEEN ? AND ?").arg(m.sqlExpr);
                params << val.toArray()[0].toVariant() << val.toArray()[1].toVariant();
            } else {
                const QString sqlOp = opToSqlNumeric(op);
                if (sqlOp.isEmpty()) {
                    return Result<CompiledQuery>::err(Error::InvalidArgument,
                        QStringLiteral("Unsupported numeric op: %1").arg(op));
                }
                whereParts << QStringLiteral("%1 %2 ?").arg(m.sqlExpr, sqlOp);
                params << val.toVariant();
            }
            continue;
        }

        // Date operators
        if (m.isDate) {
            if (op == QLatin1String("in_last")) {
                const QString s = val.toString();    // np. "180d", "7d", "1y"
                whereParts << QStringLiteral("%1 >= datetime('now', ?)").arg(m.sqlExpr);
                params << QStringLiteral("-%1").arg(
                    s.replace(QStringLiteral("d"), QStringLiteral(" days"))
                     .replace(QStringLiteral("y"), QStringLiteral(" years"))
                     .replace(QStringLiteral("m"), QStringLiteral(" months")));
            } else if (op == QLatin1String("before")) {
                whereParts << QStringLiteral("%1 < ?").arg(m.sqlExpr);
                params << val.toString();
            } else if (op == QLatin1String("after")) {
                whereParts << QStringLiteral("%1 > ?").arg(m.sqlExpr);
                params << val.toString();
            }
            continue;
        }
    }

    QString sql = QStringLiteral(
        "SELECT t.* FROM tracks t "
        "LEFT JOIN artists a ON a.id = t.artist_id "
        "LEFT JOIN discs d   ON d.id = t.disc_id "
        "LEFT JOIN genres g  ON g.id = t.genre_id "
        "WHERE t.missing = 0");

    if (!whereParts.isEmpty()) {
        sql += QStringLiteral(" AND (") + whereParts.join(matchKw) + QStringLiteral(")");
    }

    // ORDER BY
    const QString orderBy = rules.value("order_by").toString();
    if (orderBy == QLatin1String("random"))      sql += QStringLiteral(" ORDER BY RANDOM()");
    else if (orderBy == QLatin1String("year"))   sql += QStringLiteral(" ORDER BY t.year DESC");
    else if (orderBy == QLatin1String("added"))  sql += QStringLiteral(" ORDER BY t.added_at DESC");
    else if (orderBy == QLatin1String("title"))  sql += QStringLiteral(" ORDER BY t.title");
    else if (orderBy == QLatin1String("plays"))  sql += QStringLiteral(" ORDER BY t.play_count DESC");

    // LIMIT
    if (rules.contains("limit")) {
        const int lim = rules["limit"].toInt();
        if (lim > 0) {
            sql += QStringLiteral(" LIMIT ?");
            params << lim;
        }
    }

    qCDebug(lcSmart) << "Compiled smart playlist:" << sql << "params:" << params;

    return Result<CompiledQuery>::ok({sql, params});
}

} // namespace soundshelf
