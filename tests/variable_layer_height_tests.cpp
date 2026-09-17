#include <QSharedPointer>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <CGAL/Modifier_base.h>
#include <CGAL/Polyhedron_incremental_builder_3.h>

#include "configs/settings_base.h"
#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/closed_mesh.h"
#include "slicing/buffered_slicer.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
struct Triangle {
    int a;
    int b;
    int c;
};

template <class HDS>
class PolyhedronBuilder : public CGAL::Modifier_base<HDS> {
   public:
    PolyhedronBuilder(const std::vector<ORNL::MeshTypes::Point_3>& points, const std::vector<Triangle>& triangles)
        : m_points(points), m_triangles(triangles) {}

    void operator()(HDS& hds) {
        CGAL::Polyhedron_incremental_builder_3<HDS> builder(hds, true);
        builder.begin_surface(m_points.size(), m_triangles.size());

        for (const ORNL::MeshTypes::Point_3& point : m_points) builder.add_vertex(point);

        for (const Triangle& triangle : m_triangles) {
            builder.begin_facet();
            builder.add_vertex_to_facet(triangle.a);
            builder.add_vertex_to_facet(triangle.b);
            builder.add_vertex_to_facet(triangle.c);
            builder.end_facet();

            if (builder.error()) {
                m_error = true;
                builder.rollback();
                return;
            }
        }

        if (builder.error()) {
            m_error = true;
            builder.rollback();
            return;
        }

        builder.end_surface();
    }

    bool wasError() const {
        return m_error;
    }

   private:
    const std::vector<ORNL::MeshTypes::Point_3>& m_points;
    const std::vector<Triangle>& m_triangles;
    bool m_error = false;
};

ORNL::MeshTypes::Polyhedron buildPolyhedron(const std::vector<ORNL::MeshTypes::Point_3>& points,
                                            const std::vector<Triangle>& triangles) {
    ORNL::MeshTypes::Polyhedron polyhedron;
    PolyhedronBuilder<ORNL::MeshTypes::HalfedgeDescriptor> builder(points, triangles);
    polyhedron.delegate(builder);

    if (builder.wasError()) {
        std::cerr << "Failed to build test polyhedron\n";
        std::exit(EXIT_FAILURE);
    }

    return polyhedron;
}

QSharedPointer<ORNL::ClosedMesh> makeFrustum(double bottom_width, double top_width, double height) {
    const double bottom_half = bottom_width / 2.0;
    const double top_half    = top_width / 2.0;

    const std::vector<ORNL::MeshTypes::Point_3> points = {
        ORNL::MeshTypes::Point_3(-bottom_half, -bottom_half, 0.0),
        ORNL::MeshTypes::Point_3(bottom_half, -bottom_half, 0.0),
        ORNL::MeshTypes::Point_3(bottom_half, bottom_half, 0.0),
        ORNL::MeshTypes::Point_3(-bottom_half, bottom_half, 0.0),
        ORNL::MeshTypes::Point_3(-top_half, -top_half, height),
        ORNL::MeshTypes::Point_3(top_half, -top_half, height),
        ORNL::MeshTypes::Point_3(top_half, top_half, height),
        ORNL::MeshTypes::Point_3(-top_half, top_half, height),
    };

    const std::vector<Triangle> triangles = {
        {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
        {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7},
    };

    return QSharedPointer<ORNL::ClosedMesh>::create(buildPolyhedron(points, triangles));
}

QSharedPointer<ORNL::SettingsBase> slicingSettings(bool enable_variable_layer_height,
                                                   ORNL::GcodeSyntax syntax = ORNL::GcodeSyntax::kJuggerBot) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::MachineSetup::kSyntax, static_cast<int>(syntax));
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalX, 0.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalY, 0.0);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalZ, 1.0);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 5.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Layer::kEnableVariableLayerHeight, enable_variable_layer_height);
    settings->setSetting(ORNL::PS::Layer::kMinLayerHeight, 1.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Layer::kVariableLayerHeightSurfaceError, 1.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::SpecialModes::kEnableOversize, false);
    settings->setSetting(ORNL::PS::Support::kEnable, false);

    return settings;
}

