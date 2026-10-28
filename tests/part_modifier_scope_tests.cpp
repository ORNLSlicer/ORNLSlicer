#include <QCoreApplication>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cmath>
#include <cstdlib>
#include <string>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "step/layer/layer.h"
#include "test_utils.h"
#include "threading/slicers/planar_slicer.h"
#include "utilities/constants.h"

namespace {
class TestPlanarSlicer : public ORNL::PlanarSlicer {
   public:
    using PlanarSlicer::PlanarSlicer;
    using PlanarSlicer::preProcess;
};

QSharedPointer<ORNL::ClosedMesh> box(float length = 10000, float width = 10000, float height = 10000,
                                     QVector3D translation = {}) {
    auto mesh = QSharedPointer<ORNL::ClosedMesh>::create(ORNL::MeshFactory::CreateBoxMesh(length, width, height));
    QMatrix4x4 transform;
    transform.translate(translation);
    mesh->setTransformation(transform);
    return mesh;
}

QSharedPointer<ORNL::Part> addPart(const QString& name, ORNL::MeshType type,
                                   QSharedPointer<ORNL::ClosedMesh> mesh = box()) {
    mesh->setName(name);
    mesh->setType(type);
    auto part = QSharedPointer<ORNL::Part>::create(mesh, QString(), type);
    ORNL::CSM->addPart(part, false);
    return part;
}

void clearScene() {
    for (const auto& part : ORNL::CSM->parts()) {
        if (auto parent = part->parent(); !parent.isNull()) parent->orphanChild(part);
    }
    ORNL::CSM->clearParts();
}

void sliceScene(const QString& output) {
    for (const auto& part : ORNL::CSM->parts()) { part->clearSteps(); }
    TestPlanarSlicer slicer(output);
    slicer.preProcess();
}

bool checkArea(QSharedPointer<ORNL::Part> part, double expected, const std::string& stage) {
    const auto layers = part->steps(ORNL::StepType::kLayer);
    if (!ORNL::Testing::expect(!layers.isEmpty(), stage + ": expected sliced layers for " + part->name().toStdString()))
        return false;
    auto geometry       = layers.first()->getGeometry();
    const double actual = std::fabs(geometry.netArea()()) / 1000000.0;
    return ORNL::Testing::expect(ORNL::Testing::near(actual, expected, 0.001),
                                 stage + ": " + part->name().toStdString() + " area was " + std::to_string(actual) +
                                     " mm^2; expected " + std::to_string(expected));
}

QSet<int> settingsMarkers(QSharedPointer<ORNL::Part> part) {
    QSet<int> markers;
    const auto layers = part->steps(ORNL::StepType::kLayer);
    if (layers.isEmpty()) return markers;
    for (const auto& region : layers.first().dynamicCast<ORNL::Layer>()->getSettingsPolygons()) {
        if (!region.isEmpty()) markers.insert(region.getSettings()->setting<int>(ORNL::PS::Infill::kLineSpacing));
    }
    return markers;
}

bool clippingScope(const QString& output) {
    auto a = addPart("A", ORNL::MeshType::kBuild);
    auto b = addPart("B", ORNL::MeshType::kBuild);
    // Extend beyond the build meshes in Y/Z to avoid coplanar Boolean faces.
    auto clipper =
        addPart("clipper", ORNL::MeshType::kClipping, box(6000, 32000, 12000, QVector3D(-1000, -1000, -1000)));
    a->adoptChild(clipper);

    sliceScene(output);
    bool passed = checkArea(a, 50, "parent-local clipper");
    passed &= checkArea(b, 100, "parent-local clipper");

    a->appendSubMesh(box(16000, 10000, 10000, QVector3D(0, 20000, 0)));
    sliceScene(output);
    // Planar currently retains the last submesh's layer geometry.
    passed &= checkArea(a, 110, "parent-local clipper on submesh");
    passed &= checkArea(b, 100, "parent-local clipper on submesh");
    a->clearSubMeshes();

    a->orphanChild(clipper);
    b->adoptChild(clipper);
    sliceScene(output);
    passed &= checkArea(a, 100, "reparented clipper");
    passed &= checkArea(b, 50, "reparented clipper");

    b->orphanChild(clipper);
    sliceScene(output);
    passed &= checkArea(a, 50, "unparented global clipper");
    passed &= checkArea(b, 50, "unparented global clipper");

    a->adoptChild(clipper);
    a->adoptChild(b);
    sliceScene(output);
    passed &= checkArea(a, 50, "direct parent only");
    passed &= checkArea(b, 100, "direct parent only");
    clearScene();
    return passed;
}

bool settingsScope(const QString& output) {
    auto a       = addPart("A", ORNL::MeshType::kBuild);
    auto b       = addPart("B", ORNL::MeshType::kBuild);
    auto local_a = addPart("settings-A", ORNL::MeshType::kSettings, box(4000));
    auto local_b = addPart("settings-B", ORNL::MeshType::kSettings, box(4000));
    auto global = addPart("settings-global", ORNL::MeshType::kSettings, box(3000, 10000, 10000, QVector3D(6000, 0, 0)));
    local_a->getSb()->setSetting(ORNL::PS::Infill::kLineSpacing, 1000);
    local_b->getSb()->setSetting(ORNL::PS::Infill::kLineSpacing, 2000);
    global->getSb()->setSetting(ORNL::PS::Infill::kLineSpacing, 3000);
    a->adoptChild(local_a);
    b->adoptChild(local_b);

    // These local regions overlap in space, but affect different build objects.
    sliceScene(output);
    bool passed = ORNL::Testing::expect(
        settingsMarkers(a) == QSet<int>({1000, 3000}) && settingsMarkers(b) == QSet<int>({2000, 3000}),
        "Overlapping settings for separate parents must produce only their own and global polygons.");

    b->orphanChild(local_b);
    ORNL::CSM->removePart(local_b);
    a->orphanChild(local_a);
    b->adoptChild(local_a);
    sliceScene(output);
    passed &=
        ORNL::Testing::expect(settingsMarkers(a) == QSet<int>({3000}) && settingsMarkers(b) == QSet<int>({1000, 3000}),
                              "Reparenting a settings mesh must move its overrides to the new parent.");

    b->orphanChild(local_a);
    sliceScene(output);
    passed &= ORNL::Testing::expect(
        settingsMarkers(a) == QSet<int>({1000, 3000}) && settingsMarkers(b) == QSet<int>({1000, 3000}),
        "Detaching a settings mesh must restore global application.");

    a->adoptChild(local_a);
    a->adoptChild(b);
    sliceScene(output);
    passed &=
        ORNL::Testing::expect(settingsMarkers(a) == QSet<int>({1000, 3000}) && settingsMarkers(b) == QSet<int>({3000}),
                              "A build child must not inherit its ancestor's parent-local settings mesh.");

    // Global/local regions still conflict when both apply to the same target.
    global->setTransformation(QMatrix4x4());
    sliceScene(output);
    passed &=
        ORNL::Testing::expect(a->steps(ORNL::StepType::kLayer).isEmpty() && b->steps(ORNL::StepType::kLayer).isEmpty(),
                              "Overlapping settings applicable to one parent must retain existing rejection.");
    clearScene();
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temporary;
    if (!ORNL::Testing::expect(temporary.isValid(), "Could not create isolated modifier test storage."))
        return EXIT_FAILURE;
    Q_INIT_RESOURCE(configs);

    auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PRS::MachineSetup::kSyntax, static_cast<int>(ORNL::GcodeSyntax::kMarlin));
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 2000.0);
    settings->setSetting(ORNL::PS::Layer::kEnableVariableLayerHeight, false);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalX, 0.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalY, 0.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalZ, 1.0);
    settings->setSetting(ORNL::PS::SpecialModes::kEnableOversize, false);
    settings->setSetting(ORNL::PS::Support::kEnable, false);
    settings->setSetting(ORNL::PS::Skin::kEnable, false);
    settings->setSetting(ORNL::PS::LaserScanner::kLaserScanner, false);
    settings->setSetting(ORNL::PS::ThermalScanner::kThermalScanner, false);
    settings->setSetting(ORNL::MS::PlatformAdhesion::kRaftEnable, false);
    settings->setSetting(ORNL::MS::PlatformAdhesion::kBrimEnable, false);
    settings->setSetting(ORNL::MS::PlatformAdhesion::kSkirtEnable, false);

    const QString output = temporary.filePath("scope.gcode");
    bool passed          = clippingScope(output);
    passed &= settingsScope(output);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
