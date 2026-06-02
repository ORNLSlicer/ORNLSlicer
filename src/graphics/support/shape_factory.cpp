#include "graphics/support/shape_factory.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector4D>

#include "geometry/segments/bezier.h"
#include "managers/settings/settings_manager.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kVectorEpsilon = 1.0e-6f;
constexpr float kVectorEpsilonSquared = kVectorEpsilon * kVectorEpsilon;

constexpr int kCylinderSegments = 50;
constexpr int kConeSlices = 50;
constexpr int kSphereMinSectorCount = 3;
constexpr int kSphereMinStackCount = 2;
constexpr int kTubeCrossSectionResolution = 20;
constexpr int kCurveSegments = 75;
constexpr int kBuildVolumeCylinderSegments = 100;
constexpr int kBuildVolumeVerticalLines = 6;

struct Rgba {
    float r;
    float g;
    float b;
    float a;

    explicit Rgba(const QColor& color)
        : r(static_cast<float>(color.redF())), g(static_cast<float>(color.greenF())),
          b(static_cast<float>(color.blueF())), a(static_cast<float>(color.alphaF())) {}
};

void reserveAdditional(std::vector<float>& values, std::size_t count) { values.reserve(values.size() + count); }

void reserveTriangleMesh(std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals,
                         std::size_t triangle_count) {
    const std::size_t vertex_count = triangle_count * 3;
    reserveAdditional(vertices, vertex_count * 3);
    reserveAdditional(normals, vertex_count * 3);
    reserveAdditional(colors, vertex_count * 4);
}

void reserveLineMesh(std::vector<float>& vertices, std::vector<float>& colors, std::size_t line_count) {
    const std::size_t vertex_count = line_count * 2;
    reserveAdditional(vertices, vertex_count * 3);
    reserveAdditional(colors, vertex_count * 4);
}

void appendVertex(std::vector<float>& vertices, const QVector3D& vertex) {
    vertices.push_back(vertex.x());
    vertices.push_back(vertex.y());
    vertices.push_back(vertex.z());
}

void appendVector(std::vector<float>& values, const QVector3D& vector) {
    values.push_back(vector.x());
    values.push_back(vector.y());
    values.push_back(vector.z());
}

void appendColor(std::vector<float>& colors, const Rgba& rgba) {
    colors.push_back(rgba.r);
    colors.push_back(rgba.g);
    colors.push_back(rgba.b);
    colors.push_back(rgba.a);
}

QVector3D faceNormal(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2) {
    QVector3D normal = QVector3D::crossProduct(v1 - v0, v2 - v0);
    if (normal.lengthSquared() > kVectorEpsilonSquared) {
        normal.normalize();
    }
    return normal;
}

void appendTriangleData(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2, const Rgba& rgba,
                        std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals) {
    const QVector3D normal = faceNormal(v0, v1, v2);

    appendVertex(vertices, v0);
    appendVertex(vertices, v1);
    appendVertex(vertices, v2);

    for (int i = 0; i < 3; ++i) {
        appendVector(normals, normal);
        appendColor(colors, rgba);
    }
}

void appendLine(const QVector3D& start, const QVector3D& end, const Rgba& rgba, std::vector<float>& vertices,
                std::vector<float>& colors) {
    appendVertex(vertices, start);
    appendVertex(vertices, end);
    appendColor(colors, rgba);
    appendColor(colors, rgba);
}

std::size_t countLoopValues(float value, float limit, float step) {
    if (!std::isfinite(value) || !std::isfinite(limit) || !std::isfinite(step) || step <= 0.0f) {
        return 0;
    }

    std::size_t count = 0;
    while (value < limit) {
        ++count;
        value += step;
    }
    return count;
}

const std::array<QVector2D, kTubeCrossSectionResolution>& circleTable() {
    static const std::array<QVector2D, kTubeCrossSectionResolution> table = [] {
        std::array<QVector2D, kTubeCrossSectionResolution> values;
        const float step = kTwoPi / static_cast<float>(kTubeCrossSectionResolution);

        for (int i = 0; i < kTubeCrossSectionResolution; ++i) {
            const float theta = static_cast<float>(i) * step;
            values[i] = QVector2D(std::cos(theta), std::sin(theta));
        }

        return values;
    }();

    return table;
}