std::vector<double> layerHeightsFor(QSharedPointer<ORNL::ClosedMesh> mesh, bool enable_variable_layer_height,
                                    ORNL::GcodeSyntax syntax    = ORNL::GcodeSyntax::kJuggerBot,
                                    bool use_cgal_cross_section = true) {
    ORNL::BufferedSlicer slicer(mesh, slicingSettings(enable_variable_layer_height, syntax), {}, {}, 0, 0,
                                use_cgal_cross_section, false);

    std::vector<double> layer_heights;
    for (int i = 0; i < 32; ++i) {
        QSharedPointer<ORNL::BufferedSlicer::SliceMeta> slice = slicer.processNextSlice();
        if (slice.isNull()) { break; }

        layer_heights.push_back(slice->settings->setting<ORNL::Distance>(ORNL::PS::Layer::kLayerHeight).to(ORNL::mm));
    }

    return layer_heights;
}

bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

bool near(double actual, double expected) {
    return std::abs(actual - expected) <= 1.0e-5;
}

bool allNear(const std::vector<double>& values, double expected) {
    return std::all_of(values.begin(), values.end(), [expected](double value) { return near(value, expected); });
}

bool allInRange(const std::vector<double>& values, double min_value, double max_value) {
    return std::all_of(values.begin(), values.end(),
                       [min_value, max_value](double value) { return value >= min_value && value <= max_value; });
}

bool anyBetween(const std::vector<double>& values, double min_value, double max_value) {
    return std::any_of(values.begin(), values.end(),
                       [min_value, max_value](double value) { return value > min_value && value < max_value; });
}

double internal(ORNL::Distance distance) {
    return distance();
}
}  // namespace

int main() {
    bool passed = true;

    const std::vector<double> fixed_heights = layerHeightsFor(
        makeFrustum(internal(10.0 * ORNL::mm), internal(1.0 * ORNL::mm), internal(10.0 * ORNL::mm)), false);
    passed &= expect(fixed_heights.size() == 2, "Expected fixed layer height slicing to produce two layers.");
    passed &= expect(allNear(fixed_heights, 5.0), "Expected fixed layer height slicing to keep 5 mm layers.");

    const std::vector<double> stable_variable_heights = layerHeightsFor(
        makeFrustum(internal(10.0 * ORNL::mm), internal(10.0 * ORNL::mm), internal(20.0 * ORNL::mm)), true);
    passed &= expect(stable_variable_heights.size() == 4, "Expected stable geometry to produce four standard layers.");
    passed &= expect(allNear(stable_variable_heights, 5.0),
                     "Expected variable layer height to prefer standard layers for vertical-wall geometry.");

    QSharedPointer<ORNL::ClosedMesh> tapered_mesh =
        makeFrustum(internal(10.0 * ORNL::mm), internal(1.0 * ORNL::mm), internal(10.0 * ORNL::mm));
    const std::vector<double> tapered_variable_heights = layerHeightsFor(tapered_mesh, true);
    passed &= expect(tapered_variable_heights.size() > fixed_heights.size(),
                     "Expected sloped variable-height slicing to add refinement layers.");
    passed &= expect(allInRange(tapered_variable_heights, 1.0, 5.0),
                     "Expected cusp-limited layer heights to stay within configured bounds.");
    passed &= expect(anyBetween(tapered_variable_heights, 1.0, 5.0),
                     "Expected cusp-limited slicing to use intermediate adaptive layer heights.");

    const int tapered_count =
        ORNL::BufferedSlicer::computeSliceCount(tapered_mesh, slicingSettings(true, ORNL::GcodeSyntax::kJuggerBot));
    passed &= expect(tapered_count == static_cast<int>(tapered_variable_heights.size()),
                     "Expected geometry-free slice count to match buffered variable-height slicing.");

    const std::vector<double> non_cgal_variable_heights =
        layerHeightsFor(makeFrustum(internal(10.0 * ORNL::mm), internal(1.0 * ORNL::mm), internal(10.0 * ORNL::mm)),
                        true, ORNL::GcodeSyntax::kJuggerBot, false);
    passed &= expect(non_cgal_variable_heights.size() == tapered_variable_heights.size(),
                     "Expected non-CGAL cross-section path to preserve variable layer count.");
    passed &= expect(allInRange(non_cgal_variable_heights, 1.0, 5.0),
                     "Expected non-CGAL variable-height layers to stay within configured bounds.");

    const std::vector<double> non_jugger_variable_heights =
        layerHeightsFor(makeFrustum(internal(10.0 * ORNL::mm), internal(1.0 * ORNL::mm), internal(10.0 * ORNL::mm)),
                        true, ORNL::GcodeSyntax::kMarlin);
    passed &= expect(non_jugger_variable_heights.size() == fixed_heights.size(),
                     "Expected non-JuggerBot syntax to ignore variable layer height.");
    passed &= expect(allNear(non_jugger_variable_heights, 5.0),
                     "Expected non-JuggerBot variable-height layers to remain fixed.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
