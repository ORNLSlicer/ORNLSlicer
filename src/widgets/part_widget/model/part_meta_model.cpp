#include "widgets/part_widget/model/part_meta_model.h"

#include <QStack>
#include <QStringList>
#include <QUndoCommand>
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
#include "utilities/constants.h"
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

}  // namespace

class HierarchyUndoCommand : public QUndoCommand {
   public:
    HierarchyUndoCommand(QSharedPointer<PartMetaModel> model, const QString& text,
                         const QList<PartMetaModel::HierarchyState>& before,
                         const QList<PartMetaModel::HierarchyState>& after)
        : QUndoCommand(text), m_model(model), m_before(before), m_after(after) {
        for (const auto& state : before) m_selection[state.item] = state.item->isSelected();
    }

    void undo() override {
        apply(m_before);
    }
    void redo() override {
        apply(m_after);
    }

   private:
    void apply(const QList<PartMetaModel::HierarchyState>& states) {
        if (auto model = m_model.toStrongRef()) {
            model->applyHierarchyState(states);
            for (const auto& state : states) {
                if (state.present) state.item->setSelected(m_selection.value(state.item));
            }
            model->m_changing_hierarchy = false;
            emit model->hierarchyChanged();
        }
    }

    QWeakPointer<PartMetaModel> m_model;
    QMap<QSharedPointer<PartMetaItem>, bool> m_selection;
    QList<PartMetaModel::HierarchyState> m_before;
    QList<PartMetaModel::HierarchyState> m_after;
};

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
    if (m_pointer_lookup.contains(p)) return m_pointer_lookup[p];

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
        m_pointer_lookup[curr_part]      = cpm;
        cpm->setModel(this->sharedFromThis());
        QObject::connect(cpm.get(), &PartMetaItem::modified, this, &PartMetaModel::itemUpdated);
        emit itemAddedUpdate(cpm);

        item_lookup[curr_part->parent()]->blockSignals(true);
        item_lookup[curr_part->parent()]->adoptChild(cpm);
        item_lookup[curr_part->parent()]->blockSignals(false);
        item_lookup[curr_part] = cpm;
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
    if (pm.isNull() || !m_pointer_lookup.contains(pm->part())) return;

    m_pointer_lookup.remove(pm->part());

    // Children of this item need to find new parents. This item's parent can adopt.
    auto pm_par = pm->parent();
    if (pm_par.isNull()) {
        for (auto pm_child : pm->children()) {
            if (pm_child->meshType() == MeshType::kClipping || pm_child->meshType() == MeshType::kSettings)
                pm_child->part()->setModifierEnabled(false);
            pm->orphanChild(pm_child);
        }
    }
    else {
        pm_par->orphanChild(pm);

        for (auto pm_child : pm->children()) {
            if (pm_child->meshType() == MeshType::kClipping || pm_child->meshType() == MeshType::kSettings)
                pm_child->part()->setModifierEnabled(false);
            pm->orphanChild(pm_child);
            pm_par->adoptChild(pm_child);
        }
    }

    QObject::disconnect(pm.get(), &PartMetaItem::modified, this, &PartMetaModel::itemUpdated);
    // Keep source data available so undo can restore a file-backed object and save it again.
    CSM->removePart(pm->part()->name());
    emit itemRemovedUpdate(pm);
}

void PartMetaModel::clearItems() {
    if (m_undo_stack) m_undo_stack->clear();
    for (auto& pm : m_pointer_lookup.values()) { this->removeItem(pm); }
}

void PartMetaModel::setUndoStack(QUndoStack* stack) {
    m_undo_stack = stack;
}

bool PartMetaModel::hierarchyChangeInProgress() const {
    return m_changing_hierarchy;
}

QList<PartMetaModel::HierarchyState> PartMetaModel::hierarchyState(const QList<QSharedPointer<PartMetaItem>>& items) {
    QList<HierarchyState> states;
    for (const auto& item : items) states.append({item, item->parent(), item->part()->modifierEnabled()});
    return states;
}

