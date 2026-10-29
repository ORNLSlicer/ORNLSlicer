#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QUndoStack>
#include <cstdlib>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "widgets/part_widget/model/part_meta_model.h"
#include "widgets/part_widget/part_control/part_control.h"
#include "widgets/part_widget/part_control/part_control_tree_item.h"
#include "widgets/part_widget/right_click_menu.h"

namespace {
QSharedPointer<ORNL::PartMetaItem> addPart(QSharedPointer<ORNL::PartMetaModel> model, const QString& name,
                                           ORNL::MeshType type = ORNL::MeshType::kBuild) {
    auto mesh = QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(1000, 1000, 1000));
    mesh->setName(name);
    mesh->setType(type);
    auto part = QSharedPointer<ORNL::Part>::create(mesh, QString(), type);
    ORNL::CSM->addPart(part, false);
    return model->newItem(part);
}

bool triggerAction(ORNL::RightClickMenu& menu, QSharedPointer<ORNL::PartMetaItem> item, const QString& text,
                   const QString& parent = {}) {
    bool triggered = false;
    QTimer::singleShot(0, &menu, [&]() {
        for (auto* action : menu.actions()) {
            if (action->text() != text || !action->isEnabled()) continue;
            if (parent.isEmpty()) {
                action->trigger();
                triggered = true;
            }
            else if (action->menu()) {
                for (auto* choice : action->menu()->actions()) {
                    if (choice->text() == parent) {
                        choice->trigger();
                        triggered = true;
                        break;
                    }
                }
            }
            break;
        }
        menu.close();
    });
    menu.show(QPointF(10, 10), {item});
    return triggered;
}
}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    auto global = ORNL::GSM->getGlobal();
    global->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kPlanar));
    ORNL::PartControl control;
    control.resize(ORNL::Constants::UI::PartControl::kSize);
    QUndoStack undo;
    auto model = QSharedPointer<ORNL::PartMetaModel>::create();
    model->setUndoStack(&undo);
    control.setModel(model);
    auto parent   = addPart(model, "Parent A");
    auto other    = addPart(model, "Parent B");
    auto clipper  = addPart(model, "Clipper", ORNL::MeshType::kClipping);
    auto settings = addPart(model, "Settings", ORNL::MeshType::kSettings);
    for (const auto& item : model->items()) control.setFloatingStatus(item->part()->name(), false);
    auto* tree     = control.findChild<ORNL::PartControlTreeWidget*>();
    const auto row = [&](QSharedPointer<ORNL::PartMetaItem> item) {
        return static_cast<ORNL::PartControlTreeItem*>(
            tree->findItems(item->part()->name(), Qt::MatchExactly | Qt::MatchRecursive).first());
    };
    const auto has_row = [&](QSharedPointer<ORNL::PartMetaItem> item) {
        return tree->findItems(item->part()->name(), Qt::MatchExactly | Qt::MatchRecursive).size() == 1;
    };
    bool passed       = true;
    const auto expect = [&](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    control.show();
    app.processEvents();
    expect(tree->headerItem()->text(1) == "Applies to" && row(clipper)->text(1) == "All build objects",
           "The object list must identify unparented modifier scope.");

    ORNL::RightClickMenu menu(&control);
    expect(triggerAction(menu, clipper, "Assign Parent", "Parent A"), "Assign Parent must offer a valid build parent.");
    expect(clipper->parent() == parent && row(clipper)->parent() == row(parent) && row(clipper)->text(1) == "Parent A",
           "Assign Parent must update both the hierarchy and the visible scope.");
    parent->setMeshType(ORNL::MeshType::kClipping);
    expect(row(clipper)->text(1) == "Inactive" && row(clipper)->toolTip(1).contains("not a build mesh"),
           "Changing a build parent to a modifier must immediately mark its child inactive.");
    parent->setMeshType(ORNL::MeshType::kBuild);
    expect(row(clipper)->text(1) == "Parent A",
           "Changing the parent back to build must immediately restore its child's target label.");
    undo.undo();
    expect(
        clipper->parent().isNull() && row(clipper)->parent() == nullptr && row(clipper)->text(1) == "All build objects",
        "Undoing Assign Parent must restore the tree and global scope.");
    undo.redo();
    expect(row(clipper)->parent() == row(parent) && row(clipper)->text(1) == "Parent A",
           "Redoing Assign Parent must restore the tree and parent scope.");
    parent->renamePart("Renamed parent");
    expect(row(clipper)->text(1) == "Renamed parent", "Renaming a parent must refresh its child's target label.");
    expect(triggerAction(menu, clipper, "Disable Modifier(s)"), "The menu must allow disabling a modifier.");
    expect(!clipper->part()->modifierEnabled() && row(clipper)->text(1) == "Inactive",
           "Disabled modifiers must be visibly inactive.");
    expect(triggerAction(menu, clipper, "Enable Modifier(s)"), "The menu must allow re-enabling an inactive modifier.");
    expect(row(clipper)->text(1) == "Renamed parent", "Re-enabled modifiers must show their target.");
    expect(triggerAction(menu, clipper, "Detach from Parent"), "The menu must provide an explicit Detach action.");
    expect(clipper->parent().isNull() && row(clipper)->text(1) == "All build objects",
           "Detaching must restore global scope visibly.");

    expect(triggerAction(menu, settings, "Assign Parent", "Clipper"),
           "Parent choices must support existing grouping roles.");
    expect(row(settings)->text(1) == "Inactive" && row(settings)->toolTip(1).contains("not a build mesh"),
           "A modifier below another modifier must identify its ineffective target.");
    clipper->setMeshType(ORNL::MeshType::kBuild);
    expect(row(settings)->text(1) == "Clipper",
           "Converting a modifier parent to build must immediately activate its child's target label.");
    clipper->setMeshType(ORNL::MeshType::kClipping);
    expect(row(settings)->text(1) == "Inactive" && row(settings)->toolTip(1).contains("not a build mesh"),
           "Converting the parent back to a modifier must immediately mark its child inactive.");
    expect(triggerAction(menu, settings, "Assign Parent", "Renamed parent"),
           "Settings must be assignable to a build parent.");
    global->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kCylindrical));
    control.refreshModifierScopes();
    expect(row(settings)->text(1) == "Inactive" && row(settings)->toolTip(1).contains("Cylindrical"),
           "Unsupported cylindrical settings meshes must be marked inactive.");
    global->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kImage));
    control.refreshModifierScopes();
    expect(row(clipper)->text(1) == "Inactive" && row(clipper)->toolTip(1).contains("Image"),
           "Image-mode modifier limitations must be visible.");
    global->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kPlanar));
    control.refreshModifierScopes();

    tree->expandAll();
    app.processEvents();
    if (argc > 1) {
        QDir().mkpath(argv[1]);
        expect(control.grab().save(QString(argv[1]) + "/modifier-scopes.png"), "Could not capture modifier scope UI.");
        QTimer::singleShot(0, &menu, [&]() {
            expect(menu.grab().save(QString(argv[1]) + "/hierarchy-menu.png"), "Could not capture hierarchy actions.");
            menu.close();
        });
        menu.show(QPointF(10, 10), {parent});
    }

    tree->clearSelection();
    row(parent)->setSelected(true);
    expect(parent->isSelected() && row(parent)->isSelected(),
           "Keep-children deletion must exercise a selected parent.");
    expect(triggerAction(menu, parent, "Delete Object(s), Keep Children"),
           "The menu must offer a keep-children deletion.");
    expect(!model->items().contains(parent) && model->items().contains(settings) && !has_row(parent) &&
               has_row(settings) && has_row(other) && has_row(clipper) && !settings->part()->modifierEnabled() &&
               row(settings)->text(1) == "Inactive",
           "Keeping children must not silently make their modifier scope global.");
    expect(triggerAction(menu, settings, "Enable Modifier(s)"), "An orphaned inactive modifier must be recoverable.");
    expect(row(settings)->text(1) == "All build objects",
           "Re-enabling an unparented modifier must make its scope explicit.");

    expect(triggerAction(menu, clipper, "Assign Parent", "Parent B"),
           "A modifier must remain assignable after deletion.");
    tree->clearSelection();
    row(other)->setSelected(true);
    expect(other->isSelected() && row(other)->isSelected(), "Hierarchy deletion must exercise a selected parent.");
    expect(triggerAction(menu, other, "Delete Hierarchy"),
           "The default delete action must remove the complete hierarchy.");
    expect(!model->items().contains(other) && !model->items().contains(clipper) && !has_row(other) &&
               !has_row(clipper) && has_row(settings),
           "Delete Hierarchy must remove child modifiers with their parent.");
    undo.undo();
    expect(model->items().contains(other) && model->items().contains(clipper) && has_row(other) && has_row(clipper) &&
               has_row(settings) && clipper->parent() == other && row(clipper)->parent() == row(other) &&
               row(clipper)->text(1) == "Parent B",
           "Undoing hierarchy deletion must restore every row and its modifier target.");
    if (has_row(other)) {
        expect(other->isSelected() == row(other)->isSelected() &&
                   model->selectedItems().contains(other) == row(other)->isSelected(),
               "Undoing deletion must synchronize metadata selection with the restored tree row.");
        expect(other->isSelected() && row(other)->isSelected(),
               "Undoing deletion of a selected hierarchy must restore its parent selection.");
    }

    model->clearItems();
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
