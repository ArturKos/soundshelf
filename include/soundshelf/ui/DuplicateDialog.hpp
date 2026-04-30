#pragma once

#include <QWidget>
#include <QList>
#include "soundshelf/core/DuplicateDetector.hpp"

class QTreeWidget;
class QPushButton;

namespace soundshelf {

/**
 * @brief Lists duplicate groups produced by @ref DuplicateDetector and
 * lets the user pick which copies to keep.
 *
 * Top-level rows = duplicate groups (annotated with the detection
 * reason: byte-identical, AcoustID, or tag-bucket). Children = the
 * member tracks. Right-click on a child opens a context menu with
 * "keep this copy / delete others".
 *
 * The widget is purely UI — actually deleting the picked tracks is
 * delegated through the @ref deleteRequested signal.
 */
class DuplicateDialog : public QWidget {
    Q_OBJECT
public:
    explicit DuplicateDialog(QWidget* parent = nullptr);
    ~DuplicateDialog() override;

    void setGroups(const QList<DuplicateDetector::Group>& groups);

signals:
    /// Emitted when the user confirms removal of a list of track IDs.
    void deleteRequested(const QList<int>& trackIds);

private:
    QTreeWidget*  m_tree = nullptr;
    QPushButton*  m_deleteBtn = nullptr;
};

} // namespace soundshelf
