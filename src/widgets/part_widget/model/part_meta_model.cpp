#include "widgets/part_widget/model/part_meta_model.h"

#include <QStack>
#include <QStringList>
#include <algorithm>
#include <limits>

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
namespace {
const QString kCopySuffix = "_copy";

QString instanceBaseName(const QString& name) {
    const int copy_index = name.lastIndexOf(kCopySuffix);
    if (copy_index < 0) return name;

    const QString suffix = name.mid(copy_index + kCopySuffix.size());
    if (suffix.isEmpty()) return name.left(copy_index);

    if (suffix.startsWith("_")) {
        bool suffix_is_number = false;
        suffix.mid(1).toUInt(&suffix_is_number);
        if (suffix_is_number) return name.left(copy_index);
    }

    return name;
}

bool isInstanceName(const QString& name, const QString& base_name) {
    if (name == base_name) return true;

    const QString copy_name = base_name + kCopySuffix;
    if (name == copy_name) return true;

    const QString numbered_copy_prefix = copy_name + "_";
    if (!name.startsWith(numbered_copy_prefix)) return false;

    bool suffix_is_number = false;
    name.mid(numbered_copy_prefix.size()).toUInt(&suffix_is_number);
    return suffix_is_number;
}

uint instanceSortIndex(const QString& name, const QString& base_name) {
    if (name == base_name) return 0;

    const QString copy_name = base_name + kCopySuffix;
    if (name == copy_name) return 1;

    const QString numbered_copy_prefix = copy_name + "_";
    if (name.startsWith(numbered_copy_prefix)) {
        bool suffix_is_number = false;
        const uint suffix     = name.mid(numbered_copy_prefix.size()).toUInt(&suffix_is_number);
        if (suffix_is_number) return suffix + 2;
    }

    return std::numeric_limits<uint>::max();
}

QString uniqueInstanceName(const QString& base_name, const QStringList& names) {
    if (!names.contains(base_name)) return base_name;

    const QString first_copy_name = base_name + kCopySuffix;
    if (!names.contains(first_copy_name)) return first_copy_name;

    uint count = 1;
    QString name;
    do {
        name = first_copy_name + "_" + QString::number(count);
        ++count;
    } while (names.contains(name));

    return name;
}

void setPartInstanceName(QSharedPointer<Part> part, const QString& name) {
    if (part.isNull()) return;

    part->setName(name);
    if (!part->rootMesh().isNull()) part->rootMesh()->setName(name);
}

QSharedPointer<Part> copyPartInstance(QSharedPointer<Part> source, const QString& name) {
    QSharedPointer<Part> copy = QSharedPointer<Part>::create(source);
    copy->setTransformation(QMatrix4x4());
    setPartInstanceName(copy, name);

    return copy;
}
}  // namespace

PartMetaModel::PartMetaModel() {
    // NOP
}

QList<QSharedPointer<PartMetaItem>> PartMetaModel::items() {
    return m_pointer_lookup.values();
}

QList<QSharedPointer<PartMetaItem>> PartMetaModel::selectedItems() {
    QList<QSharedPointer<PartMetaItem>> ret;
    for (auto& pm : m_pointer_lookup.values()) {
        if (pm->isSelected()) ret.append(pm);
    }

    return ret;
}

QSharedPointer<PartMetaItem> PartMetaModel::lookupByGraphic(QSharedPointer<PartObject> gop) {
    if (gop.isNull()) return nullptr;
    return this->lookupByPointer(gop->part());
}

QSharedPointer<PartMetaItem> PartMetaModel::lookupByTreeItem(PartControlTreeItem* item) {
    if (!item) return nullptr;
    return this->lookupByPointer(item->getPart());
}

QSharedPointer<PartMetaItem> PartMetaModel::lookupByPointer(QSharedPointer<Part> p) {
    if (!m_pointer_lookup.contains(p)) return nullptr;
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
    for (auto& c : pm->part()->children()) { queue.push(c); }

    while (!queue.empty()) {
        QSharedPointer<Part> curr_part = queue.pop();

        for (auto& c : curr_part->children()) { queue.push(c); }

        QSharedPointer<PartMetaItem> cpm = QSharedPointer<PartMetaItem>::create(curr_part);
        QObject::connect(cpm.get(), &PartMetaItem::modified, this, &PartMetaModel::itemUpdated);
        emit itemAddedUpdate(cpm);

        item_lookup[curr_part->parent()]->blockSignals(true);
        item_lookup[curr_part->parent()]->adoptChild(cpm);
        item_lookup[curr_part->parent()]->blockSignals(false);
        item_lookup[curr_part]      = cpm;
        m_pointer_lookup[curr_part] = cpm;

        cpm->setModel(this->sharedFromThis());
    }
}

