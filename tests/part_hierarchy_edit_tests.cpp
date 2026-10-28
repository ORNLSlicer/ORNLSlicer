#include <QCoreApplication>
#include <QStandardPaths>
#include <QUndoStack>
#include <cstdlib>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "slicing/slicing_utilities.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/mathutils.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace {
using Item = QSharedPointer<ORNL::PartMetaItem>;

struct Scene {
    QUndoStack undo;
    QSharedPointer<ORNL::PartMetaModel> model = QSharedPointer<ORNL::PartMetaModel>::create();

    Scene() {
        model->setUndoStack(&undo);
    }

    ~Scene() {
        model->clearItems();
        ORNL::CSM->clearParts();
    }

    Item add(const QString& name, ORNL::MeshType type = ORNL::MeshType::kBuild) {
        auto mesh = QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(1000, 2000, 3000));
        mesh->setName(name);
        mesh->setType(type);
        auto part = QSharedPointer<ORNL::Part>::create(mesh, QString(), type);
        ORNL::CSM->addPart(part, false);
        auto item       = model->newItem(part);
        QMatrix4x4 pose = ORNL::MathUtils::composeTransformMatrix(
            QVector3D(model->items().size(), 2, 3), QQuaternion::fromEulerAngles(10, 20, 30), QVector3D(1, 2, 3));
        item->setTransformation(pose);
        for (int axis = 0; axis < 3; ++axis) pose(axis, 3) *= ORNL::Constants::OpenGL::kViewToObject;
        part->setTransformation(pose);
        return item;
    }

    Item find(const QString& name) {
        return model->lookupByPointer(ORNL::CSM->getPart(name));
    }
};

bool linked(Item child, Item parent) {
    return child->parent() == parent && child->part()->parent() == parent->part() &&
           parent->children().count(child) == 1 && parent->part()->children().count(child->part()) == 1;
}

bool reparentUndo() {
    Scene scene;
    auto first    = scene.add("first");
    auto second   = scene.add("second");
    auto child    = scene.add("child");
    auto clipper  = scene.add("clipper", ORNL::MeshType::kClipping);
    auto settings = scene.add("settings", ORNL::MeshType::kSettings);
    first->adoptChild(child);
    child->adoptChild(clipper);
    first->adoptChild(settings);
    settings->part()->setModifierEnabled(false);
    QMap<Item, QMatrix4x4> poses;
    QMap<Item, QMatrix4x4> mesh_poses;
    for (const auto& item : scene.model->items()) {
        poses[item]      = item->transformation();
        mesh_poses[item] = item->part()->rootMesh()->transformation();
    }
    int changes = 0;
    QObject::connect(scene.model.get(), &ORNL::PartMetaModel::hierarchyChanged, [&] { ++changes; });

    bool passed = ORNL::Testing::expect(scene.model->reparentItems({child, settings, settings}, second),
                                        "Batch parenting should accept valid objects.");
    passed &= ORNL::Testing::expect(scene.undo.count() == 1 && changes == 1 && linked(child, second) &&
                                        linked(settings, second) && linked(clipper, child) &&
                                        first->children().isEmpty() && settings->part()->modifierEnabled(),
                                    "Batch parenting must be atomic, preserve descendants, and enable modifiers.");
    scene.undo.undo();
    passed &= ORNL::Testing::expect(linked(child, first) && linked(settings, first) &&
                                        !settings->part()->modifierEnabled() && second->children().isEmpty(),
                                    "Undo must restore both original parents and modifier state.");
    scene.undo.redo();
    passed &= ORNL::Testing::expect(linked(child, second) && linked(settings, second),
                                    "Redo must restore both parents in one operation.");
    passed &= ORNL::Testing::expect(scene.model->reparentItems({child, settings}, nullptr) && scene.undo.count() == 2 &&
                                        child->parent().isNull() && settings->parent().isNull() &&
                                        settings->part()->modifierEnabled() && linked(clipper, child),
                                    "Batch detach must be atomic and keep nested ownership intact.");
    scene.undo.undo();
    passed &= ORNL::Testing::expect(linked(child, second) && linked(settings, second),
                                    "Undoing detach must restore the group.");
    scene.undo.redo();
    const int count = scene.undo.count();
    passed &= ORNL::Testing::expect(!scene.model->reparentItems({child}, clipper) &&
                                        !scene.model->reparentItems({settings}, settings) &&
                                        scene.undo.count() == count && linked(clipper, child) && changes == 6,
                                    "Cycles and self-parenting must be rejected without modifying undo history.");
    scene.model->setModifiersEnabled({child, clipper, settings}, false);
    passed &= ORNL::Testing::expect(
        scene.undo.count() == count + 1 && child->part()->modifierEnabled() && !clipper->part()->modifierEnabled() &&
            !settings->part()->modifierEnabled() && linked(clipper, child),
        "Batch modifier deactivation must be one undo operation and leave build objects alone.");
    scene.undo.undo();
    passed &= ORNL::Testing::expect(clipper->part()->modifierEnabled() && settings->part()->modifierEnabled() &&
                                        settings->parent().isNull() && linked(clipper, child),
                                    "Undoing modifier deactivation must preserve the original targets.");
    for (const auto& item : scene.model->items()) {
        passed &= ORNL::Testing::expect(
            item->transformation() == poses[item] && item->part()->rootMesh()->transformation() == mesh_poses[item],
            "Parenting, detaching, undo, and redo must preserve world placement.");
    }
    return passed;
}

