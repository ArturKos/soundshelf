#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QByteArray>
#include "soundshelf/network/RestClient.hpp"

namespace soundshelf {

/**
 * @brief Looks up CD-DA tracks against the AccurateRip database.
 *
 * AccurateRip stores per-track CRC32s computed from the rip and lets
 * clients verify that they ripped a CD identically to other users.
 *
 * Endpoint format (binary, not JSON):
 * `http://www.accuraterip.com/accuraterip/<f>/<g>/<h>/dBAR-<n>-<id1>-<id2>-<freedb>.bin`
 * where `<f><g><h>` are the first three hex chars of the disc id.
 *
 * The class only fetches the binary blob; parsing it into per-track
 * CRC pairs is done by the caller.
 */
class AccurateRipClient : public QObject {
    Q_OBJECT
public:
    explicit AccurateRipClient(QObject* parent = nullptr);
    ~AccurateRipClient() override;

    /// @param trackCount  number of audio tracks on the disc
    /// @param ar1         AccurateRip "ID1" (CRC of leadouts)
    /// @param ar2         AccurateRip "ID2" (CRC of offsets)
    /// @param freedbId    classic FreeDB id (8-hex-char int)
    QFuture<Result<QByteArray>> lookup(int trackCount,
                                       quint32 ar1,
                                       quint32 ar2,
                                       quint32 freedbId);

private:
    RestClient m_rest;
};

} // namespace soundshelf
