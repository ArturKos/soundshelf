#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "soundshelf/core/Result.hpp"

namespace soundshelf {

/**
 * @brief Helpers for SoundShelf's SQLite FTS5 indexes.
 *
 * Schema-side, FTS5 indexes are kept in sync by triggers (see
 * `migrations/001_initial.sql`). This class is a thin layer that:
 *  - escapes user input safely (FTS5 has its own grammar with `^`, `*`,
 *    `"..."`, `:`, `AND`, `OR`, `NOT` keywords)
 *  - exposes search helpers returning track / disc IDs
 *  - allows a manual rebuild via `INSERT INTO ..._fts(..._fts) VALUES('rebuild')`
 *
 * Tokenizer is `unicode61 remove_diacritics 2`, so `slowik` matches
 * `słowik` and `oxygene` matches `Oxygène` transparently.
 */
class FTS5Index : public QObject {
    Q_OBJECT
public:
    explicit FTS5Index(QObject* parent = nullptr);
    ~FTS5Index() override;

    /// Escapes/normalises @p userQuery so it cannot inject FTS5 grammar.
    /// Strategy: trim, collapse whitespace, wrap each non-empty token in
    /// double quotes (also escaping internal `"`), then suffix `*` so the
    /// last token is treated as a prefix match. The resulting string is
    /// safe to pass to `MATCH ?`.
    static QString prepareQuery(const QString& userQuery);

    /// Returns track IDs matching @p userQuery, ordered by FTS bm25 rank.
    Result<QList<int>> searchTrackIds(const QString& userQuery, int limit = 100);

    /// Returns disc IDs matching @p userQuery, ordered by FTS bm25 rank.
    Result<QList<int>> searchDiscIds(const QString& userQuery, int limit = 50);

    /// Forces a full rebuild of the FTS indexes (`'rebuild'` command).
    /// Slow on large libraries — only call after schema migrations or
    /// on user request from the preferences.
    Result<void> rebuildAll();

    /// Returns approximate row counts for the two indexes.
    Result<QPair<int, int>> stats();
};

} // namespace soundshelf