bool deletionUndo() {
    Scene scene;
    auto parent   = scene.add("parent");
    auto child    = scene.add("child");
    auto clipper  = scene.add("clipper", ORNL::MeshType::kClipping);
    auto settings = scene.add("settings", ORNL::MeshType::kSettings);
    auto other    = scene.add("other");
    parent->adoptChild(child);
    child->adoptChild(clipper);
    parent->adoptChild(settings);
    settings->part()->setModifierEnabled(false);
    scene.model->deleteItems({parent, settings}, true);
    bool passed =
        ORNL::Testing::expect(scene.undo.count() == 1 && scene.model->items().size() == 1 && ORNL::CSM->count() == 1 &&
                                  scene.find("other") == other,
                              "Deleting a hierarchy must remove every descendant from the model and session.");
    for (const auto& item : {parent, child, clipper, settings}) {
        passed &= ORNL::Testing::expect(
            !scene.find(item->part()->name()) && item->parent().isNull() && item->children().isEmpty(),
            "Deleted hierarchy members must not retain live ownership links.");
    }
    scene.undo.undo();
    passed &= ORNL::Testing::expect(
        scene.model->items().size() == 5 && ORNL::CSM->count() == 5 && scene.find("parent") == parent &&
            linked(child, parent) && linked(clipper, child) && linked(settings, parent) &&
            !settings->part()->modifierEnabled() && clipper->meshType() == ORNL::MeshType::kClipping &&
            settings->meshType() == ORNL::MeshType::kSettings,
        "Undo must restore session membership, roles, enabled state, and complete hierarchy.");
    scene.undo.redo();
    passed &= ORNL::Testing::expect(scene.model->items().size() == 1 && ORNL::CSM->count() == 1,
                                    "Redo must remove the same complete hierarchy.");
    return passed;
}

bool keepChildrenScope() {
    Scene scene;
    auto parent   = scene.add("parent");
    auto clipper  = scene.add("clipper", ORNL::MeshType::kClipping);
    auto settings = scene.add("settings", ORNL::MeshType::kSettings);
    auto other    = scene.add("other");
    parent->adoptChild(clipper);
    parent->adoptChild(settings);
    scene.model->deleteItems({parent}, false);
    bool passed = ORNL::Testing::expect(
        scene.model->items().size() == 3 && clipper->parent().isNull() && settings->parent().isNull() &&
            !clipper->part()->modifierEnabled() && !settings->part()->modifierEnabled() &&
            ORNL::SlicingUtilities::GetPartsByType(ORNL::CSM->parts(), ORNL::MeshType::kClipping, other->part())
                .isEmpty() &&
            ORNL::SlicingUtilities::GetPartsByType(ORNL::CSM->parts(), ORNL::MeshType::kSettings, other->part())
                .isEmpty(),
        "Keeping children must not turn local modifiers into active global modifiers.");
    scene.undo.undo();
    passed &= ORNL::Testing::expect(linked(clipper, parent) && linked(settings, parent) &&
                                        clipper->part()->modifierEnabled() && settings->part()->modifierEnabled(),
                                    "Undo must restore modifier scope and activation.");
    scene.undo.redo();
    passed &= ORNL::Testing::expect(
        scene.model->reparentItems({clipper}, other) && linked(clipper, other) && clipper->part()->modifierEnabled() &&
            scene.model->reparentItems({settings}, nullptr) && settings->part()->modifierEnabled(),
        "Explicit assignment and detach must reactivate surviving modifiers.");
    scene.undo.undo();
    scene.undo.undo();
    passed &= ORNL::Testing::expect(!clipper->part()->modifierEnabled() && !settings->part()->modifierEnabled(),
                                    "Undoing explicit activation must restore the inactive surviving modifiers.");
    scene.undo.undo();
    auto grandparent = scene.add("grandparent");
    grandparent->adoptChild(parent);
    scene.undo.clear();
    scene.model->deleteItems({parent}, false);
    passed &=
        ORNL::Testing::expect(linked(clipper, grandparent) && linked(settings, grandparent) &&
                                  !clipper->part()->modifierEnabled() && !settings->part()->modifierEnabled(),
                              "Promoted modifiers must stay inactive rather than silently target the grandparent.");
    scene.undo.undo();
    passed &=
        ORNL::Testing::expect(linked(parent, grandparent) && linked(clipper, parent) && linked(settings, parent) &&
                                  clipper->part()->modifierEnabled() && settings->part()->modifierEnabled(),
                              "Undoing nested deletion must restore the original active ownership.");
    return passed;
}

