#pragma once

#include <QWidget>
#include <QList>
#include "soundshelf/core/Disc.hpp"

class QListView;
class QStandardItemModel;

namespace soundshelf {

/**
 * @brief Grid of discs (cover thumbnails + title / artist label).
 *
 * `QListView` in `IconMode`, fed by a `QStandardItemModel`. Selecting
 * an item emits @ref discActivated; the shell typically responds by
 * loading the disc's tracks into @ref LibraryView.
 *
 * Cover art is taken from `Disc::coverData` if present, otherwise a
 * generic placeholder is rendered.
 */
class DiscView : public QWidget {
    Q_OBJECT
public:
    explicit DiscView(QWidget* parent = nullptr);
    ~DiscView() override;

    void setDiscs(const QList<Disc>& discs);

signals:
    void discActivated(const Disc& disc);

private:
    QListView*          m_view = nullptr;
    QStandardItemModel* m_model = nullptr;
    QList<Disc>         m_discs;
};

} // namespace soundshelf