void PartMetaModel::replaceItem(QSharedPointer<PartMetaItem> pm, QString filename) {
    if (pm.isNull()) return;

    CSM->replacePart(pm, filename);
}

void PartMetaModel::reloadItem(QSharedPointer<PartMetaItem> pm) {
    if (pm.isNull()) return;

    CSM->reloadPart(pm);
}

void PartMetaModel::removeItem(QSharedPointer<PartMetaItem> pm) {
    if (pm.isNull()) return;

    m_pointer_lookup.remove(pm->part());

    // Children of this item need to find new parents. This item's parent can adopt.
    auto pm_par = pm->parent();
    if (pm_par.isNull()) {
        for (auto pm_child : pm->children()) { pm->orphanChild(pm_child); }
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
    for (auto& pm : m_pointer_lookup.values()) { this->removeItem(pm); }
}

void PartMetaModel::setSelectionCopied() {
    m_copied_list = this->selectedItems();
}

void PartMetaModel::copySelection() {
    QStack<QSharedPointer<PartMetaItem>> queue;
    for (auto& pm : m_copied_list) { queue.push(pm); }

    QStringList namelist;
    for (auto& p : m_pointer_lookup.keys()) { namelist.append(p->name()); }

    QMap<QSharedPointer<PartMetaItem>, QSharedPointer<Part>> result;

    while (!queue.empty()) {
        QSharedPointer<PartMetaItem> pm = queue.pop();

        for (auto& cpm : pm->children()) { queue.push(cpm); }

        // Copy part.
        QSharedPointer<Part> p = QSharedPointer<Part>::create(pm->part());
        p->setTransformation(QMatrix4x4());
        p->setName(p->name() + "_copy");

        QString org_name = p->name();
        uint count       = 1;

        // Find a new name.
        while (namelist.contains(p->name())) {
            p->setName(org_name + "_" + QString::number(count));
            count++;
        }
        setPartInstanceName(p, p->name());

        result[pm] = p;

        // If this item has a parent, link it up.
        if (result.contains(pm->parent())) { result[pm->parent()]->adoptChild(p); }
    }

    // Add to model.
    for (auto& p : result.values()) {
        if (!p->parent().isNull()) continue;
        this->newItem(p);
    }
}

int PartMetaModel::instanceCount(QSharedPointer<PartMetaItem> pm) {
    return this->instanceItems(pm).size();
}

void PartMetaModel::setInstanceCount(QSharedPointer<PartMetaItem> pm, int count) {
    if (pm.isNull() || count < 1) return;

    QList<QSharedPointer<PartMetaItem>> instances = this->instanceItems(pm);
    const QString base_name                       = instanceBaseName(pm->part()->name());

    while (instances.size() > count) {
        int remove_index = -1;
        for (int i = instances.size() - 1; i >= 0; --i) {
            if (instances.at(i) != pm) {
                remove_index = i;
                break;
            }
        }

        if (remove_index < 0) break;

        QSharedPointer<PartMetaItem> item = instances.takeAt(remove_index);
        this->removeItem(item);
    }

    if (instances.size() >= count) return;

    QStringList namelist;
    for (auto& p : m_pointer_lookup.keys()) { namelist.append(p->name()); }

    while (instances.size() < count) {
        const QString name        = uniqueInstanceName(base_name, namelist);
        QSharedPointer<Part> copy = copyPartInstance(pm->part(), name);

        CSM->addPart(copy, false);
        setPartInstanceName(copy, copy->name());
        namelist.append(copy->name());
        instances.append(this->newItem(copy));
    }
}

QList<QSharedPointer<PartMetaItem>> PartMetaModel::instanceItems(QSharedPointer<PartMetaItem> pm) {
    QList<QSharedPointer<PartMetaItem>> instances;
    if (pm.isNull()) return instances;

    const QString base_name = instanceBaseName(pm->part()->name());
    for (auto& item : m_pointer_lookup.values()) {
        if (isInstanceName(item->part()->name(), base_name)) instances.append(item);
    }

    std::sort(instances.begin(), instances.end(),
              [base_name](const QSharedPointer<PartMetaItem>& lhs, const QSharedPointer<PartMetaItem>& rhs) {
                  return instanceSortIndex(lhs->part()->name(), base_name) <
                         instanceSortIndex(rhs->part()->name(), base_name);
              });

    return instances;
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
}  // namespace ORNL
