#pragma once

#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf {

/// Czyta fizyczne CD-DA przez libcdio + libcdio-paranoia.
/// Wymaga ENABLE_LIBCDIO_PARANOIA w build configu.
class CDDAReader : public DiscReader {
public:
    /// device: "/dev/sr0" na Linuksie, "\\\\.\\D:" na Windows
    explicit CDDAReader(QString device);
    ~CDDAReader() override;

    Result<Toc> readToc() override;
    DiscType type() const override { return DiscType::Physical; }
    QString source() const override { return m_device; }
    bool supportsRipping() const override { return true; }

    /// Zwraca MusicBrainz disc ID (libdiscid SHA1) z aktualnego TOC.
    /// Wywoływane po readToc().
    Result<QString> computeDiscId();

    /// Wysuwa płytę z napędu.
    Result<void> eject();

    /// Czyta jedną ścieżkę do pliku WAV.
    /// Używa paranoia mode jeśli włączony.
    Result<void> ripTrackToWav(int trackNumber, const QString& outWavPath);

private:
    QString m_device;
    void* m_cdio = nullptr;     ///< cdio_t*  (void* żeby nie wciągać <cdio/cdio.h> do headera)
    Toc m_lastToc;
    bool m_tocValid = false;
};

} // namespace soundshelf
