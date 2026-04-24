#include "widgets/part_widget/part_control/part_control.h"

#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QListWidget>
#include <QMenu>
#include <QStack>
#include <QtWidgets/QInputDialog>
#include <qabstractitemview.h>
#include <qboxlayout.h>
#include <qfiledevice.h>
#include <qlist.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qtreewidget.h>
#include <qwidget.h>

#include "gcode/writers/writer_base.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/model/part_meta_model.h"
#include "widgets/part_widget/part_control/part_control_tree_item.h"
#include "widgets/part_widget/part_control/part_control_tree_widget.h"
#include "widgets/part_widget/right_click_menu.h"

namespace ORNL {
PartControl::PartControl(QWidget* parent) : QWidget(parent) {
    this->setupStyle();
    this->setupSubWidgets();
    this->setupLayouts();
    this->setupEvents();
}

int PartControl::count() { return m_tree_widget->topLevelItemCount(); }

QString PartControl::nameOfFirstPart() {
    return ((PartControlTreeItem*)m_tree_widget->topLevelItem(0))->getPart()->name();
    ;
}

void PartControl::setModel(QSharedPointer<PartMetaModel> m) {
    m_model = m;

    QObject::connect(m_model.get(), &PartMetaModel::parentingUpdate, this, &PartControl::modelParentingUpdate);
    QObject::connect(m_model.get(), &PartMetaModel::selectionUpdate, this, &PartControl::modelSelectionUpdate);
    QObject::connect(m_model.get(), &PartMetaModel::itemAddedUpdate, this, &PartControl::modelAdditionUpdate);
    QObject::connect(m_model.get(), &PartMetaModel::itemRemovedUpdate, this, &PartControl::modelRemovalUpdate);
    QObject::connect(m_model.get(), &PartMetaModel::visualUpdate, this, &PartControl::modelVisualUpdate);
}

void PartControl::modelAdditionUpdate(QSharedPointer<PartMetaItem> pm) {
    QList<QTreeWidgetItem*> tl = m_tree_widget->findItems(pm->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);
    PartControlTreeItem* tree_item;

    // New item.
    if (tl.empty()) {
        tree_item = new PartControlTreeItem(pm);
        m_tree_widget->insertTopLevelItem(0, tree_item);
        tree_item->addContainer(m_tree_widget, 0);
    }
}

void PartControl::modelRemovalUpdate(QSharedPointer<PartMetaItem> pm) {
    QList<QTreeWidgetItem*> tl = m_tree_widget->findItems(pm->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);
    PartControlTreeItem* tree_item = tree_item = (PartControlTreeItem*)tl.at(0);
    PartControlTreeItem* tree_item_parent = (PartControlTreeItem*)tree_item->parent();

    // Move children to parent.
    QList<QTreeWidgetItem*> child_list = tree_item->takeChildren();
    if (tree_item_parent)
        tree_item_parent->addChildren(child_list);
    else
        m_tree_widget->addTopLevelItems(child_list);

    delete tree_item;
}

void PartControl::modelSelectionUpdate(QSharedPointer<PartMetaItem> pm) {
    disconnect(m_tree_widget, &PartControlTreeWidget::itemSelectionChanged, this, &PartControl::handleSelectionChange);
    QList<QTreeWidgetItem*> tl = m_tree_widget->findItems(pm->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);
    PartControlTreeItem* tree_item = tree_item = (PartControlTreeItem*)tl.at(0);

    if (pm->isSelected()) {
        tree_item->setSelected(true);
        m_tree_widget->scrollToItem(tree_item);

        PartControlTreeItem* p = (PartControlTreeItem*)tree_item->parent();
        while (p) {
            p->setExpanded(true);
            p = (PartControlTreeItem*)p->parent();
        }
    }
    else {
        tree_item->setSelected(false);
    }
    connect(m_tree_widget, &PartControlTreeWidget::itemSelectionChanged, this, &PartControl::handleSelectionChange);
}

void PartControl::modelParentingUpdate(QSharedPointer<PartMetaItem> pm) {
    QList<QTreeWidgetItem*> tl = m_tree_widget->findItems(pm->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);
    PartControlTreeItem* tree_item = tree_item = (PartControlTreeItem*)tl.at(0);

    // Add to top level by default.
    m_tree_widget->insertTopLevelItem(0, tree_item);

    // If this item has a parent, try to move it to the parent's child group.
    if (!pm->parent().isNull()) {
        QList<QTreeWidgetItem*> tlp =
            m_tree_widget->findItems(pm->parent()->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);
        if (!tlp.empty()) {
            m_tree_widget->takeTopLevelItem(m_tree_widget->indexOfTopLevelItem(tree_item));
            tlp.at(0)->addChild(tree_item);
        }
    }

    // Look at this item's children and move them to be sub items in the view.
    for (QSharedPointer<PartMetaItem> child : pm->children()) {
        QList<QTreeWidgetItem*> tlc =
            m_tree_widget->findItems(child->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);

        if (!tlc.empty()) {
            m_tree_widget->takeTopLevelItem(m_tree_widget->indexOfTopLevelItem(tlc.at(0)));
            tree_item->addChild(tlc.at(0));
        }
    }
}

void PartControl::modelVisualUpdate(QSharedPointer<PartMetaItem> pm) {
    QList<QTreeWidgetItem*> tl = m_tree_widget->findItems(pm->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);
    PartControlTreeItem* tree_item = tree_item = (PartControlTreeItem*)tl.at(0);

    tree_item->updateMeshType(pm->meshType());
}

void PartControl::handleSelectionChange() {
    QObject::disconnect(m_model.get(), &PartMetaModel::selectionUpdate, this, &PartControl::modelSelectionUpdate);

    // Look at all items and check if there has been a change to it.
    QList<QTreeWidgetItem*> tl = m_tree_widget->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
    for (auto curr_item : tl) {
        QSharedPointer<PartMetaItem> meta = m_model->lookupByTreeItem((PartControlTreeItem*)curr_item);
        if (meta.isNull())
            continue;

        if (meta->graphicsPart()->locked()) {
            curr_item->setSelected(false);
            continue;
        }

        // Note: this extra checking here is to prevent unnecessary calls to views.
        if (curr_item->isSelected() && !meta->isSelected()) {
            meta->setSelected(true);
        }
        else if (!curr_item->isSelected() && meta->isSelected()) {
            meta->setSelected(false);
        }
    }

    QObject::connect(m_model.get(), &PartMetaModel::selectionUpdate, this, &PartControl::modelSelectionUpdate);
}

void PartControl::handleParentingChange() {
    QObject::disconnect(m_model.get(), &PartMetaModel::parentingUpdate, this, &PartControl::modelParentingUpdate);

    QList<QTreeWidgetItem*> tl = m_tree_widget->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
    for (auto curr_item : tl) {
        QSharedPointer<PartMetaItem> meta = m_model->lookupByTreeItem((PartControlTreeItem*)curr_item);
        QSharedPointer<PartMetaItem> item_parent = m_model->lookupByTreeItem((PartControlTreeItem*)curr_item->parent());

        // If the model's parent does not match the item's parent, then we need to update the model.
        if (meta->parent() != item_parent) {
            // Parent orphans the child.
            if (!meta->parent().isNull())
                meta->parent()->orphanChild(meta);

            // New parent adopts the child.
            if (!item_parent.isNull())
                item_parent->adoptChild(meta);

            QList<QTreeWidgetItem*> tl =
                m_tree_widget->findItems(meta->part()->name(), Qt::MatchExactly | Qt::MatchRecursive);
            PartControlTreeItem* tree_item = tree_item = (PartControlTreeItem*)tl.at(0);

            m_tree_widget->removeItemWidget(tree_item, 0);
            tree_item->addContainer(m_tree_widget, 0);
        }
    }

    QObject::connect(m_model.get(), &PartMetaModel::parentingUpdate, this, &PartControl::modelParentingUpdate);
}

void PartControl::setFloatingStatus(const QString& name, bool status) {
    QList<QTreeWidgetItem*> search = m_tree_widget->findItems(name, Qt::MatchExactly | Qt::MatchRecursive);

    for (auto result : search) {
        auto tree_item = dynamic_cast<PartControlTreeItem*>(result);
        if (tree_item != nullptr) {
            tree_item->setFloating(status);
        }
    }
}

void PartControl::setOutsideStatus(const QString& name, bool status) {
    QList<QTreeWidgetItem*> search = m_tree_widget->findItems(name, Qt::MatchExactly | Qt::MatchRecursive);

    for (auto result : search) {
        auto tree_item = dynamic_cast<PartControlTreeItem*>(result);
        if (tree_item != nullptr) {
            tree_item->setOutsideVolume(status);
        }
    }
}

void PartControl::setupSubWidgets() {
    m_tree_widget = new PartControlTreeWidget(this);
    m_tree_widget->resize(Constants::UI::PartControl::kSize);
    m_tree_widget->setHeaderHidden(true);
    m_tree_widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree_widget->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree_widget->show();

    m_right_click_menu = new RightClickMenu(this);
}

void PartControl::setupLayouts() {
    m_layout = new QVBoxLayout(this);
    m_layout->addWidget(m_tree_widget);
}

void PartControl::setupEvents() {
    // Setup context right-click menu
    m_tree_widget->setContextMenuPolicy(Qt::CustomContextMenu);

    // Tree Widget -> Model
    connect(m_tree_widget, &PartControlTreeWidget::itemSelectionChanged, this, &PartControl::handleSelectionChange);
    connect(m_tree_widget, &PartControlTreeWidget::dropEnded, this, &PartControl::handleParentingChange);

    connect(m_tree_widget, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto list_item = dynamic_cast<PartControlTreeItem*>(m_tree_widget->itemAt(pos));

        if (list_item != nullptr)
            m_right_click_menu->show(mapToGlobal(pos), m_model->selectedItems());
        else
            m_right_click_menu->show(mapToGlobal(pos), QList<QSharedPointer<PartMetaItem>>());
    });
}

void PartControl::setupStyle() {
    // Load stylesheet
    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "part_control.qss"));
    style->open(QIODevice::ReadOnly);
    this->setStyleSheet(style->readAll());
    style->close();

    // Add drop shadow
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);
}
} // namespace ORNL