float arcSweepAngle(const Point& start, const Point& center, const Point& end, bool counterclockwise) {
    if (MathUtils::orientation(start, center, end) == 0) {
        return (start == end) ? kTwoPi : kPi;
    }

    const float start_angle = std::atan2(center.x() - start.x(), center.y() - start.y());
    const float end_angle = std::atan2(center.x() - end.x(), center.y() - end.y());

    float angle = counterclockwise ? start_angle - end_angle : end_angle - start_angle;
    if (angle < 0.0f) {
        angle += kTwoPi;
    }

    return angle;
}

void createArcCylinderMesh(float cylinder_height, const Point& start, const Point& center, const Point& end,
                           const QMatrix4x4& transform, bool counterclockwise, const QColor& color,
                           std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals) {
    const float major_radius = std::hypot(start.x() - center.x(), start.y() - center.y());
    const float minor_radius = cylinder_height / 2.0f;
    if (major_radius <= kVectorEpsilon || minor_radius <= kVectorEpsilon) {
        return;
    }

    const float angle = arcSweepAngle(start, center, end, counterclockwise);
    const float phi_increment = angle / static_cast<float>(kCurveSegments);
    const float height_increment = (end.z() - start.z()) / static_cast<float>(kCurveSegments);
    const auto& circle = circleTable();
    const Rgba rgba(color);

    reserveTriangleMesh(vertices, colors, normals,
                        kTubeCrossSectionResolution * (2 + (2 * static_cast<std::size_t>(kCurveSegments))));

    std::array<std::array<QVector3D, kTubeCrossSectionResolution>, kCurveSegments + 1> tube_vertices;

    float phi = counterclockwise ? 0.0f : kTwoPi;
    float height = 0.0f;
    for (auto& ring_vertices : tube_vertices) {
        const float cos_phi = std::cos(phi);
        const float sin_phi = std::sin(phi);

        for (int i = 0; i < kTubeCrossSectionResolution; ++i) {
            const float local_radius = major_radius + (minor_radius * circle[i].x());
            ring_vertices[i] = transform * QVector3D(cos_phi * local_radius, sin_phi * local_radius,
                                                     (circle[i].y() * minor_radius) + height);
        }

        height += height_increment;
        phi += counterclockwise ? phi_increment : -phi_increment;
    }

    const QVector3D start_center = transform * QVector3D(major_radius, 0.0f, 0.0f);
    const float end_phi = counterclockwise ? angle : -angle;
    const QVector3D end_center =
        transform * QVector3D(major_radius * std::cos(end_phi), major_radius * std::sin(end_phi), end.z() - start.z());

    for (int i = 0; i < kTubeCrossSectionResolution; ++i) {
        const int next = (i + 1) % kTubeCrossSectionResolution;
        if (counterclockwise) {
            appendTriangleData(start_center, tube_vertices[0][i], tube_vertices[0][next], rgba, vertices, colors,
                               normals);
        }
        else {
            appendTriangleData(tube_vertices[0][next], tube_vertices[0][i], start_center, rgba, vertices, colors,
                               normals);
        }
    }

    for (int slice_index = 0; slice_index < kCurveSegments; ++slice_index) {
        const int next_slice = slice_index + 1;
        for (int vertex_index = 0; vertex_index < kTubeCrossSectionResolution; ++vertex_index) {
            const int next_vertex = (vertex_index + 1) % kTubeCrossSectionResolution;

            if (counterclockwise) {
                appendTriangleData(tube_vertices[slice_index][vertex_index], tube_vertices[next_slice][vertex_index],
                                   tube_vertices[slice_index][next_vertex], rgba, vertices, colors, normals);
                appendTriangleData(tube_vertices[next_slice][next_vertex], tube_vertices[slice_index][next_vertex],
                                   tube_vertices[next_slice][vertex_index], rgba, vertices, colors, normals);
            }
            else {
                appendTriangleData(tube_vertices[slice_index][next_vertex], tube_vertices[next_slice][vertex_index],
                                   tube_vertices[slice_index][vertex_index], rgba, vertices, colors, normals);
                appendTriangleData(tube_vertices[next_slice][vertex_index], tube_vertices[slice_index][next_vertex],
                                   tube_vertices[next_slice][next_vertex], rgba, vertices, colors, normals);
            }
        }
    }

    for (int i = 0; i < kTubeCrossSectionResolution; ++i) {
        const int next = (i + 1) % kTubeCrossSectionResolution;
        if (counterclockwise) {
            appendTriangleData(tube_vertices[kCurveSegments][next], tube_vertices[kCurveSegments][i], end_center, rgba,
                               vertices, colors, normals);
        }
        else {
            appendTriangleData(end_center, tube_vertices[kCurveSegments][i], tube_vertices[kCurveSegments][next], rgba,
                               vertices, colors, normals);
        }
    }
}
} // namespace