void PartMetaModel::applyHierarchyState(const QList<HierarchyState>& states) {
    m_changing_hierarchy = true;
    for (const auto& state : states) {
        state.item->part()->setModifierEnabled(state.enabled);
        state.item->setParent(nullptr);
    }
    for (const auto& state : states) {
        if (!state.present) removeItem(state.item);
    }
    for (const auto& state : states) {
        if (state.present && !m_pointer_lookup.contains(state.item->part())) {
            CSM->addPart(state.item->part(), false);
            setPartInstanceName(state.item->part(), state.item->part()->name());
            addItem(state.item);
        }
    }
    for (const auto& state : states) {
        if (state.present) {
            state.item->setParent(state.parent);
            emit visualUpdate(state.item);
        }
    }
}

void PartMetaModel::changeHierarchy(const QString& text, const QList<HierarchyState>& before,
                                    const QList<HierarchyState>& after) {
    if (m_undo_stack)
        m_undo_stack->push(new HierarchyUndoCommand(sharedFromThis(), text, before, after));
    else {
        HierarchyUndoCommand command(sharedFromThis(), text, before, after);
        command.redo();
    }
}

bool PartMetaModel::reparentItems(QList<QSharedPointer<PartMetaItem>> items, QSharedPointer<PartMetaItem> parent) {
    if (!parent.isNull() && !m_pointer_lookup.contains(parent->part())) return false;
    QList<QSharedPointer<PartMetaItem>> changed;
    for (const auto& item : items) {
        if (item.isNull() || !m_pointer_lookup.contains(item->part())) return false;
        for (auto ancestor = parent; !ancestor.isNull(); ancestor = ancestor->parent()) {
            if (ancestor == item) return false;
        }
        bool ancestor_selected = false;
        for (auto ancestor = item->parent(); !ancestor.isNull(); ancestor = ancestor->parent()) {
            if (items.contains(ancestor)) ancestor_selected = true;
        }
        if (ancestor_selected) continue;
        if ((item->parent() != parent || !item->part()->modifierEnabled()) && !changed.contains(item))
            changed.append(item);
    }
    if (changed.isEmpty()) return true;
    auto before = hierarchyState(changed);
    auto after  = before;
    for (auto& state : after) {
        state.parent  = parent;
        state.enabled = true;
    }
    changeHierarchy(parent.isNull() ? "Detach objects" : "Assign parent", before, after);
    return true;
}

void PartMetaModel::deleteItems(QList<QSharedPointer<PartMetaItem>> items, bool delete_children) {
    QList<QSharedPointer<PartMetaItem>> removed;
    for (const auto& item : items) {
        if (!item.isNull() && m_pointer_lookup.contains(item->part()) && !removed.contains(item)) removed.append(item);
    }
    if (removed.isEmpty()) return;
    if (delete_children) {
        for (int i = 0; i < removed.size(); ++i) {
            for (const auto& child : removed[i]->children()) {
                if (!removed.contains(child)) removed.append(child);
            }
        }
    }
    auto affected = removed;
    for (const auto& item : removed) {
        for (const auto& child : item->children()) {
            if (!affected.contains(child)) affected.append(child);
        }
    }
    auto before = hierarchyState(affected);
    auto after  = before;
    for (auto& state : after) {
        if (removed.contains(state.item)) {
            state.present = false;
            state.parent  = nullptr;
        }
        else {
            while (removed.contains(state.parent)) state.parent = state.parent->parent();
            if (state.item->meshType() == MeshType::kClipping || state.item->meshType() == MeshType::kSettings)
                state.enabled = false;
        }
    }
    changeHierarchy(delete_children ? "Delete hierarchy" : "Delete objects, keep children", before, after);
}

