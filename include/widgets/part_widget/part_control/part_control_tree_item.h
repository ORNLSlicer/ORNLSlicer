#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qwidget.h>

#include "part/part.h"
#include "utilities/enums.h"
#include "widgets/part_widget/model/part_meta_item.h"

namespace ORNL {
/*!
 * \class PartControlTreeItem
 * \brief an item that sits in the part control's list
 */
class PartControlTreeItem : public QTreeWidgetItem {
  public:
    //! \brief Constructor
    //! \param part: a pointer to the part this item will contain
    explicit PartControlTreeItem(QSharedPointer<PartMetaItem> pm);

    //! \brief returns the part stored inside
    //! \return a pointer to a part
    QSharedPointer<Part> getPart();

    //! \brief Changes icon based on mesh type.
    void updateMeshType(MeshType type);

    //! \brief adds a container for warning badges to the item in the tree
    //! \param tree the tree to add to
    //! \param item_index the index of the item in this tree
    void addContainer(QTreeWidget* tree, int item_index);

    //! \brief enables/ disables the badge for parts outside the print volume
    //! \param outside if the part is outside the print volume
    void setOutsideVolume(bool outside);

    //! \brief enables/ disables the badge for floating parts
    //! \param floating
    void setFloating(bool floating);

  private:
    //! \class Container
    //! \brief a sub-class that is a container for the warning badges
    class Container : public QWidget {
      public:
        //! \brief Contructor
        //! \param name the name of the part
        //! \param closed if the part is a closed mesh
        //! \param parent the parent
        Container(QString name, bool closed, QWidget* parent = nullptr);

        //! \brief enables/ disables the badge for parts outside the print volume
        //! \param outside if the part is outside the print volume
        void showOutsideVolumeBadge(bool show);

        //! \brief enables/ disables the badge for floating parts
        //! \param floating
        void showFloatingBadge(bool show);

      private:
        QLabel* m_alignment_badge;
    };

    //! \brief updates the tooltip with new information
    void updateToolTip();

    //! \brief the part
    QSharedPointer<Part> m_part;

    //! \brief container for warning badges
    Container* m_container;

    bool m_is_mesh_closed = true;
    bool m_is_mesh_inside_volume = true;
    bool m_is_mesh_floating = true;
    MeshType m_mesh_type = MeshType::kBuild;
};
} // namespace ORNL