void ShapeFactory::createRectangle(float length, float width, float height, const QMatrix4x4& transform,
                                   const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                                   std::vector<float>& normals) {
    const float half_length = length / 2.0f;
    const float half_width = width / 2.0f;
    const float half_height = height / 2.0f;

    const std::array<QVector3D, 8> corner_vertices = {
        transform * QVector3D(-half_length, -half_width, half_height),  // back  left  top
        transform * QVector3D(half_length, -half_width, half_height),   // back  right top
        transform * QVector3D(half_length, half_width, half_height),    // front right top
        transform * QVector3D(-half_length, half_width, half_height),   // front left  top
        transform * QVector3D(-half_length, -half_width, -half_height), // back  left  bottom
        transform * QVector3D(half_length, -half_width, -half_height),  // back  right bottom
        transform * QVector3D(half_length, half_width, -half_height),   // front right bottom
        transform * QVector3D(-half_length, half_width, -half_height),  // front left  bottom
    };

    static constexpr std::array<std::array<int, 3>, 12> kTriangles = {
        std::array<int, 3> {7, 6, 4},
        {6, 5, 4},
        {0, 1, 3},
        {1, 2, 3},
        {0, 3, 4},
        {3, 7, 4},
        {3, 2, 7},
        {2, 6, 7},
        {2, 1, 6},
        {1, 5, 6},
        {1, 0, 5},
        {0, 4, 5},
    };

    const Rgba rgba(color);
    reserveTriangleMesh(vertices, colors, normals, kTriangles.size());

    for (const auto& triangle : kTriangles) {
        appendTriangleData(corner_vertices[triangle[0]], corner_vertices[triangle[1]], corner_vertices[triangle[2]],
                           rgba, vertices, colors, normals);
    }
}

void ShapeFactory::createArcCylinderCCW(float cylinder_height, const Point& start, const Point& center,
                                        const Point& end, const QMatrix4x4& transform, const QColor& color,
                                        std::vector<float>& vertices, std::vector<float>& colors,
                                        std::vector<float>& normals) {
    createArcCylinderMesh(cylinder_height, start, center, end, transform, true, color, vertices, colors, normals);
}

void ShapeFactory::createArcCylinder(float cylinder_height, const Point& start, const Point& center, const Point& end,
                                     const QMatrix4x4& transform, const QColor& color, std::vector<float>& vertices,
                                     std::vector<float>& colors, std::vector<float>& normals) {
    createArcCylinderMesh(cylinder_height, start, center, end, transform, false, color, vertices, colors, normals);
}

