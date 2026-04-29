#pragma once

#include <QObject>
#include <QString>


namespace soundshelf {

/// MPRIS2 D-Bus adapter dla Linux integration
/// TODO: implementacja — patrz CLAUDE.md
class MprisAdapter : public QObject {
    Q_OBJECT
public:
    explicit MprisAdapter(QObject* parent = nullptr);
    ~MprisAdapter() override;

    // TODO: API
};

} // namespace soundshelf
