#include "soundshelf/io/CDDAReader.hpp"

#include <QFile>
#include <QLoggingCategory>

#ifdef SOUNDSHELF_HAVE_LIBCDIO
#include <cdio/cdio.h>
#include <cdio/paranoia/paranoia.h>
#include <cdio/paranoia/cdda.h>
#include <discid/discid.h>
#endif

Q_LOGGING_CATEGORY(lcCdda, "soundshelf.disc.cdda")

namespace soundshelf {

namespace {

#ifdef SOUNDSHELF_HAVE_LIBCDIO
inline CdIo_t* asCdio(void* p) { return reinterpret_cast<CdIo_t*>(p); }
#endif

/// Zapisuje 16-bitowy stereo PCM jako prosty WAV header + data.
bool writeWavHeader(QFile& out, qint64 pcmBytes) {
    const quint32 sampleRate = 44100;
    const quint16 numChannels = 2;
    const quint16 bitsPerSample = 16;
    const quint32 byteRate = sampleRate * numChannels * bitsPerSample / 8;
    const quint16 blockAlign = numChannels * bitsPerSample / 8;
    const quint32 dataSize = static_cast<quint32>(pcmBytes);
    const quint32 fileSize = 36 + dataSize;

    auto write32 = [&out](quint32 v) {
        char b[4]{ char(v), char(v>>8), char(v>>16), char(v>>24) };
        return out.write(b, 4) == 4;
    };
    auto write16 = [&out](quint16 v) {
        char b[2]{ char(v), char(v>>8) };
        return out.write(b, 2) == 2;
    };

    if (out.write("RIFF", 4) != 4) return false;
    if (!write32(fileSize)) return false;
    if (out.write("WAVE", 4) != 4) return false;
    if (out.write("fmt ", 4) != 4) return false;
    if (!write32(16)) return false;            // fmt chunk size
    if (!write16(1)) return false;             // PCM format
    if (!write16(numChannels)) return false;
    if (!write32(sampleRate)) return false;
    if (!write32(byteRate)) return false;
    if (!write16(blockAlign)) return false;
    if (!write16(bitsPerSample)) return false;
    if (out.write("data", 4) != 4) return false;
    if (!write32(dataSize)) return false;
    return true;
}

} // anonymous

CDDAReader::CDDAReader(QString device)
    : m_device(std::move(device))
{
#ifdef SOUNDSHELF_HAVE_LIBCDIO
    m_cdio = cdio_open(m_device.toLocal8Bit().constData(), DRIVER_UNKNOWN);
    if (!m_cdio) {
        qCWarning(lcCdda) << "cdio_open failed for" << m_device;
    }
#endif
}

CDDAReader::~CDDAReader() {
#ifdef SOUNDSHELF_HAVE_LIBCDIO
    if (m_cdio) cdio_destroy(asCdio(m_cdio));
#endif
}

Result<Toc> CDDAReader::readToc() {
#ifndef SOUNDSHELF_HAVE_LIBCDIO
    return Result<Toc>::err(Error::DependencyMissing,
        QStringLiteral("Built without libcdio support"));
#else
    if (!m_cdio) {
        return Result<Toc>::err(Error::DeviceNotReady,
            QStringLiteral("Device %1 not opened").arg(m_device));
    }

    auto* cdio = asCdio(m_cdio);
    const track_t first = cdio_get_first_track_num(cdio);
    const track_t last  = cdio_get_last_track_num(cdio);
    if (first == CDIO_INVALID_TRACK || last == CDIO_INVALID_TRACK) {
        return Result<Toc>::err(Error::DeviceNotReady,
            QStringLiteral("No audio disc in %1").arg(m_device));
    }

    Toc toc;
    for (track_t t = first; t <= last; ++t) {
        const track_format_t fmt = cdio_get_track_format(cdio, t);
        if (fmt != TRACK_FORMAT_AUDIO) continue;  // skip data tracks

        const lsn_t startLsn = cdio_get_track_lsn(cdio, t);
        const lsn_t endLsn   = cdio_get_track_last_lsn(cdio, t);
        if (startLsn == CDIO_INVALID_LSN || endLsn == CDIO_INVALID_LSN) continue;

        TocEntry e;
        e.trackNumber = static_cast<int>(t);
        e.startSector = startLsn;
        e.endSector = endLsn;
        // 75 sektorów na sekundę
        e.durationMs = static_cast<int>((endLsn - startLsn) * 1000LL / 75);
        toc.entries.append(e);
        toc.totalDurationMs += e.durationMs;
    }

    m_lastToc = toc;
    m_tocValid = true;

    qCDebug(lcCdda) << "Read TOC:" << toc.entries.size() << "tracks,"
                    << toc.totalDurationMs << "ms total";

    // Compute disc ID (MusicBrainz)
    auto idResult = computeDiscId();
    if (idResult.isOk()) {
        toc.discId = idResult.value();
        m_lastToc.discId = toc.discId;
    }

    return Result<Toc>::ok(std::move(toc));
#endif
}

Result<QString> CDDAReader::computeDiscId() {
#ifndef SOUNDSHELF_HAVE_LIBCDIO
    return Result<QString>::err(Error::DependencyMissing,
        QStringLiteral("Built without libdiscid"));
#else
    DiscId* d = discid_new();
    if (!d) {
        return Result<QString>::err(Error::Unknown,
            QStringLiteral("discid_new failed"));
    }

    const int rc = discid_read(d, m_device.toLocal8Bit().constData());
    if (!rc) {
        const QString err = QString::fromUtf8(discid_get_error_msg(d));
        discid_free(d);
        return Result<QString>::err(Error::DeviceNotReady,
            QStringLiteral("discid_read failed: %1").arg(err));
    }

    const QString id = QString::fromUtf8(discid_get_id(d));
    discid_free(d);

    qCDebug(lcCdda) << "MusicBrainz disc ID:" << id;
    return Result<QString>::ok(id);
#endif
}

Result<void> CDDAReader::eject() {
#ifndef SOUNDSHELF_HAVE_LIBCDIO
    return Result<void>::err(Error::DependencyMissing,
        QStringLiteral("Built without libcdio support"));
#else
    if (cdio_eject_media_drive(m_device.toLocal8Bit().constData()) != DRIVER_OP_SUCCESS) {
        return Result<void>::err(Error::DeviceNotReady,
            QStringLiteral("Eject failed for %1").arg(m_device));
    }
    return Result<void>::ok();
#endif
}

Result<void> CDDAReader::ripTrackToWav(int trackNumber, const QString& outWavPath) {
#ifndef SOUNDSHELF_HAVE_LIBCDIO
    Q_UNUSED(trackNumber); Q_UNUSED(outWavPath);
    return Result<void>::err(Error::DependencyMissing,
        QStringLiteral("Built without libcdio paranoia"));
#else
    if (!m_tocValid) {
        auto r = readToc();
        if (!r) return Result<void>::err(r.error().code, r.error().message);
    }

    // Znajdź entry
    const TocEntry* entry = nullptr;
    for (const auto& e : m_lastToc.entries) {
        if (e.trackNumber == trackNumber) { entry = &e; break; }
    }
    if (!entry) {
        return Result<void>::err(Error::InvalidArgument,
            QStringLiteral("Track %1 not found").arg(trackNumber));
    }

    cdrom_drive_t* drive = cdio_cddap_identify(
        m_device.toLocal8Bit().constData(), CDDA_MESSAGE_FORGETIT, nullptr);
    if (!drive) {
        return Result<void>::err(Error::DeviceNotReady,
            QStringLiteral("cdda_identify failed"));
    }
    cdio_cddap_open(drive);

    cdrom_paranoia_t* paranoia = cdio_paranoia_init(drive);
    cdio_paranoia_modeset(paranoia,
        PARANOIA_MODE_FULL ^ PARANOIA_MODE_NEVERSKIP);
    cdio_paranoia_seek(paranoia, entry->startSector, SEEK_SET);

    QFile out(outWavPath);
    if (!out.open(QIODevice::WriteOnly)) {
        cdio_paranoia_free(paranoia);
        cdio_cddap_close(drive);
        return Result<void>::err(Error::FileAccessDenied,
            QStringLiteral("Cannot write to %1").arg(outWavPath));
    }

    const lsn_t totalSectors = entry->endSector - entry->startSector + 1;
    const qint64 pcmBytes = totalSectors * CDIO_CD_FRAMESIZE_RAW;
    writeWavHeader(out, pcmBytes);

    for (lsn_t i = 0; i < totalSectors; ++i) {
        int16_t* buf = cdio_paranoia_read(paranoia, nullptr);
        if (!buf) break;
        out.write(reinterpret_cast<char*>(buf), CDIO_CD_FRAMESIZE_RAW);
    }

    out.close();
    cdio_paranoia_free(paranoia);
    cdio_cddap_close(drive);

    qCDebug(lcCdda) << "Ripped track" << trackNumber << "to" << outWavPath;
    return Result<void>::ok();
#endif
}

} // namespace soundshelf