void ShapeFactory::createSplineCylinder(float diameter, const Point& start, const Point& control_a,
                                        const Point& control_b, const Point& end, const QColor& color,
                                        std::vector<float>& vertices, std::vector<float>& colors,
                                        std::vector<float>& normals) {
    const float radius = diameter / 2.0f;
    if (radius <= kVectorEpsilon) {
        return;
    }

    const auto& circle = circleTable();
    const Rgba rgba(color);
    reserveTriangleMesh(vertices, colors, normals,
                        kTubeCrossSectionResolution * (2 + (2 * static_cast<std::size_t>(kCurveSegments))));

    std::array<std::array<QVector3D, kTubeCrossSectionResolution>, kCurveSegments + 1> tube_vertices;

    BezierSegment curve(start, control_a, control_b, end);
    const double increment = 1.0 / static_cast<double>(kCurveSegments);
    double t = 0.0;

    for (auto& ring_vertices : tube_vertices) {
        const Point center_point = curve.getPointAlong(t);
        const Point next_center = curve.getPointAlong(t + increment);
        const Angle tangent_angle = MathUtils::signedInternalAngle(
            Point(center_point.x(), center_point.y() + 10.0f, center_point.z()), center_point, next_center);

        for (int i = 0; i < kTubeCrossSectionResolution; ++i) {
            Point point(center_point.x() + (radius * circle[i].x()), center_point.y(),
                        center_point.z() + (radius * circle[i].y()));
            point = point.rotateAround(center_point, tangent_angle);
            ring_vertices[i] = point.toQVector3D();
        }

        t += increment;
    }

    const QVector3D start_center = curve.getPointAlong(0.0).toQVector3D();
    const QVector3D end_center = curve.getPointAlong(1.0).toQVector3D();

    for (int i = 0; i < kTubeCrossSectionResolution; ++i) {
        appendTriangleData(start_center, tube_vertices[0][i], tube_vertices[0][(i + 1) % kTubeCrossSectionResolution],
                           rgba, vertices, colors, normals);
    }

    for (int slice_index = 0; slice_index < kCurveSegments; ++slice_index) {
        const int next_slice = slice_index + 1;
        for (int vertex_index = 0; vertex_index < kTubeCrossSectionResolution; ++vertex_index) {
            const int next_vertex = (vertex_index + 1) % kTubeCrossSectionResolution;

            appendTriangleData(tube_vertices[slice_index][vertex_index], tube_vertices[next_slice][vertex_index],
                               tube_vertices[slice_index][next_vertex], rgba, vertices, colors, normals);
            appendTriangleData(tube_vertices[next_slice][next_vertex], tube_vertices[slice_index][next_vertex],
                               tube_vertices[next_slice][vertex_index], rgba, vertices, colors, normals);
        }
    }

    for (int i = 0; i < kTubeCrossSectionResolution; ++i) {
        appendTriangleData(tube_vertices[kCurveSegments][(i + 1) % kTubeCrossSectionResolution],
                           tube_vertices[kCurveSegments][i], end_center, rgba, vertices, colors, normals);
    }
}

void ShapeFactory::createCylinder(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                                  std::vector<float>& vertices, std::vector<float>& colors,
                                  std::vector<float>& normals) {
    if (radius <= kVectorEpsilon || height <= kVectorEpsilon) {
        return;
    }

    std::array<QVector3D, kCylinderSegments> top_vertices;
    std::array<QVector3D, kCylinderSegments> bottom_vertices;
    const float theta_increment = kTwoPi / static_cast<float>(kCylinderSegments);

    for (int i = 0; i < kCylinderSegments; ++i) {
        const float theta = static_cast<float>(i) * theta_increment;
        const float x = radius * std::cos(theta);
        const float y = radius * std::sin(theta);
        top_vertices[i] = transform * QVector3D(x, y, height);
        bottom_vertices[i] = transform * QVector3D(x, y, 0.0f);
    }

    const QVector3D top_center = transform * QVector3D(0.0f, 0.0f, height);
    const QVector3D bottom_center = transform * QVector3D(0.0f, 0.0f, 0.0f);
    const Rgba rgba(color);
    reserveTriangleMesh(vertices, colors, normals, static_cast<std::size_t>(kCylinderSegments) * 4);

    for (int i = 0; i < kCylinderSegments; ++i) {
        const int next = (i + 1) % kCylinderSegments;
        appendTriangleData(top_center, top_vertices[i], top_vertices[next], rgba, vertices, colors, normals);
        appendTriangleData(top_vertices[next], top_vertices[i], bottom_vertices[i], rgba, vertices, colors, normals);
        appendTriangleData(top_vertices[next], bottom_vertices[i], bottom_vertices[next], rgba, vertices, colors,
                           normals);
        appendTriangleData(bottom_vertices[next], bottom_vertices[i], bottom_center, rgba, vertices, colors, normals);
    }
}

