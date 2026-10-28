#include <QCoreApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cstdlib>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "threading/slicers/cylindrical_slicer.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
class TestCylindricalSlicer : public ORNL::CylindricalSlicer {
   public:
    TestCylindricalSlicer(const QString& output) : CylindricalSlicer(output) {}

    using CylindricalSlicer::preProcess;
};

QSharedPointer<ORNL::Part> addBox(const QString& name, ORNL::MeshType type, double width, double height) {
    auto mesh = QSharedPointer<ORNL::ClosedMesh>::create(
        ORNL::MeshFactory::CreateBoxMesh(width * ORNL::mm, width * ORNL::mm, height * ORNL::mm));
    mesh->setName(name);
    mesh->setType(type);
    auto part = QSharedPointer<ORNL::Part>::create(mesh, QString(), type);
    ORNL::CSM->addPart(part, false);
    return part;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create temporary cylindrical test storage."))
        return EXIT_FAILURE;
    Q_INIT_RESOURCE(configs);

    auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PRS::MachineSetup::kSyntax, static_cast<int>(ORNL::GcodeSyntax::kArcSpecialties));
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kCylindrical));
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kRadial));
    settings->setSetting(ORNL::PS::Slicing::kCylinderAxisSource,
                         static_cast<int>(ORNL::CylinderAxisSource::kPartCentroid));
    settings->setSetting(ORNL::PS::Slicing::kCylinderInnerRadius, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Slicing::kCylinderHeight, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 10.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Layer::kBeadWidth, 2.0 * ORNL::mm);

    auto parent       = addBox("parent", ORNL::MeshType::kBuild, 20, 2);
    auto other        = addBox("other", ORNL::MeshType::kBuild, 20, 2);
    auto nested_build = addBox("nested", ORNL::MeshType::kBuild, 20, 2);
    auto clipper      = addBox("clipper", ORNL::MeshType::kClipping, 22, 4);
    QMatrix4x4 transform;
    transform.translate(-ORNL::mm(), -ORNL::mm(), -ORNL::mm());
    clipper->setTransformation(transform);
    parent->adoptChild(nested_build);
    parent->adoptChild(clipper);

    bool passed = true;
    TestCylindricalSlicer slicer(temp_dir.filePath("scope.gcode"));
    slicer.preProcess();
    passed &= ORNL::Testing::expect(
        parent->countStepPairs() == 0 && other->countStepPairs() > 0 && nested_build->countStepPairs() > 0,
        "A cylindrical child clipper must affect only its direct parent.");

    parent->orphanChild(clipper);
    other->adoptChild(clipper);
    slicer.preProcess();
    passed &= ORNL::Testing::expect(
        parent->countStepPairs() > 0 && other->countStepPairs() == 0 && nested_build->countStepPairs() > 0,
        "Reparenting must move the cylindrical clipper's scope to its new parent.");

    other->orphanChild(clipper);
    slicer.preProcess();
    passed &= ORNL::Testing::expect(
        parent->countStepPairs() == 0 && other->countStepPairs() == 0 && nested_build->countStepPairs() == 0,
        "Unparented cylindrical clippers must retain global behavior.");

    parent->orphanChild(nested_build);
    ORNL::CSM->clearParts();
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
