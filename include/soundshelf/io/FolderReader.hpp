#pragma once

#include "soundshelf/io/DiscReader.hpp"

namespace soundshelf {

/// Traktuje katalog jako jedną płytę (lub multi-disc gdy są podfoldery).
/// Sortuje po polu TRACKNUMBER lub po nazwie pliku.
class FolderReader : public DiscReader {
public:
    explicit FolderReader(QString folderPath);

    Result<Toc> readToc() override;
    DiscType type() const override { return DiscType::Folder; }
    QString source() const override { return m_path; }
    bool supportsRipping() const override { return false; }

    /// Statyczne extensions, które uznajemy za audio.
    static QStringList audioExtensions();

private:
    QString m_path;
};

} // namespace soundshelf
