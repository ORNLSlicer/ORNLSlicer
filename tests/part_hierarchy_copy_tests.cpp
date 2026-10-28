#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdlib>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/mathutils.h"
#include "widgets/part_widget/model/part_meta_model.h"

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

QSharedPointer<ORNL::PartMetaItem> findItem(QSharedPointer<ORNL::PartMetaModel> model, const QString& name) {
    for (auto item : model->items()) {
        if (item->part()->name() == name) return item;
    }
    return nullptr;
}

bool sameTransform(const QMatrix4x4& actual, const QMatrix4x4& expected) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            const double tolerance = column == 3 ? 0.1 : 1.0e-4;
            if (!ORNL::Testing::near(actual(row, column), expected(row, column), tolerance)) return false;
        }
    }
    return true;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create temporary directory.")) return EXIT_FAILURE;
    qputenv("XDG_DATA_HOME", temp_dir.path().toUtf8());

    auto model      = QSharedPointer<ORNL::PartMetaModel>::create();
    auto root       = addPart(model, "assembly");
    auto child      = addPart(model, "setting", ORNL::MeshType::kSettings);
    auto grandchild = addPart(model, "clipper", ORNL::MeshType::kClipping);
    root->adoptChild(child);
    child->adoptChild(grandchild);
    if (!ORNL::Testing::expect(child->part()->parent() == root->part() && grandchild->part()->parent() == child->part(),
                               "Object-list parenting must update the hierarchy used for project saving."))
        return EXIT_FAILURE;

    const QQuaternion rotation = QQuaternion::fromEulerAngles(0, 0, 35);
    root->setTransformation(ORNL::MathUtils::composeTransformMatrix(QVector3D(2, 3, 4), rotation, QVector3D(2, 2, 2)));
    child->setTransformation(QMatrix4x4());
    QMatrix4x4 grandchild_transform =
        ORNL::MathUtils::composeTransformMatrix(QVector3D(4, 1, 6), rotation, QVector3D(1, 2, 3));
    QMatrix4x4 child_rotation;
    child_rotation.rotate(QQuaternion::fromEulerAngles(0, 0, 45));
    grandchild_transform *= child_rotation;
    grandchild->setTransformation(grandchild_transform);
    root->setSelected(true);
    model->setSelectionCopied();
    model->copySelection();

    auto copied_root       = findItem(model, "assembly_copy");
    auto copied_child      = findItem(model, "setting_copy");
    auto copied_grandchild = findItem(model, "clipper_copy");
    bool passed            = ORNL::Testing::expect(copied_root && copied_child && copied_grandchild,
                                                   "Pasting a parent should copy every descendant.");
    if (!passed) return EXIT_FAILURE;

    passed &=
        ORNL::Testing::expect(copied_child->parent() == copied_root && copied_grandchild->parent() == copied_child,
                              "Pasting should preserve the nested parent/child relationships.");
    passed &= ORNL::Testing::expect(
        copied_root->part()->children().size() == 1 && copied_child->part()->children().size() == 1,
        "Reconstructing copied metadata must not duplicate core hierarchy links.");
    const QMatrix4x4 root_expected =
        ORNL::MathUtils::composeTransformMatrix(QVector3D(200000, 300000, 400000), rotation, QVector3D(2, 2, 2));
    QMatrix4x4 grandchild_expected =
        ORNL::MathUtils::composeTransformMatrix(QVector3D(400000, 100000, 600000), rotation, QVector3D(1, 2, 3));
    grandchild_expected *= child_rotation;
    passed &= ORNL::Testing::expect(sameTransform(copied_root->part()->rootMesh()->transformation(), root_expected),
                                    "Copied parent should preserve its live position, rotation, and scale.");
    passed &= ORNL::Testing::expect(copied_child->part()->rootMesh()->transformation().isIdentity(),
                                    "An identity-transformed child should keep its original placement.");
    passed &= ORNL::Testing::expect(
        sameTransform(copied_grandchild->part()->rootMesh()->transformation(), grandchild_expected),
        "Copied descendants should preserve their live transforms, including shear, in object units.");
    passed &= ORNL::Testing::expect(copied_child->part()->getMeshType() == ORNL::MeshType::kSettings &&
                                        copied_grandchild->part()->getMeshType() == ORNL::MeshType::kClipping,
                                    "Copied hierarchy members should keep their mesh roles.");
    passed &= ORNL::Testing::expect(ORNL::CSM->getPart("assembly_copy") == copied_root->part() &&
                                        ORNL::CSM->getPart("setting_copy") == copied_child->part() &&
                                        ORNL::CSM->getPart("clipper_copy") == copied_grandchild->part(),
                                    "Pasted objects should be registered before saving or slicing.");

    model->copySelection();
    passed &= ORNL::Testing::expect(findItem(model, "assembly_copy_1") && findItem(model, "setting_copy_1") &&
                                        findItem(model, "clipper_copy_1") && model->items().size() == 9,
                                    "Repeated pastes should create uniquely named copies of the complete hierarchy.");

    root->setSelected(false);
    auto standalone = addPart(model, "standalone");
    standalone->setTransformation(
        ORNL::MathUtils::composeTransformMatrix(QVector3D(5, 6, 7), QQuaternion(), QVector3D(1, 1, 1)));
    standalone->setSelected(true);
    model->setSelectionCopied();
    model->copySelection();
    auto copied_standalone = findItem(model, "standalone_copy");
    passed &=
        ORNL::Testing::expect(copied_standalone && copied_standalone->part()->rootMesh()->transformation().isIdentity(),
                              "Standalone copies should retain the existing automatic placement behavior.");

    model->clearItems();
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