void PartMetaModel::setModifiersEnabled(QList<QSharedPointer<PartMetaItem>> items, bool enabled) {
    QList<QSharedPointer<PartMetaItem>> changed;
    for (const auto& item : items) {
        if (!item.isNull() && m_pointer_lookup.contains(item->part()) &&
            (item->meshType() == MeshType::kClipping || item->meshType() == MeshType::kSettings) &&
            item->part()->modifierEnabled() != enabled && !changed.contains(item))
            changed.append(item);
    }
    if (changed.isEmpty()) return;
    auto before = hierarchyState(changed);
    auto after  = before;
    for (auto& state : after) state.enabled = enabled;
    changeHierarchy(enabled ? "Enable modifiers" : "Disable modifiers", before, after);
}

void PartMetaModel::setSelectionCopied() {
    m_copied_list = this->selectedItems();
}

void PartMetaModel::copySelection() {
    for (const auto& item : m_copied_list) {
        bool ancestor_copied = false;
        for (auto parent = item->parent(); !parent.isNull(); parent = parent->parent()) {
            if (m_copied_list.contains(parent)) ancestor_copied = true;
        }
        if (!ancestor_copied) duplicateItem(item);
    }
}

QSharedPointer<PartMetaItem> PartMetaModel::duplicateItem(QSharedPointer<PartMetaItem> item) {
    QStack<QSharedPointer<PartMetaItem>> queue;
    queue.push(item);

    QStringList namelist;
    for (auto& p : m_pointer_lookup.keys()) { namelist.append(p->name()); }

    QMap<QSharedPointer<PartMetaItem>, QSharedPointer<Part>> result;

    while (!queue.empty()) {
        QSharedPointer<PartMetaItem> pm = queue.pop();

        for (auto& cpm : pm->children()) { queue.push(cpm); }

        // Copy part.
        QSharedPointer<Part> p = QSharedPointer<Part>::create(pm->part());
        QMatrix4x4 transform;
        if (!item->parent().isNull() || !item->children().isEmpty()) {
            transform = pm->transformation();
            for (int axis = 0; axis < 3; ++axis) transform(axis, 3) *= Constants::OpenGL::kViewToObject;
        }
        p->setTransformation(transform);
        p->setMeshType(pm->meshType());
        p->rootMesh()->setType(pm->meshType());
        setPartInstanceName(p, uniqueInstanceName(instanceBaseName(p->name()), namelist));
        namelist.append(p->name());

        result[pm] = p;

        // If this item has a parent, link it up.
        if (result.contains(pm->parent())) { result[pm->parent()]->adoptChild(p); }
    }

    for (auto& p : result.values()) {
        CSM->addPart(p, false);
        setPartInstanceName(p, p->name());
    }

    if (!item->parent().isNull()) item->parent()->part()->adoptChild(result[item]);
    auto copy = this->newItem(result[item]);
    // Copying a child alone keeps it under the same parent, including local modifiers.
    copy->setParent(item->parent());
    return copy;
}

int PartMetaModel::instanceCount(QSharedPointer<PartMetaItem> pm) {
    return this->instanceItems(pm).size();
}

void PartMetaModel::setInstanceCount(QSharedPointer<PartMetaItem> pm, int count) {
    if (pm.isNull() || count < 1) return;

    QList<QSharedPointer<PartMetaItem>> instances = this->instanceItems(pm);

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
        this->deleteItems({item}, true);
    }

    if (instances.size() >= count) return;

    while (instances.size() < count) { instances.append(duplicateItem(pm)); }
}

QList<QSharedPointer<PartMetaItem>> PartMetaModel::instanceItems(QSharedPointer<PartMetaItem> pm) {
    QList<QSharedPointer<PartMetaItem>> instances;
    if (pm.isNull()) return instances;

    const QString base_name = instanceBaseName(pm->part()->name());
    for (auto& item : m_pointer_lookup.values()) {
        if (item->parent() == pm->parent() && isInstanceName(item->part()->name(), base_name)) instances.append(item);
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
        case ORNL::PartMetaItem::PartMetaUpdateType::kNameUpdate:
            emit nameUpdate(sender);
            break;
    }

    emit modelUpdated(sender);
}
}  // namespace ORNL
