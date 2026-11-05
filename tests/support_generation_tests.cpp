#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "step/layer/island/island_base.h"
#include "step/layer/layer.h"
#include "test_utils.h"
#include "threading/traditional_ast.h"
#define private public
#include "threading/slicers/planar_slicer.h"
#undef private
#include "units/unit.h"
#include "utilities/constants.h"

namespace {
using namespace ORNL;

PolygonList rectangle(double right) {
    Polygon polygon;
    polygon << Point(0, 0) << Point(right * mm(), 0) << Point(right * mm(), 100 * mm()) << Point(0, 100 * mm());
    PolygonList geometry;
    geometry += polygon;
    return geometry;
}

QSharedPointer<SettingsBase> supportSettings(Distance clearance, Distance bead_width) {
    auto settings = QSharedPointer<SettingsBase>::create();
    settings->setSetting(PS::Support::kEnable, true);
    settings->setSetting(PS::Support::kThresholdAngle, 40 * deg);
    settings->setSetting(PS::Support::kXYDistance, clearance);
    settings->setSetting(PS::Support::kInterfaceLayers, 3);
    settings->setSetting(PS::Support::kValidation, true);
    settings->setSetting(PS::Support::kValidationMinOverlap, 5.0);
    settings->setSetting(PS::Layer::kLayerHeight, 4 * mm);
    settings->setSetting(PS::Layer::kBeadWidth, bead_width);
    return settings;
}

QSharedPointer<Part> makePart(const QVector<double>& edges, const QSharedPointer<SettingsBase>& settings) {
    auto part = QSharedPointer<Part>::create();
    for (int i = 0; i < edges.size(); ++i) {
        auto layer = QSharedPointer<Layer>::create(i + 1, settings);
        layer->setGeometry(rectangle(edges[i]), QVector3D(0, 0, 1));
        const Point center(0, 0, (i + 0.5) * 4 * mm());
        layer->setOrientation(Plane(center, QVector3D(0, 0, 1)), center);
        part->appendStep(layer);
    }
    return part;
}

PolygonList supports(const QSharedPointer<Part>& part, int index) {
    PolygonList geometry;
    for (const auto& island : part->step(index, StepType::kLayer)->getIslands(IslandType::kSupport))
        geometry |= island->getGeometry();
    return geometry;
}

bool projectsBelowClearance(PlanarSlicer& slicer) {
    const auto settings = supportSettings(25.4 * mm, 1 * mm);
    const auto part     = makePart({20, 26, 32, 38, 44, 50, 56, 62}, settings);
    slicer.processSupport(part, 8, 0);
    bool passed = Testing::expect(supports(part, 0).inside(Point(54 * mm(), 50 * mm()), true),
                                  "Overhang demand must survive clearance clipping to reach lower layers.");
    for (int i = 0; i < 8; ++i) {
        PolygonList geometry        = supports(part, i);
        const PolygonList forbidden = part->step(i, StepType::kLayer)->getGeometry().offset(25.4 * mm);
        passed &= Testing::expect((geometry & forbidden).isEmpty(), "Support must preserve the XY clearance.");
    }
    return passed;
}

bool slopedInterfaceHasPaths(PlanarSlicer& slicer) {
    const auto part = makePart({20, 26, 32, 38, 44, 50, 56}, supportSettings(0 * mm, 4 * mm));
    slicer.processSupport(part, 7, 0);
    PolygonList interface_geometry;
    bool has_paths = false;
    Point current;
    QVector<QSharedPointer<RegionBase>> previous;
    for (const auto& island : part->step(3, StepType::kLayer)->getIslands(IslandType::kSupport)) {
        if (!island->getSb()->setting<bool>(PS::Support::kInterfaceRegion)) continue;
        interface_geometry |= island->getGeometry();
        island->compute(4);
        island->optimize(4, current, previous);
        for (const auto& region : island->getRegions()) has_paths |= !region->getPaths().isEmpty();
    }
    bool passed = Testing::expect(interface_geometry.splitIntoParts(true).size() == 1,
                                  "Adjacent sloped contacts must form a continuous dense interface.");
    passed &= Testing::expect(has_paths, "Sloped support interfaces must produce printable paths.");
    return passed;
}

bool respectsAngleAndLandings(PlanarSlicer& slicer) {
    const auto settings = supportSettings(0 * mm, 1 * mm);
    auto part           = makePart({20, 22, 24, 26}, settings);
    slicer.processSupport(part, 4, 0);
    bool passed = true;
    for (int i = 0; i < 4; ++i)
        passed &= Testing::expect(supports(part, i).isEmpty(), "Self-supporting slopes must not generate supports.");

    part = makePart({100, 20, 20, 20, 80}, settings);
    slicer.processSupport(part, 5, 0);
    passed &= Testing::expect(supports(part, 0).isEmpty(), "Support projection must stop on the model.");
    passed &= Testing::expect(!supports(part, 1).isEmpty(), "Everywhere support may land on the model.");

    settings->setSetting(PS::Support::kPlacement, 1);
    slicer.processSupport(part, 5, 0);
    for (int i = 0; i < 5; ++i)
        passed &= Testing::expect(supports(part, i).isEmpty(), "Build Plate Only must reject model-founded support.");
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!ORNL::Testing::expect(directory.isValid(), "Failed to create support test directory.")) return 1;
    ORNL::PlanarSlicer slicer(directory.filePath("support.nc"));
    bool passed = projectsBelowClearance(slicer);
    passed &= slopedInterfaceHasPaths(slicer);
    passed &= respectsAngleAndLandings(slicer);
    if (!passed) return 1;
    std::cout << "Support generation tests passed\n";
    return 0;
}
