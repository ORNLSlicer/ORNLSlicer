#include "widgets/part_widget/model/part_meta_model.h"

#include <QStack>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmap.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "graphics/objects/part_object.h"
#include "managers/session_manager.h"
#include "part/part.h"
#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/part_control/part_control_tree_item.h"

namespace ORNL {
PartMetaModel::PartMetaModel() {
    // NOP
}

QList<QSharedPointer<PartMetaItem>> PartMetaModel::items() { return m_pointer_lookup.values(); }

QList<QSharedPointer<PartMetaItem>> PartMetaModel::selectedItems() {
    QList<QSharedPointer<PartMetaItem>> ret;
    for (auto& pm : m_pointer_lookup.values()) {
        if (pm->isSelected())
            ret.append(pm);
    }

    return ret;
}

QSharedPointer<PartMetaItem> PartMetaModel::lookupByGraphic(QSharedPointer<PartObject> gop) {
    if (gop.isNull())
        return nullptr;
    return this->lookupByPointer(gop->part());
}

QSharedPointer<PartMetaItem> PartMetaModel::lookupByTreeItem(PartControlTreeItem* item) {
    if (!item)
        return nullptr;
    return this->lookupByPointer(item->getPart());
}

QSharedPointer<PartMetaItem> PartMetaModel::lookupByPointer(QSharedPointer<Part> p) {
    if (!m_pointer_lookup.contains(p))
        return nullptr;
    return m_pointer_lookup[p];
}

QSharedPointer<PartMetaItem> PartMetaModel::newItem(QSharedPointer<Part> p) {
    auto pm = QSharedPointer<PartMetaItem>::create(p);
    this->addItem(pm);

    return pm;
}

void PartMetaModel::addItem(QSharedPointer<PartMetaItem> pm) {
    m_pointer_lookup[pm->part()] = pm;
    pm->setModel(this->sharedFromThis());
    QObject::connect(pm.get(), &PartMetaItem::modified, this, &PartMetaModel::itemUpdated);
    emit itemAddedUpdate(pm);

    // Add children
    QStack<QSharedPointer<Part>> queue;
    QMap<QSharedPointer<Part>, QSharedPointer<PartMetaItem>> item_lookup;

    item_lookup[pm->part()] = pm;
    for (auto& c : pm->part()->children()) {
        queue.push(c);
    }

    while (!queue.empty()) {
        QSharedPointer<Part> curr_part = queue.pop();

        for (auto& c : curr_part->children()) {
            queue.push(c);
        }

        QSharedPointer<PartMetaItem> cpm = QSharedPointer<PartMetaItem>::create(curr_part);
        QObject::connect(cpm.get(), &PartMetaItem::modified, this, &PartMetaModel::itemUpdated);
        emit itemAddedUpdate(cpm);

        item_lookup[curr_part->parent()]->blockSignals(true);
        item_lookup[curr_part->parent()]->adoptChild(cpm);
        item_lookup[curr_part->parent()]->blockSignals(false);
        item_lookup[curr_part] = cpm;
        m_pointer_lookup[curr_part] = cpm;

        cpm->setModel(this->sharedFromThis());
    }
}

void PartMetaModel::replaceItem(QSharedPointer<PartMetaItem> pm, QString filename) {
    if (pm.isNull())
        return;

    CSM->replacePart(pm, filename);
}

void PartMetaModel::reloadItem(QSharedPointer<PartMetaItem> pm) {
    if (pm.isNull())
        return;

    CSM->reloadPart(pm);
}

void PartMetaModel::removeItem(QSharedPointer<PartMetaItem> pm) {
    if (pm.isNull())
        return;

    m_pointer_lookup.remove(pm->part());

    // Children of this item need to find new parents. This item's parent can adopt.
    auto pm_par = pm->parent();
    if (pm_par.isNull()) {
        for (auto pm_child : pm->children()) {
            pm->orphanChild(pm_child);
        }
    }
    else {
        pm_par->orphanChild(pm);

        for (auto pm_child : pm->children()) {
            pm->orphanChild(pm_child);
            pm_par->adoptChild(pm_child);
        }
    }

    emit itemRemovedUpdate(pm);
}

void PartMetaModel::clearItems() {
    for (auto& pm : m_pointer_lookup.values()) {
        this->removeItem(pm);
    }
}

void PartMetaModel::setSelectionCopied() { m_copied_list = this->selectedItems(); }

void PartMetaModel::copySelection() {
    QStack<QSharedPointer<PartMetaItem>> queue;
    for (auto& pm : m_copied_list) {
        queue.push(pm);
    }

    QStringList namelist;
    for (auto& p : m_pointer_lookup.keys()) {
        namelist.append(p->name());
    }

    QMap<QSharedPointer<PartMetaItem>, QSharedPointer<Part>> result;

    while (!queue.empty()) {
        QSharedPointer<PartMetaItem> pm = queue.pop();

        for (auto& cpm : pm->children()) {
            queue.push(cpm);
        }

        // Copy part.
        QSharedPointer<Part> p = QSharedPointer<Part>::create(pm->part());
        p->setTransformation(QMatrix4x4());
        p->setName(p->name() + "_copy");

        QString org_name = p->name();
        uint count = 1;

        // Find a new name.
        while (namelist.contains(p->name())) {
            p->setName(org_name + "_" + QString::number(count));
            count++;
        }

        result[pm] = p;

        // If this item has a parent, link it up.
        if (result.contains(pm->parent())) {
            result[pm->parent()]->adoptChild(p);
        }
    }

    // Add to model.
    for (auto& p : result.values()) {
        if (!p->parent().isNull())
            continue;
        this->newItem(p);
    }
}

void PartMetaModel::itemUpdated(PartMetaItem::PartMetaUpdateType type) {
    QSharedPointer<PartMetaItem> sender = static_cast<PartMetaItem*>(QObject::sender())->sharedFromThis();

    switch (type) {
        case ORNL::PartMetaItem::PartMetaUpdateType::kAddUpdate:
            emit itemAddedUpdate(sender);
            break;
        case ORNL::PartMetaItem::PartMetaUpdateType::kReloadUpdate:
            emit itemReloadUpdate(sender);
            break;
        case ORNL::PartMetaItem::PartMetaUpdateType::kRemoveUpdate:
            this->removeItem(sender);
            break;
        case ORNL::PartMetaItem::PartMetaUpdateType::kParentingUpdate:
            emit parentingUpdate(sender);
            break;
        case ORNL::PartMetaItem::PartMetaUpdateType::kSelectionUpdate:
            emit selectionUpdate(sender);
            break;
        case ORNL::PartMetaItem::PartMetaUpdateType::kVisualUpdate:
            emit visualUpdate(sender);
            break;
        case ORNL::PartMetaItem::PartMetaUpdateType::kTransformUpdate:
            emit transformUpdate(sender);
            break;
    }

    emit modelUpdated(sender);
}
} // namespace ORNL
