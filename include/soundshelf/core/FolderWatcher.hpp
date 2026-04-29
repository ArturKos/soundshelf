#pragma once

#include <QObject>
#include <QString>


namespace soundshelf {

/// QFileSystemWatcher wrapper — auto-update biblioteki
/// TODO: implementacja — patrz CLAUDE.md
class FolderWatcher : public QObject {
    Q_OBJECT
public:
    explicit FolderWatcher(QObject* parent = nullptr);
    ~FolderWatcher() override;

    // TODO: API
};

} // namespace soundshelf
