#include "soundshelf/network/AcoustIDClient.hpp"

#include <QUrl>
#include <QUrlQuery>

namespace soundshelf {

namespace {
const QUrl kBase{ QStringLiteral("https://api.acoustid.org/v2") };
} // namespace

AcoustIDClient::AcoustIDClient(QObject* parent) : QObject(parent) {
    m_rest.setRateLimit(3.0);
}

AcoustIDClient::~AcoustIDClient() = default;

QFuture<Result<QJsonDocument>>
AcoustIDClient::lookup(const QString& fingerprint, int durationSec) {
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client"),      m_apiKey);
    q.addQueryItem(QStringLiteral("duration"),    QString::number(durationSec));
    q.addQueryItem(QStringLiteral("fingerprint"), fingerprint);
    q.addQueryItem(QStringLiteral("meta"),
                   QStringLiteral("recordings+releasegroups+compress"));
    q.addQueryItem(QStringLiteral("format"),      QStringLiteral("json"));
    return m_rest.getJson(kBase, QStringLiteral("/lookup"), q);
}

} // namespace soundshelf
