#include "soundshelf/ui/LibraryView.hpp"

#include <QVBoxLayout>
#include <QTableView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QHeaderView>

namespace soundshelf {

namespace {

QString fmtDuration(int ms) {
    if (ms <= 0) return QString();
    const int s = ms / 1000;
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

LibraryView::LibraryView(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search tracks…"));
    root->addWidget(m_search);

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({
        tr("#"), tr("Title"), tr("Artist"), tr("Album"),
        tr("Genre"), tr("Year"), tr("Duration"),
    });

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);  // search across all columns

    m_view = new QTableView(this);
    m_view->setModel(m_proxy);
    m_view->setSortingEnabled(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setAlternatingRowColors(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->horizontalHeader()->setStretchLastSection(true);
    root->addWidget(m_view, 1);

    // Two parallel filtering paths — both run for every keystroke,
    // and that's intentional. The proxy filter narrows the *currently
    // visible* tracks instantly (great UX), while the debounced
    // signal asks the shell to fetch a wider FTS5-backed result set
    // for libraries that don't fit in the proxy's source model.
    connect(m_search, &QLineEdit::textChanged,
            m_proxy, &QSortFilterProxyModel::setFilterFixedString);

    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(250);
    connect(m_search, &QLineEdit::textChanged, this,
            [this]() { m_searchDebounce.start(); });
    connect(&m_searchDebounce, &QTimer::timeout, this, [this]() {
        emit searchRequested(m_search->text().trimmed());
    });

    connect(m_view, &QTableView::doubleClicked,
            this, [this](const QModelIndex& proxyIdx) {
        const auto src = m_proxy->mapToSource(proxyIdx);
        if (src.row() < 0 || src.row() >= m_tracks.size()) return;
        emit trackActivated(m_tracks[src.row()]);
    });
}

LibraryView::~LibraryView() = default;

void LibraryView::setTracks(const QList<Track>& tracks) {
    m_tracks = tracks;
    m_model->removeRows(0, m_model->rowCount());
    for (const auto& t : tracks) {
        auto* numItem = new QStandardItem(QString::number(t.trackNumber));
        numItem->setData(Qt::Unchecked, Qt::CheckStateRole);
        numItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);

        QList<QStandardItem*> row;
        row << numItem
            << new QStandardItem(t.title)
            << new QStandardItem(t.artist)
            << new QStandardItem(t.album)
            << new QStandardItem(t.genre)
            << new QStandardItem(t.year > 0 ? QString::number(t.year) : QString())
            << new QStandardItem(fmtDuration(t.durationMs));
        m_model->appendRow(row);
    }
}

QList<int> LibraryView::selectedTrackIds() const {
    QList<int> ids;
    for (const auto& idx : m_view->selectionModel()->selectedRows()) {
        const int row = m_proxy->mapToSource(idx).row();
        if (row >= 0 && row < m_tracks.size()) ids << m_tracks[row].id;
    }
    return ids;
}

QList<int> LibraryView::checkedTrackIds() const {
    QList<int> ids;
    const int rows = m_model->rowCount();
    for (int r = 0; r < rows; ++r) {
        const auto* item = m_model->item(r, 0);
        if (item && item->checkState() == Qt::Checked && r < m_tracks.size())
            ids << m_tracks[r].id;
    }
    return ids;
}

void LibraryView::selectAll() {
    const int rows = m_model->rowCount();
    for (int r = 0; r < rows; ++r) {
        if (auto* item = m_model->item(r, 0))
            item->setCheckState(Qt::Checked);
    }
}

void LibraryView::selectNone() {
    const int rows = m_model->rowCount();
    for (int r = 0; r < rows; ++r) {
        if (auto* item = m_model->item(r, 0))
            item->setCheckState(Qt::Unchecked);
    }
}

void LibraryView::invertSelection() {
    const int rows = m_model->rowCount();
    for (int r = 0; r < rows; ++r) {
        if (auto* item = m_model->item(r, 0)) {
            item->setCheckState(item->checkState() == Qt::Checked
                ? Qt::Unchecked : Qt::Checked);
        }
    }
}

} // namespace soundshelf
