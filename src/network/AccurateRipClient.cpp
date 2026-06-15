#include "soundshelf/network/AccurateRipClient.hpp"
#include "soundshelf/network/AccurateRip.hpp"

#include <QUrl>

namespace soundshelf {

namespace {
const QUrl kBase{ QStringLiteral("http://www.accuraterip.com") };
} // namespace

AccurateRipClient::AccurateRipClient(QObject* parent) : QObject(parent) {
    m_rest.setRateLimit(2.0);
}
AccurateRipClient::~AccurateRipClient() = default;

QFuture<Result<QByteArray>>
AccurateRipClient::lookup(int trackCount, quint32 ar1, quint32 ar2, quint32 freedbId) {
    const QString hex = QStringLiteral("%1").arg(ar1, 8, 16, QLatin1Char('0'));
    const QString f = hex.right(1);
    const QString g = hex.right(2).left(1);
    const QString h = hex.right(3).left(1);
    const QString filename = QStringLiteral("dBAR-%1-%2-%3-%4.bin")
        .arg(QStringLiteral("%1").arg(trackCount, 3, 10, QLatin1Char('0')))
        .arg(QStringLiteral("%1").arg(ar1,        8, 16, QLatin1Char('0')))
        .arg(QStringLiteral("%1").arg(ar2,        8, 16, QLatin1Char('0')))
        .arg(QStringLiteral("%1").arg(freedbId,   8, 16, QLatin1Char('0')));
    const QString path = QStringLiteral("/accuraterip/%1/%2/%3/%4")
        .arg(f, g, h, filename);
    return m_rest.getBytes(kBase, path);
}

QFuture<Result<QByteArray>> AccurateRipClient::lookup(const Toc& toc) {
    const accuraterip::DiscIds ids = accuraterip::computeDiscIds(toc);
    return lookup(ids.trackCount, ids.id1, ids.id2, ids.freedbId);
}

} // namespace soundshelf
