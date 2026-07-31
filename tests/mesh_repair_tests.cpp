#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <CGAL/Modifier_base.h>
#include <CGAL/Polyhedron_incremental_builder_3.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/closed_mesh.h"

namespace {
struct Triangle {
    int a;
    int b;
    int c;
};

template <class HDS> class PolyhedronBuilder : public CGAL::Modifier_base<HDS> {
  public:
    PolyhedronBuilder(const std::vector<ORNL::MeshTypes::Point_3>& points, const std::vector<Triangle>& triangles)
        : m_points(points), m_triangles(triangles) {}

    void operator()(HDS& hds) {
        CGAL::Polyhedron_incremental_builder_3<HDS> builder(hds, true);
        builder.begin_surface(m_points.size(), m_triangles.size());

        for (const ORNL::MeshTypes::Point_3& point : m_points)
            builder.add_vertex(point);

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

    bool wasError() const { return m_error; }

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

ORNL::MeshTypes::Polyhedron buildLargeBoundaryOpenStrip() {
    constexpr int strip_segments = 18;

    std::vector<ORNL::MeshTypes::Point_3> points;
    points.reserve((strip_segments + 1) * 2);
    for (int x = 0; x <= strip_segments; ++x) {
        points.emplace_back(x, 0, 0);
        points.emplace_back(x, 1, 0);
    }

    std::vector<Triangle> triangles;
    triangles.reserve(strip_segments * 2);
    for (int x = 0; x < strip_segments; ++x) {
        const int lower_left = x * 2;
        const int upper_left = lower_left + 1;
        const int lower_right = lower_left + 2;
        const int upper_right = lower_left + 3;

        triangles.push_back({lower_left, lower_right, upper_right});
        triangles.push_back({lower_left, upper_right, upper_left});
    }

    return buildPolyhedron(points, triangles);
}

ORNL::MeshTypes::Polyhedron buildClosedTetrahedron() {
    const std::vector<ORNL::MeshTypes::Point_3> points = {
        ORNL::MeshTypes::Point_3(0, 0, 0), ORNL::MeshTypes::Point_3(1, 0, 0), ORNL::MeshTypes::Point_3(0, 1, 0),
        ORNL::MeshTypes::Point_3(0, 0, 1)};

    const std::vector<Triangle> triangles = {{0, 2, 1}, {0, 1, 3}, {1, 2, 3}, {2, 0, 3}};

    return buildPolyhedron(points, triangles);
}

bool expect(bool condition, const std::string& message) {
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}
} // namespace

int main() {
    bool passed = true;

    ORNL::MeshTypes::Polyhedron open_strip = buildLargeBoundaryOpenStrip();
    ORNL::ClosedMesh::RepairResult open_result = ORNL::ClosedMesh::CleanPolyhedronWithStatus(open_strip);
    passed &= expect(open_result == ORNL::ClosedMesh::RepairResult::kSkippedLargeBoundary,
                     "Expected large-boundary open strip repair to be skipped.");
    passed &= expect(!open_strip.is_closed(), "Expected skipped open strip to remain open.");

    ORNL::MeshTypes::Polyhedron tetrahedron = buildClosedTetrahedron();
    passed &= expect(tetrahedron.is_closed(), "Expected tetrahedron test fixture to start closed.");
    ORNL::ClosedMesh::RepairResult closed_result = ORNL::ClosedMesh::CleanPolyhedronWithStatus(tetrahedron);
    passed &= expect(closed_result == ORNL::ClosedMesh::RepairResult::kSuccess,
                     "Expected closed tetrahedron repair to succeed.");
    passed &= expect(tetrahedron.is_closed(), "Expected closed tetrahedron to remain closed after repair.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
