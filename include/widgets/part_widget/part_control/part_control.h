#pragma once

#include <QMenu>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <qmap.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/model/part_meta_model.h"
#include "widgets/part_widget/part_control/part_control_tree_widget.h"
#include "widgets/part_widget/right_click_menu.h"

namespace ORNL {
/*!
 * \class PartControl
 * \brief is a widget that lists parts by name and type allows for basic interaction through a button and context menu
 */
class PartControl : public QWidget {
    Q_OBJECT

  public:
    //! \brief Constructor
    //! \param parent: the widget this sits on
    explicit PartControl(QWidget* parent = nullptr);

    //! \brief the number of items in the list
    //! \return an int
    int count();

    //! \brief First part's name
    //! \return string of name
    QString nameOfFirstPart();

    //! \brief Sets the model to track.
    void setModel(QSharedPointer<PartMetaModel> m);

    //! \brief sets style according to current theme
    void setupStyle();

    //! \brief sets the status warning if a part is floating
    //! \param name the name of the part
    //! \param status if it is floating
    void setFloatingStatus(const QString& name, bool status);

    //! \brief sets the status warning if a part is outside the print volume
    //! \param name the name of the part
    //! \param status if the part is outside the volume
    void setOutsideStatus(const QString& name, bool status);

  private slots:
    //! \brief Model updates.
    void modelAdditionUpdate(QSharedPointer<PartMetaItem> pm);
    void modelRemovalUpdate(QSharedPointer<PartMetaItem> pm);
    void modelSelectionUpdate(QSharedPointer<PartMetaItem> pm);
    void modelParentingUpdate(QSharedPointer<PartMetaItem> pm);
    void modelVisualUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Handle selection update by the tree view.
    void handleSelectionChange();

    //! \brief Handle parenting changes.
    void handleParentingChange();

  private:
    //! \brief sets up this widgets sub-widgets
    void setupSubWidgets();
    //! \brief sets up this widget's layout
    void setupLayouts();
    //! \brief sets up this widget's events
    void setupEvents();

    //! \brief a list to display the parts
    PartControlTreeWidget* m_tree_widget;
    //! \brief a menu to display context controls on a right click
    RightClickMenu* m_right_click_menu;

    //! \brief Actions for the drop down menu.
    QMap<QString, QAction*> m_actions;

    //! \brief a layout to hold everything
    QVBoxLayout* m_layout;

    //! \brief Model
    QSharedPointer<PartMetaModel> m_model;
};
} // namespace ORNL
