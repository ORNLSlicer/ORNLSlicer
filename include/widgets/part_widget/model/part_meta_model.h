#pragma once

#include <QObject>
#include <qlist.h>
#include <qmap.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "widgets/part_widget/model/part_meta_item.h"

namespace ORNL {
// Forward
class PartControlTreeItem;
class PartObject;
class Part;

/*!
 * \brief A model that allows related UI classes to access the same information. Think of it as
 *        a simpilfied version of Qt's builtin model-view architecture for use with our view classes.
 */
class PartMetaModel : public QObject, public QEnableSharedFromThis<PartMetaModel> {
    Q_OBJECT
  public:
    //! \brief Constructor
    PartMetaModel();

    //! \brief All items in the model.
    QList<QSharedPointer<PartMetaItem>> items();
    //! \brief All items that are selected in the model.
    QList<QSharedPointer<PartMetaItem>> selectedItems();

    //! \brief Looks up an item by the PartObject.
    QSharedPointer<PartMetaItem> lookupByGraphic(QSharedPointer<PartObject> gop);
    //! \brief Looks up an item by the tree item.
    QSharedPointer<PartMetaItem> lookupByTreeItem(PartControlTreeItem* item);
    //! \brief Looks up an item by the part pointer.
    QSharedPointer<PartMetaItem> lookupByPointer(QSharedPointer<Part> p);

    //! \brief Creates a item from a part pointer.
    QSharedPointer<PartMetaItem> newItem(QSharedPointer<Part> p);
    //! \brief Adds an already created item.
    void addItem(QSharedPointer<PartMetaItem> pm);

    //! \brief Reloads an item.
    void replaceItem(QSharedPointer<PartMetaItem> pm, QString filename);

    //! \brief Reloads an item.
    void reloadItem(QSharedPointer<PartMetaItem> pm);

    //! \brief Removes an item.
    void removeItem(QSharedPointer<PartMetaItem> pm);
    //! \brief Removes all items.
    void clearItems();

    //! \brief Setup a copy of the selected items.
    void setSelectionCopied();
    //! \brief Pastes the previously copied items.
    void copySelection();

  signals:
    //! \brief Signal that an item was added.
    void itemAddedUpdate(QSharedPointer<PartMetaItem> pm);
    //! \brief Signal that an item was reloaded.
    void itemReloadUpdate(QSharedPointer<PartMetaItem> pm);
    //! \brief Signal that an item was removed.
    void itemRemovedUpdate(QSharedPointer<PartMetaItem> pm);
    //! \brief Signal that an item has a change in parenting.
    void parentingUpdate(QSharedPointer<PartMetaItem> pm);
    //! \brief Signal that an item has been selected/unselected.
    void selectionUpdate(QSharedPointer<PartMetaItem> pm);
    //! \brief Signal that an item has a visual update.
    void visualUpdate(QSharedPointer<PartMetaItem> pm);
    //! \brief Signal that an item has a new transformation.
    void transformUpdate(QSharedPointer<PartMetaItem> pm);

    //! \brief Signal that any update has occured.
    void modelUpdated(QSharedPointer<PartMetaItem> pm);

  private slots:
    //! \brief Slot to recieve updates from items.
    void itemUpdated(PartMetaItem::PartMetaUpdateType type);

  private:
    //! \brief Items need access to private slots.
    friend class PartMetaItem;

    //! \brief Items.
    QMap<QSharedPointer<Part>, QSharedPointer<PartMetaItem>> m_pointer_lookup;

    //! \brief Copied items.
    QList<QSharedPointer<PartMetaItem>> m_copied_list;
};
} // namespace ORNL
