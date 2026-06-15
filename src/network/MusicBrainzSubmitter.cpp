#include "soundshelf/network/MusicBrainzSubmitter.hpp"

#include <QLoggingCategory>
#include <QUrl>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(lcMbSubmit, "soundshelf.network.mbsubmit")

namespace soundshelf {

namespace {

QString discTypeToMbFormat(DiscType type)
{
    switch (type) {
    case DiscType::Physical:
        return QStringLiteral("CD");
    case DiscType::Folder:
    case DiscType::Image:
    case DiscType::Remote:
        return QStringLiteral("Digital Media");
    }
    return QStringLiteral("Digital Media");
}

} // namespace

QList<QPair<QString, QString>> MusicBrainzSubmitter::buildSeedFields(
    const Disc& disc, const QString& editNote)
{
    QList<QPair<QString, QString>> fields;

    auto add = [&](const QString& key, const QString& value) {
        fields.append({ key, value });
    };

    // Release title
    if (!disc.title.isEmpty())
        add(QStringLiteral("name"), disc.title);

    // Release-level artist credit (both the join-phrase name and the canonical artist name)
    if (!disc.artist.isEmpty()) {
        add(QStringLiteral("artist_credit.names.0.name"), disc.artist);
        add(QStringLiteral("artist_credit.names.0.artist.name"), disc.artist);
    }

    // Release year (only when non-zero)
    if (disc.year > 0)
        add(QStringLiteral("date.year"), QString::number(disc.year));

    // Barcode
    if (!disc.barcode.isEmpty())
        add(QStringLiteral("barcode"), disc.barcode);

    // Label + catalogue number (skip either when empty)
    if (!disc.label.isEmpty())
        add(QStringLiteral("labels.0.name"), disc.label);
    if (!disc.catalogNo.isEmpty())
        add(QStringLiteral("labels.0.catalog_number"), disc.catalogNo);

    // Medium format derived from DiscType
    add(QStringLiteral("mediums.0.format"), discTypeToMbFormat(disc.type));

    // Per-track fields
    for (int i = 0; i < disc.tracks.size(); ++i) {
        const Track& t = disc.tracks.at(i);
        const QString prefix = QStringLiteral("mediums.0.track.%1.").arg(i);

        // Use stored track number; fall back to 1-based position when unset
        const int num = (t.trackNumber > 0) ? t.trackNumber : (i + 1);
        add(prefix + QStringLiteral("number"), QString::number(num));

        if (!t.title.isEmpty())
            add(prefix + QStringLiteral("name"), t.title);

        if (t.durationMs > 0)
            add(prefix + QStringLiteral("length"), QString::number(t.durationMs));

        // Track-level artist credit only for various-artists tracks
        if (!t.artist.isEmpty() && t.artist != disc.artist)
            add(prefix + QStringLiteral("artist_credit.names.0.name"), t.artist);
    }

    // Optional edit note (free-form text shown to MB editors)
    if (!editNote.isEmpty())
        add(QStringLiteral("edit_note"), editNote);

    qCDebug(lcMbSubmit) << "buildSeedFields: disc" << disc.title
                         << "->" << fields.size() << "fields,"
                         << disc.tracks.size() << "tracks";

    return fields;
}

QUrl MusicBrainzSubmitter::buildSeedUrl(const Disc& disc, const QString& editNote)
{
    const auto fields = buildSeedFields(disc, editNote);

    QUrlQuery query;
    for (const auto& [key, value] : fields)
        query.addQueryItem(key, value);

    QUrl url(QStringLiteral("https://musicbrainz.org/release/add"));
    url.setQuery(query);

    qCDebug(lcMbSubmit) << "buildSeedUrl:" << url.toString().left(120);

    return url;
}

} // namespace soundshelf