void ShapeFactory::createSphere(float radius, int sectorCount, int stackCount, const QMatrix4x4& transform,
                                const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                                std::vector<float>& normals) {
    if (radius <= kVectorEpsilon || sectorCount < kSphereMinSectorCount || stackCount < kSphereMinStackCount) {
        return;
    }

    const float sector_step = kTwoPi / static_cast<float>(sectorCount);
    const float stack_step = kPi / static_cast<float>(stackCount);

    std::vector<QVector3D> tmp_vertices;
    tmp_vertices.reserve(static_cast<std::size_t>(stackCount + 1) * static_cast<std::size_t>(sectorCount + 1));

    for (int i = 0; i <= stackCount; ++i) {
        const float stack_angle = (kPi / 2.0f) - (static_cast<float>(i) * stack_step);
        const float xy = radius * std::cos(stack_angle);
        const float z = radius * std::sin(stack_angle);

        for (int j = 0; j <= sectorCount; ++j) {
            const float sector_angle = static_cast<float>(j) * sector_step;
            tmp_vertices.push_back(transform * QVector3D(xy * std::cos(sector_angle), xy * std::sin(sector_angle), z));
        }
    }

    const std::size_t triangle_count =
        static_cast<std::size_t>(sectorCount) * static_cast<std::size_t>((2 * stackCount) - 2);
    const Rgba rgba(color);
    reserveTriangleMesh(vertices, colors, normals, triangle_count);

    for (int i = 0; i < stackCount; ++i) {
        int vi1 = i * (sectorCount + 1);
        int vi2 = (i + 1) * (sectorCount + 1);

        for (int j = 0; j < sectorCount; ++j, ++vi1, ++vi2) {
            const QVector3D& v1 = tmp_vertices[vi1];
            const QVector3D& v2 = tmp_vertices[vi2];
            const QVector3D& v3 = tmp_vertices[vi1 + 1];
            const QVector3D& v4 = tmp_vertices[vi2 + 1];

            if (i == 0) {
                appendTriangleData(v1, v2, v4, rgba, vertices, colors, normals);
            }
            else if (i == (stackCount - 1)) {
                appendTriangleData(v1, v2, v3, rgba, vertices, colors, normals);
            }
            else {
                appendTriangleData(v1, v2, v3, rgba, vertices, colors, normals);
                appendTriangleData(v3, v2, v4, rgba, vertices, colors, normals);
            }
        }
    }
}

void ShapeFactory::createGcodeCylinder(float width, float length, float height, const QVector3D& start,
                                       const QVector3D& end, const QColor& color, std::vector<float>& vertices,
                                       std::vector<float>& colors, std::vector<float>& normals) {
    if (width <= kVectorEpsilon || height <= kVectorEpsilon || length <= kVectorEpsilon ||
        (end - start).lengthSquared() <= kVectorEpsilonSquared) {
        return;
    }

    const QMatrix4x4 transform = computeGcodeCylinderTransform(start, end);

    const bool rectangular_prism = height > width;
    const float radius = rectangular_prism
                             ? std::sqrt(((width / 2.0f) * (width / 2.0f)) + ((height / 2.0f) * (height / 2.0f)))
                             : width / 2.0f;
    if (radius <= kVectorEpsilon) {
        return;
    }

    const unsigned int quads_per_side = rectangular_prism ? 1 : 6;
    const unsigned int vertices_per_arc = quads_per_side + 1;
    const unsigned int vertices_per_side = 2 * vertices_per_arc;
    const float theta_start = -std::asin(std::clamp((height / 2.0f) / radius, -1.0f, 1.0f));
    const float theta_end = -theta_start;
    const float theta_increment = (theta_end - theta_start) / static_cast<float>(quads_per_side);

    std::vector<QVector3D> top_vertices(vertices_per_side);
    std::vector<QVector3D> bottom_vertices(vertices_per_side);

    for (unsigned int i = 0; i < vertices_per_arc; ++i) {
        const float theta = theta_start + (static_cast<float>(i) * theta_increment);
        const float x = radius * std::cos(theta);
        const float y = radius * std::sin(theta);

        top_vertices[i] = transform * QVector3D(x, y, length);
        bottom_vertices[i] = transform * QVector3D(x, y, 0.0f);
        top_vertices[i + vertices_per_arc] = transform * QVector3D(-x, -y, length);
        bottom_vertices[i + vertices_per_arc] = transform * QVector3D(-x, -y, 0.0f);
    }

    const QVector3D top_center = transform * QVector3D(0.0f, 0.0f, length);
    const QVector3D bottom_center = transform * QVector3D(0.0f, 0.0f, 0.0f);
    const Rgba rgba(color);
    reserveTriangleMesh(vertices, colors, normals, static_cast<std::size_t>(vertices_per_side) * 4);

    for (unsigned int i = 0; i < vertices_per_side; ++i) {
        const unsigned int next = (i + 1) % vertices_per_side;
        appendTriangleData(top_center, top_vertices[next], top_vertices[i], rgba, vertices, colors, normals);
        appendTriangleData(top_vertices[i], bottom_vertices[next], bottom_vertices[i], rgba, vertices, colors, normals);
        appendTriangleData(top_vertices[i], top_vertices[next], bottom_vertices[next], rgba, vertices, colors, normals);
        appendTriangleData(bottom_center, bottom_vertices[i], bottom_vertices[next], rgba, vertices, colors, normals);
    }
}

