#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QJsonDocument>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/**
 * @brief Client for the AcoustID `/v2/lookup` endpoint.
 *
 * Maps a Chromaprint fingerprint + duration to one or more MusicBrainz
 * Recording MBIDs. Free service that requires a per-application API
 * key from `https://acoustid.org/`.
 */
class AcoustIDClient : public QObject {
    Q_OBJECT
public:
    explicit AcoustIDClient(QObject* parent = nullptr);
    ~AcoustIDClient() override;

    /// Required: AcoustID API key.
    void setApiKey(const QString& key) { m_apiKey = key; }

    /// Looks up @p fingerprint with @p durationSec.
    QFuture<Result<QJsonDocument>> lookup(const QString& fingerprint, int durationSec);

private:
    RestClient m_rest;
    QString    m_apiKey;
};

} // namespace soundshelf
