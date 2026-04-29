#pragma once

#include <QObject>
#include <QString>


namespace soundshelf {

/// Wrapper nad SQLite FTS5 — rebuild, search
/// TODO: implementacja — patrz CLAUDE.md
class FTS5Index : public QObject {
    Q_OBJECT
public:
    explicit FTS5Index(QObject* parent = nullptr);
    ~FTS5Index() override;

    // TODO: API
};

} // namespace soundshelf