void ShapeFactory::createCone(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                              std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals) {
    if (radius <= kVectorEpsilon || height <= kVectorEpsilon) {
        return;
    }

    std::array<QVector3D, kConeSlices> perimeter_vertices;
    const float theta_increment = kTwoPi / static_cast<float>(kConeSlices);

    for (int i = 0; i < kConeSlices; ++i) {
        const float theta = static_cast<float>(i) * theta_increment;
        perimeter_vertices[i] = transform * QVector3D(radius * std::sin(theta), radius * std::cos(theta), 0.0f);
    }

    const QVector3D tip = transform * QVector3D(0.0f, 0.0f, height);
    const QVector3D base_center = transform * QVector3D(0.0f, 0.0f, 0.0f);
    const Rgba rgba(color);
    reserveTriangleMesh(vertices, colors, normals, static_cast<std::size_t>(kConeSlices) * 2);

    for (int i = 0; i < kConeSlices; ++i) {
        const int next = (i + 1) % kConeSlices;
        appendTriangleData(tip, perimeter_vertices[next], perimeter_vertices[i], rgba, vertices, colors, normals);
        appendTriangleData(perimeter_vertices[i], perimeter_vertices[next], base_center, rgba, vertices, colors,
                           normals);
    }
}

void ShapeFactory::createGridPlane(float length, float width, float x_grid_dist, float y_grid_dist, const QColor& color,
                                   std::vector<float>& vertices, std::vector<float>& colors) {
    const float y_min = -width / 2.0f;
    const float y_max = width / 2.0f;
    const float x_min = -length / 2.0f;
    const float x_max = length / 2.0f;
    constexpr float z = 0.0f;
    const Rgba rgba(color);

    std::size_t line_count = 0;
    if (x_grid_dist > 0.0f && x_grid_dist < (x_max - x_min)) {
        line_count += countLoopValues(x_min, x_max + (x_grid_dist / 2.0f), x_grid_dist);
    }
    if (y_grid_dist > 0.0f && y_grid_dist < (y_max - y_min)) {
        line_count += countLoopValues(y_min, y_max + (y_grid_dist / 2.0f), y_grid_dist);
    }
    reserveLineMesh(vertices, colors, line_count);

    if (x_grid_dist > 0.0f && x_grid_dist < (x_max - x_min)) {
        for (float current_x = x_min; current_x < (x_max + (x_grid_dist / 2.0f)); current_x += x_grid_dist) {
            appendLine(QVector3D(current_x, y_min, z), QVector3D(current_x, y_max, z), rgba, vertices, colors);
        }
    }

    if (y_grid_dist > 0.0f && y_grid_dist < (y_max - y_min)) {
        for (float current_y = y_min; current_y < (y_max + (y_grid_dist / 2.0f)); current_y += y_grid_dist) {
            appendLine(QVector3D(x_min, current_y, z), QVector3D(x_max, current_y, z), rgba, vertices, colors);
        }
    }
}

