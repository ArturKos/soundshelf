#pragma once

#include <QObject>
#include <QString>
#include "soundshelf/core/Result.hpp"
#include <QFuture>
#include <QJsonDocument>
class QNetworkAccessManager;

namespace soundshelf {

/// Bazowa klasa REST client — rate limit, retry, auth
/// TODO: implementacja — patrz CLAUDE.md
class RestClient : public QObject {
    Q_OBJECT
public:
    explicit RestClient(QObject* parent = nullptr);
    ~RestClient() override;

    // TODO: API
};

} // namespace soundshelf
