#pragma once

#include <QObject>
#include <QString>


namespace soundshelf {

/// HTTP REST server — headless mode
/// TODO: implementacja — patrz CLAUDE.md
class HttpServer : public QObject {
    Q_OBJECT
public:
    explicit HttpServer(QObject* parent = nullptr);
    ~HttpServer() override;

    // TODO: API
};

} // namespace soundshelf