void ShapeFactory::createBuildVolumeRectangle(const QVector3D& min, const QVector3D& max, float x_grid_dist,
                                              float x_grid_offset, float y_grid_dist, float y_grid_offset,
                                              const QColor& color, std::vector<float>& vertices,
                                              std::vector<float>& colors) {
    const float printer_x_min = min.x();
    const float printer_x_max = max.x();
    const float printer_y_min = min.y();
    const float printer_y_max = max.y();
    const float printer_z_min = min.z();
    const float printer_z_max = max.z();

    std::size_t line_count = 12;
    if (x_grid_dist > 0.0f && x_grid_dist < (printer_x_max - printer_x_min)) {
        line_count += countLoopValues(printer_x_min + x_grid_offset, printer_x_max, x_grid_dist);
    }
    if (y_grid_dist > 0.0f && y_grid_dist < (printer_y_max - printer_y_min)) {
        line_count += countLoopValues(printer_y_min + y_grid_offset, printer_y_max, y_grid_dist);
    }

    const Rgba rgba(color);
    reserveLineMesh(vertices, colors, line_count);

    appendLine(QVector3D(printer_x_min, printer_y_min, printer_z_max),
               QVector3D(printer_x_max, printer_y_min, printer_z_max), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_min, printer_y_min, printer_z_max),
               QVector3D(printer_x_min, printer_y_max, printer_z_max), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_min, printer_y_min, printer_z_max),
               QVector3D(printer_x_min, printer_y_min, printer_z_min), rgba, vertices, colors);

    appendLine(QVector3D(printer_x_max, printer_y_max, printer_z_max),
               QVector3D(printer_x_max, printer_y_min, printer_z_max), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_max, printer_y_max, printer_z_max),
               QVector3D(printer_x_min, printer_y_max, printer_z_max), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_max, printer_y_max, printer_z_max),
               QVector3D(printer_x_max, printer_y_max, printer_z_min), rgba, vertices, colors);

    appendLine(QVector3D(printer_x_max, printer_y_min, printer_z_min),
               QVector3D(printer_x_max, printer_y_min, printer_z_max), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_max, printer_y_min, printer_z_min),
               QVector3D(printer_x_min, printer_y_min, printer_z_min), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_max, printer_y_min, printer_z_min),
               QVector3D(printer_x_max, printer_y_max, printer_z_min), rgba, vertices, colors);

    appendLine(QVector3D(printer_x_min, printer_y_max, printer_z_min),
               QVector3D(printer_x_min, printer_y_max, printer_z_max), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_min, printer_y_max, printer_z_min),
               QVector3D(printer_x_min, printer_y_min, printer_z_min), rgba, vertices, colors);
    appendLine(QVector3D(printer_x_min, printer_y_max, printer_z_min),
               QVector3D(printer_x_max, printer_y_max, printer_z_min), rgba, vertices, colors);

    if (x_grid_dist > 0.0f && x_grid_dist < (printer_x_max - printer_x_min)) {
        for (float current_x = printer_x_min + x_grid_offset; current_x < printer_x_max; current_x += x_grid_dist) {
            appendLine(QVector3D(current_x, printer_y_min, printer_z_min),
                       QVector3D(current_x, printer_y_max, printer_z_min), rgba, vertices, colors);
        }
    }

    if (y_grid_dist > 0.0f && y_grid_dist < (printer_y_max - printer_y_min)) {
        for (float current_y = printer_y_min + y_grid_offset; current_y < printer_y_max; current_y += y_grid_dist) {
            appendLine(QVector3D(printer_x_min, current_y, printer_z_min),
                       QVector3D(printer_x_max, current_y, printer_z_min), rgba, vertices, colors);
        }
    }
}

