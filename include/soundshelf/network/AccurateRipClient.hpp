#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QByteArray>
#include "soundshelf/network/RestClient.hpp"
#include "soundshelf/network/AccurateRip.hpp"

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
 * CRC pairs is done by accuraterip::parseResponse().
 */
class AccurateRipClient : public QObject {
    Q_OBJECT
public:
    explicit AccurateRipClient(QObject* parent = nullptr);
    ~AccurateRipClient() override;

    /// @param trackCount  number of audio tracks on the disc
    /// @param ar1         AccurateRip ID1 (sum of sector offsets)
    /// @param ar2         AccurateRip ID2 (weighted sum)
    /// @param freedbId    classic FreeDB id (8-hex-char int)
    QFuture<Result<QByteArray>> lookup(int trackCount,
                                       quint32 ar1,
                                       quint32 ar2,
                                       quint32 freedbId);

    /**
     * @brief Convenience overload that derives disc IDs from a TOC.
     *
     * Calls accuraterip::computeDiscIds() then forwards to the four-argument
     * overload.  Returns the raw dBAR binary blob for subsequent parsing with
     * accuraterip::parseResponse().
     *
     * @param toc  Table of contents from CDDAReader or CueParser.
     */
    QFuture<Result<QByteArray>> lookup(const Toc& toc);

private:
    RestClient m_rest;
};

} // namespace soundshelf