bool duplicateHierarchies() {
    Scene scene;
    auto root     = scene.add("assembly");
    auto settings = scene.add("settings", ORNL::MeshType::kSettings);
    auto clipper  = scene.add("clipper", ORNL::MeshType::kClipping);
    root->adoptChild(settings);
    settings->adoptChild(clipper);
    settings->part()->setModifierEnabled(false);
    const auto layer_height = ORNL::PS::Layer::kLayerHeight;
    auto grouped_settings   = QSharedPointer<ORNL::SettingsBase>::create();
    grouped_settings->setSetting(layer_height, 111.0);
    root->part()->createSettingsRange(1, 2, grouped_settings, "shared");
    root->part()->createSettingsRange(4, 5, grouped_settings, "shared");
    settings->part()->createSettingsRange(2, 3);
    settings->part()->getSettingsRange(2, 3)->getSb()->setSetting(layer_height, 222.0);
    scene.model->setInstanceCount(root, 3);
    bool passed = ORNL::Testing::expect(
        scene.model->items().size() == 9 && ORNL::CSM->count() == 9 && scene.model->instanceCount(root) == 3,
        "Increasing instance count must duplicate complete nested hierarchies.");
    for (const QString& suffix : {QString("_copy"), QString("_copy_1")}) {
        auto root_copy      = scene.find("assembly" + suffix);
        auto settings_copy  = scene.find("settings" + suffix);
        auto clipper_copy   = scene.find("clipper" + suffix);
        const bool complete = root_copy && settings_copy && clipper_copy;
        passed &= ORNL::Testing::expect(complete, "Instances must include every descendant.");
        if (!complete) return false;
        passed &= ORNL::Testing::expect(
            linked(settings_copy, root_copy) && linked(clipper_copy, settings_copy) &&
                settings_copy->meshType() == ORNL::MeshType::kSettings &&
                clipper_copy->meshType() == ORNL::MeshType::kClipping && !settings_copy->part()->modifierEnabled() &&
                root_copy->part()->rootMesh()->transformation() == root->part()->rootMesh()->transformation() &&
                settings_copy->part()->rootMesh()->transformation() == settings->part()->rootMesh()->transformation() &&
                clipper_copy->part()->rootMesh()->transformation() == clipper->part()->rootMesh()->transformation(),
            "Instances must preserve complete ownership, roles, activation, and world poses.");
        auto root_range          = root_copy->part()->getSettingsRange(1, 2);
        auto grouped_range       = root_copy->part()->getSettingsRange(4, 5);
        auto child_range         = settings_copy->part()->getSettingsRange(2, 3);
        const bool copied_ranges = root_range && grouped_range && child_range;
        passed &=
            ORNL::Testing::expect(copied_ranges, "Every copied hierarchy member must retain its layer overrides.");
        if (!copied_ranges) return false;
        passed &= ORNL::Testing::expect(
            root_range->getSb() != grouped_settings && root_range->getSb() == grouped_range->getSb() &&
                root_range->getSb()->setting<double>(layer_height) == 111.0 &&
                child_range->getSb() != settings->part()->getSettingsRange(2, 3)->getSb() &&
                child_range->getSb()->setting<double>(layer_height) == 222.0 &&
                root_copy->part()->getRangesFromTemplate() == root->part()->getRangesFromTemplate() &&
                settings_copy->part()->getRangesFromTemplate() == settings->part()->getRangesFromTemplate() &&
                root_copy->part()->getTemplateApplied() == root->part()->getTemplateApplied(),
            "Copied overrides must retain grouping and template state without sharing source settings.");
        root_range->getSb()->setSetting(layer_height, 333.0);
        child_range->getSb()->setSetting(layer_height, 444.0);
        passed &= ORNL::Testing::expect(
            grouped_settings->setting<double>(layer_height) == 111.0 &&
                settings->part()->getSettingsRange(2, 3)->getSb()->setting<double>(layer_height) == 222.0 &&
                grouped_range->getSb()->setting<double>(layer_height) == 333.0,
            "Editing copied layer overrides must update its group without changing the source.");
    }
    scene.model->setInstanceCount(root, 1);
    passed &= ORNL::Testing::expect(scene.model->items().size() == 3 && ORNL::CSM->count() == 3 &&
                                        !scene.find("settings_copy") && !scene.find("clipper_copy_1"),
                                    "Reducing instance count must remove the discarded instances' entire subtrees.");
    scene.undo.undo();
    scene.undo.undo();
    passed &= ORNL::Testing::expect(scene.model->items().size() == 9 && ORNL::CSM->count() == 9,
                                    "Undo must restore removed instance subtrees.");
    scene.undo.redo();
    scene.undo.redo();
    settings->setSelected(true);
    scene.model->setSelectionCopied();
    scene.model->copySelection();
    auto settings_copy = scene.find("settings_copy");
    auto clipper_copy  = scene.find("clipper_copy");
    passed &=
        ORNL::Testing::expect(settings_copy && clipper_copy && linked(settings_copy, root) &&
                                  linked(clipper_copy, settings_copy) && !settings_copy->part()->modifierEnabled(),
                              "Copying a child alone must keep the same parent, descendants, and activation state.");
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    bool passed = reparentUndo();
    passed &= deletionUndo();
    passed &= keepChildrenScope();
    passed &= duplicateHierarchies();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