void ShapeFactory::createBuildVolumeCylinder(float radius, float height, float x_grid_dist, float y_grid_dist,
                                             const QColor& color, std::vector<float>& vertices,
                                             std::vector<float>& colors) {
    if (radius <= kVectorEpsilon || height <= kVectorEpsilon) {
        return;
    }

    const float theta_increment = kTwoPi / static_cast<float>(kBuildVolumeCylinderSegments);
    const float vertical_step = kTwoPi / static_cast<float>(kBuildVolumeVerticalLines);
    const float r_squared = radius * radius;

    std::size_t line_count = (static_cast<std::size_t>(kBuildVolumeCylinderSegments) * 2) + kBuildVolumeVerticalLines;
    if (x_grid_dist > 0.0f) {
        line_count += countLoopValues(-radius, radius + kVectorEpsilon, x_grid_dist);
    }
    if (y_grid_dist > 0.0f) {
        line_count += countLoopValues(-radius, radius + kVectorEpsilon, y_grid_dist);
    }

    const Rgba rgba(color);
    reserveLineMesh(vertices, colors, line_count);

    for (float current_height : std::array<float, 2> {0.0f, height}) {
        for (int i = 0; i < kBuildVolumeCylinderSegments; ++i) {
            const float theta = static_cast<float>(i) * theta_increment;
            const float next_theta = static_cast<float>(i + 1) * theta_increment;
            appendLine(QVector3D(radius * std::cos(theta), radius * std::sin(theta), current_height),
                       QVector3D(radius * std::cos(next_theta), radius * std::sin(next_theta), current_height), rgba,
                       vertices, colors);
        }
    }

    for (int i = 0; i < kBuildVolumeVerticalLines; ++i) {
        const float theta = static_cast<float>(i) * vertical_step;
        const float x = radius * std::cos(theta);
        const float y = radius * std::sin(theta);
        appendLine(QVector3D(x, y, 0.0f), QVector3D(x, y, height), rgba, vertices, colors);
    }

    if (x_grid_dist > 0.0f) {
        for (float x = -radius; x <= radius + kVectorEpsilon; x += x_grid_dist) {
            const float y = std::sqrt(std::max(0.0f, r_squared - (x * x)));
            appendLine(QVector3D(x, y, 0.0f), QVector3D(x, -y, 0.0f), rgba, vertices, colors);
        }
    }

    if (y_grid_dist > 0.0f) {
        for (float y = -radius; y <= radius + kVectorEpsilon; y += y_grid_dist) {
            const float x = std::sqrt(std::max(0.0f, r_squared - (y * y)));
            appendLine(QVector3D(x, y, 0.0f), QVector3D(-x, y, 0.0f), rgba, vertices, colors);
        }
    }
}

QMatrix4x4 ShapeFactory::computeGcodeCylinderTransform(const QVector3D& start, const QVector3D& end) {
    QMatrix4x4 transform;
    transform.translate(start);

    QVector3D tangent = end - start;
    if (tangent.lengthSquared() <= kVectorEpsilonSquared) {
        return transform;
    }
    tangent.normalize();

    QVector3D normal = {GSM->getGlobal()->setting<float>(PS::Slicing::kSlicingVectorX),
                        GSM->getGlobal()->setting<float>(PS::Slicing::kSlicingVectorY),
                        GSM->getGlobal()->setting<float>(PS::Slicing::kSlicingVectorZ)};
    if (normal.lengthSquared() <= kVectorEpsilonSquared) {
        normal = QVector3D(0.0f, 0.0f, 1.0f);
    }
    else {
        normal.normalize();
    }

    QVector3D binormal = QVector3D::crossProduct(tangent, normal);
    if (binormal.lengthSquared() <= kVectorEpsilonSquared) {
        normal = (std::fabs(tangent.x()) < 0.9f) ? QVector3D(1.0f, 0.0f, 0.0f) : QVector3D(0.0f, 1.0f, 0.0f);
        binormal = QVector3D::crossProduct(tangent, normal);
    }
    binormal.normalize();

    normal = QVector3D::crossProduct(binormal, tangent);
    normal.normalize();

    QMatrix4x4 rotation;
    rotation.setColumn(0, QVector4D(binormal, 0.0f));
    rotation.setColumn(1, QVector4D(normal, 0.0f));
    rotation.setColumn(2, QVector4D(tangent, 0.0f));
    rotation.setColumn(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));

    transform *= rotation;
    return transform;
}

void ShapeFactory::createArcCylinder(float cylinder_height, const Point& start, const Point& center, const Point& end,
                                     bool is_ccw, const QColor& color, std::vector<float>& vertices,
                                     std::vector<float>& colors, std::vector<float>& normals) {
    const float major_radius = std::hypot(start.x() - center.x(), start.y() - center.y());
    if (major_radius <= kVectorEpsilon || cylinder_height <= kVectorEpsilon) {
        return;
    }

    QMatrix4x4 transform;
    transform.translate(center.toQVector3D());

    const Point projected_start(start.x(), start.y(), center.z());
    const Point reference(center.x() + major_radius, center.y(), center.z());
    transform.rotate(
        MathUtils::CreateQuaternion((reference - center).toQVector3D(), (projected_start - center).toQVector3D()));

    if (is_ccw) {
        createArcCylinderCCW(cylinder_height, start, center, end, transform, color, vertices, colors, normals);
    }
    else {
        createArcCylinder(cylinder_height, start, center, end, transform, color, vertices, colors, normals);
    }
}

} // namespace ORNL
