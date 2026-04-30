#include "soundshelf/network/CoverArtClient.hpp"

#include <QUrl>

namespace soundshelf {

namespace {
const QUrl kBase{ QStringLiteral("https://coverartarchive.org") };
} // namespace

CoverArtClient::CoverArtClient(QObject* parent) : QObject(parent) {
    m_rest.setRateLimit(5.0);
}
CoverArtClient::~CoverArtClient() = default;

QFuture<Result<QByteArray>>
CoverArtClient::fetchFront(const QString& releaseMbid, Size size) {
    QString suffix;
    switch (size) {
        case Size::Preview250: suffix = QStringLiteral("-250"); break;
        case Size::Medium500:  suffix = QStringLiteral("-500"); break;
        case Size::Original:   break;
    }
    const QString path = QStringLiteral("/release/%1/front%2")
        .arg(releaseMbid, suffix);
    return m_rest.getBytes(kBase, path);
}

} // namespace soundshelf
