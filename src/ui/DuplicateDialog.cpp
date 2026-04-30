#include "soundshelf/ui/DuplicateDialog.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QFileInfo>

namespace soundshelf {

namespace {

QString reasonLabel(DuplicateDetector::Strategy s) {
    switch (s) {
        case DuplicateDetector::ByByteHash: return QObject::tr("Byte-identical");
        case DuplicateDetector::ByAcoustId: return QObject::tr("AcoustID match");
        case DuplicateDetector::ByTags:     return QObject::tr("Tag match");
        default: return QObject::tr("Other");
    }
}

} // namespace

DuplicateDialog::DuplicateDialog(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({
        tr("Track / Group"), tr("Artist"), tr("Album"), tr("Path")
    });
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    root->addWidget(m_tree, 1);

    m_deleteBtn = new QPushButton(tr("Delete selected duplicates"), this);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(m_deleteBtn);
    root->addLayout(row);

    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        QList<int> ids;
        for (auto* item : m_tree->selectedItems()) {
            const int id = item->data(0, Qt::UserRole).toInt();
            if (id > 0) ids << id;
        }
        if (!ids.isEmpty()) emit deleteRequested(ids);
    });
}

DuplicateDialog::~DuplicateDialog() = default;

void DuplicateDialog::setGroups(const QList<DuplicateDetector::Group>& groups) {
    m_tree->clear();
    int gIdx = 0;
    for (const auto& g : groups) {
        ++gIdx;
        auto* parent = new QTreeWidgetItem(m_tree);
        parent->setText(0, tr("Group %1 (%2 — %3 copies)")
            .arg(gIdx).arg(reasonLabel(g.reason)).arg(g.tracks.size()));
        parent->setFirstColumnSpanned(false);

        for (const auto& t : g.tracks) {
            auto* child = new QTreeWidgetItem(parent);
            child->setText(0, t.title);
            child->setText(1, t.artist);
            child->setText(2, t.album);
            child->setText(3, t.filepath);
            child->setData(0, Qt::UserRole, t.id);
        }
        parent->setExpanded(true);
    }
}

} // namespace soundshelf
