#include "soundshelf/ui/DiscView.hpp"

#include <QVBoxLayout>
#include <QListView>
#include <QStandardItemModel>
#include <QPixmap>
#include <QImage>

namespace soundshelf {

DiscView::DiscView(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_model = new QStandardItemModel(this);
    m_view  = new QListView(this);
    m_view->setModel(m_model);
    m_view->setViewMode(QListView::IconMode);
    m_view->setIconSize(QSize(140, 140));
    m_view->setGridSize(QSize(180, 200));
    m_view->setResizeMode(QListView::Adjust);
    m_view->setMovement(QListView::Static);
    m_view->setUniformItemSizes(true);
    root->addWidget(m_view);

    connect(m_view, &QListView::doubleClicked,
            this, [this](const QModelIndex& idx) {
        if (idx.row() < 0 || idx.row() >= m_discs.size()) return;
        emit discActivated(m_discs[idx.row()]);
    });
}

DiscView::~DiscView() = default;

void DiscView::setDiscs(const QList<Disc>& discs) {
    m_discs = discs;
    m_model->removeRows(0, m_model->rowCount());
    for (const auto& d : discs) {
        QPixmap thumb;
        if (!d.coverData.isEmpty()) {
            QImage img;
            if (img.loadFromData(d.coverData)) {
                thumb = QPixmap::fromImage(img.scaled(140, 140,
                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        if (thumb.isNull()) {
            thumb = QPixmap(140, 140);
            thumb.fill(QColor(60, 60, 60));
        }
        const QString label = d.artist.isEmpty()
            ? d.title : QStringLiteral("%1\n%2").arg(d.title, d.artist);
        auto* item = new QStandardItem(QIcon(thumb), label);
        item->setEditable(false);
        item->setTextAlignment(Qt::AlignCenter);
        m_model->appendRow(item);
    }
}

} // namespace soundshelf
