#pragma once

#include <QMenu>
#include <qaction.h>
#include <qlist.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qslider.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "widgets/part_widget/model/part_meta_item.h"

namespace ORNL {
/*!
 * \class RightClickMenu
 * \brief displays a contex menu to change a part's settings
 */
class RightClickMenu : public QMenu {
    Q_OBJECT

  public:
    //! \brief Constructor
    //! \param parent: the widget this sits on
    explicit RightClickMenu(QWidget* parent);

    //! \brief displays the contex menu with information on the supplied part as a position
    //! \param pos: where to spawn the menu in global UI coordinates
    //! \param part: the part to display info on. Note: this can be nullptr to show a disabled(grayed out) menu
    //! \param transparency: part's current transparency
    void show(const QPointF& pos, QList<QSharedPointer<PartMetaItem>> items);

  private:
    //! \brief sets up this menu's actions
    void setupActions();

    //! \brief sets up this widget's events
    void setupEvents();

    //! \brief disables actions/ buttons in the menu depending on nullability and mesh type
    void disableActions();

    //! \brief actions to display
    QAction* m_switch_to_clipper_action;
    QAction* m_switch_to_build_action;
    QAction* m_switch_to_setting_action;
    QAction* m_reset_transformation_action;
    QAction* m_reload_part_action;
    QAction* m_replace_part_action;
    QAction* m_delete_part_action;
    QAction* m_wireframe_action;
    QAction* m_solidwireframe_action;
    QAction* m_lock_part_action;

    //! \brief Menu to hold transparency
    QMenu* m_transparency_menu;

    //! \brief Slider to transparency (must connect directly to widget not Action)
    QSlider* m_transparency_slider;

    QList<QSharedPointer<PartMetaItem>> m_selected_items;
};
} // namespace ORNL
