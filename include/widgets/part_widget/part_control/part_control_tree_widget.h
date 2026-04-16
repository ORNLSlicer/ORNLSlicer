#pragma once

#include <QTreeWidget>
#include <qitemselectionmodel.h>
#include <qtmetamacros.h>
#include <qwidget.h>

namespace ORNL {
/*!
 * \brief Simple subclass to allow custom selection/drop behavior.
 */
class PartControlTreeWidget : public QTreeWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param parent: A widget.
    PartControlTreeWidget(QWidget* parent = nullptr);

  public slots:
    //! \brief When all parts are selected, ensure that only top level objects are selected.
    void selectAll() override;

  signals:
    //! \brief When a drop ends, signal that parenting may have changed.
    void dropEnded();

  protected slots:
    //! \brief Catches when a drop occurs.
    void dropEvent(QDropEvent* event) override;

    //! \brief When the selection changes, ensure that only top level objects are selected.
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;
};
} // namespace ORNL
