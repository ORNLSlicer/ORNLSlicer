
#include "graphics/support/shape_factory.h"

#include "geometry/segments/bezier.h"
#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"
#include "utilities/mathutils.h"

#include <QMatrix4x4>
#include <QtMath>
#include <vector>

#include <math.h>

namespace ORNL {
ShapeFactory::ShapeFactory() {}

void ShapeFactory::createRectangle(float length, float width, float height, const QMatrix4x4& transform,
                                   const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                                   std::vector<float>& normals) {
    QVector3D vertex0, vertex1, vertex2, vertex3, vertex4, vertex5, vertex6, vertex7;

    // Divide everything by 2 since we want to center the rectangle on the origin
    length /= 2.0f;
    width /= 2.0f;
    height /= 2.0f;

    // Create the eight vertices
    vertex0 = transform * QVector3D(-length, -width, height); // back  left  top

    vertex1 = transform * QVector3D(length, -width, height); // back  right top

    vertex2 = transform * QVector3D(length, width, height); // front right top

    vertex3 = transform * QVector3D(-length, width, height); // front left  top

    vertex4 = transform * QVector3D(-length, -width, -height); // back  left  bottom

    vertex5 = transform * QVector3D(length, -width, -height); // back  right bottom

    vertex6 = transform * QVector3D(length, width, -height); // front right bottom

    vertex7 = transform * QVector3D(-length, width, -height); // front left  bottom

    // Create the necessary 12 (2 for each face) triangles from the vertices

    // Bottom face
    vertices.push_back(vertex7.x());
    vertices.push_back(vertex7.y());
    vertices.push_back(vertex7.z());
    vertices.push_back(vertex6.x());
    vertices.push_back(vertex6.y());
    vertices.push_back(vertex6.z());
    vertices.push_back(vertex4.x());
    vertices.push_back(vertex4.y());
    vertices.push_back(vertex4.z());

    vertices.push_back(vertex6.x());
    vertices.push_back(vertex6.y());
    vertices.push_back(vertex6.z());
    vertices.push_back(vertex5.x());
    vertices.push_back(vertex5.y());
    vertices.push_back(vertex5.z());
    vertices.push_back(vertex4.x());
    vertices.push_back(vertex4.y());
    vertices.push_back(vertex4.z());

    // All vertices for each face have the same normal and color
    for (int i = 0; i < 6; i++) {
        normals.push_back(0.0f);
        normals.push_back(0.0f);
        normals.push_back(-1.0f);
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }

    // Top face
    vertices.push_back(vertex0.x());
    vertices.push_back(vertex0.y());
    vertices.push_back(vertex0.z());
    vertices.push_back(vertex1.x());
    vertices.push_back(vertex1.y());
    vertices.push_back(vertex1.z());
    vertices.push_back(vertex3.x());
    vertices.push_back(vertex3.y());
    vertices.push_back(vertex3.z());

    vertices.push_back(vertex1.x());
    vertices.push_back(vertex1.y());
    vertices.push_back(vertex1.z());
    vertices.push_back(vertex2.x());
    vertices.push_back(vertex2.y());
    vertices.push_back(vertex2.z());
    vertices.push_back(vertex3.x());
    vertices.push_back(vertex3.y());
    vertices.push_back(vertex3.z());

    for (int i = 0; i < 6; i++) {
        normals.push_back(0.0f);
        normals.push_back(0.0f);
        normals.push_back(1.0f);
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }

    // Left face
    vertices.push_back(vertex0.x());
    vertices.push_back(vertex0.y());
    vertices.push_back(vertex0.z());
    vertices.push_back(vertex3.x());
    vertices.push_back(vertex3.y());
    vertices.push_back(vertex3.z());
    vertices.push_back(vertex4.x());
    vertices.push_back(vertex4.y());
    vertices.push_back(vertex4.z());

    vertices.push_back(vertex3.x());
    vertices.push_back(vertex3.y());
    vertices.push_back(vertex3.z());
    vertices.push_back(vertex7.x());
    vertices.push_back(vertex7.y());
    vertices.push_back(vertex7.z());
    vertices.push_back(vertex4.x());
    vertices.push_back(vertex4.y());
    vertices.push_back(vertex4.z());

    for (int i = 0; i < 6; i++) {
        normals.push_back(-1.0f);
        normals.push_back(0.0f);
        normals.push_back(0.0f);
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }

    // Front face
    vertices.push_back(vertex3.x());
    vertices.push_back(vertex3.y());
    vertices.push_back(vertex3.z());
    vertices.push_back(vertex2.x());
    vertices.push_back(vertex2.y());
    vertices.push_back(vertex2.z());
    vertices.push_back(vertex7.x());
    vertices.push_back(vertex7.y());
    vertices.push_back(vertex7.z());

    vertices.push_back(vertex2.x());
    vertices.push_back(vertex2.y());
    vertices.push_back(vertex2.z());
    vertices.push_back(vertex6.x());
    vertices.push_back(vertex6.y());
    vertices.push_back(vertex6.z());
    vertices.push_back(vertex7.x());
    vertices.push_back(vertex7.y());
    vertices.push_back(vertex7.z());

    for (int i = 0; i < 6; i++) {
        normals.push_back(0.0f);
        normals.push_back(1.0f);
        normals.push_back(0.0f);
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }

    // Right face
    vertices.push_back(vertex2.x());
    vertices.push_back(vertex2.y());
    vertices.push_back(vertex2.z());
    vertices.push_back(vertex1.x());
    vertices.push_back(vertex1.y());
    vertices.push_back(vertex1.z());
    vertices.push_back(vertex6.x());
    vertices.push_back(vertex6.y());
    vertices.push_back(vertex6.z());

    vertices.push_back(vertex1.x());
    vertices.push_back(vertex1.y());
    vertices.push_back(vertex1.z());
    vertices.push_back(vertex5.x());
    vertices.push_back(vertex5.y());
    vertices.push_back(vertex5.z());
    vertices.push_back(vertex6.x());
    vertices.push_back(vertex6.y());
    vertices.push_back(vertex6.z());

    for (int i = 0; i < 6; i++) {
        normals.push_back(1.0f);
        normals.push_back(0.0f);
        normals.push_back(0.0f);
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }

    // Back face
    vertices.push_back(vertex1.x());
    vertices.push_back(vertex1.y());
    vertices.push_back(vertex1.z());
    vertices.push_back(vertex0.x());
    vertices.push_back(vertex0.y());
    vertices.push_back(vertex0.z());
    vertices.push_back(vertex5.x());
    vertices.push_back(vertex5.y());
    vertices.push_back(vertex5.z());

    vertices.push_back(vertex0.x());
    vertices.push_back(vertex0.y());
    vertices.push_back(vertex0.z());
    vertices.push_back(vertex4.x());
    vertices.push_back(vertex4.y());
    vertices.push_back(vertex4.z());
    vertices.push_back(vertex5.x());
    vertices.push_back(vertex5.y());
    vertices.push_back(vertex5.z());

    for (int i = 0; i < 6; i++) {
        normals.push_back(0.0f);
        normals.push_back(-1.0f);
        normals.push_back(0.0f);
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }
}

void ShapeFactory::createTorus(float inner_radius, float outer_radius, const QMatrix4x4& transform, const QColor& color,
                               std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals) {
    unsigned int segments = 50; // segments of torus
    unsigned int segment_slices =
        30;          // a segment is essentially a cylinder, this is the number of axial slices we want to use
    float theta = 0; // angle in plane of torus; used to distinguish segments
    float phi = 0;   // angle used to go around the tube; always go from 0 to 2PI for each segment
    float tube_radius = (outer_radius - inner_radius) / 2.0f;
    float mid_radius = inner_radius + tube_radius; // distance from center of torus to center of tube
    float phiIncrement = 2.0f * float(M_PI) / float(segment_slices);
    float thetaIncrement = 2.0f * float(M_PI) / float(segments);
    unsigned int next_radial_slice;       // used in loop that pushes back data
    unsigned int next_axial_slice;        // used in loop that pushes back data
    std::vector<QVector3D> temp_vertices; // store all the raw values of the vertices used to construct the triangles

    /*
     * The the torus will be drawn by segment by segment. Each segment is drawn as
     * a band of triangles around the center of torus' tube.
     * Torus is centered at (0,0,0) in local coordinates
     */

    // Outer loop is segments, inner loop is going around each segment
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < segment_slices; ++j) {
            // Parametric equations can be found on Wikipedia
            temp_vertices.push_back(transform * QVector3D((mid_radius + tube_radius * qCos(theta)) * qCos(phi),
                                                          tube_radius * qSin(theta),
                                                          (mid_radius + tube_radius * qCos(theta)) * qSin(phi)));

            phi += phiIncrement;
        }
        theta += thetaIncrement;
    }

    /* Vertices are arranged like so:
     * 0 to segment_slices-1: all vertices (aka every phi) at first theta
     *
     * segment_slices to 2*segement_slices-1: all vertices at second theta
     *
     * 2*segment_slices-1 to 3*segement_slices-1: all vertices at third theta
     *
     * And so on.
     * Last segment needs to connect to vertices at first theta.
     */

    QVector3D vertex1, vertex2, vertex3; // vertices of a face(triangle)
    QVector3D normal;                    // normal of a face shared by each of its vertices
    for (int i = 0; i < segments; ++i) {
        // Number from 0 to segments-1 of the next segment
        // Modulo so that the last segment connects to vertices at first theta
        next_radial_slice = (i + 1) % segments;

        // Each segment will have a number of triangles equal to twice the amount
        // of segment slices because it takes two triangles to connect four points

        // This loop constructs a segment (which is again, a cylinder) from two circles
        for (int j = 0; j < segment_slices; ++j) {
            // Number from 0 to segment_slices-1 of the next slice of this segment
            // Modulo so that the last segment connects to vertices at first theta
            next_axial_slice = (j + 1) % segment_slices;

            // First triangle's data
            vertex1 = temp_vertices.at(i * segment_slices + j); // Point on first circle
            vertices.push_back(vertex1.x());
            vertices.push_back(vertex1.y());
            vertices.push_back(vertex1.z());

            vertex2 = temp_vertices.at(next_radial_slice * segment_slices + j); // Point on second circle
            vertices.push_back(vertex2.x());
            vertices.push_back(vertex2.y());
            vertices.push_back(vertex2.z());

            vertex3 = temp_vertices.at(next_radial_slice * segment_slices + next_axial_slice); // Point on second circle
            vertices.push_back(vertex3.x());
            vertices.push_back(vertex3.y());
            vertices.push_back(vertex3.z());

            // Every vertex in a triangle has the same normal
            // Every vertex has the same color, this is just a convenient place to push back the color data
            normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
            for (int i = 0; i < 3; ++i) {
                normals.push_back(normal.x());
                normals.push_back(normal.y());
                normals.push_back(normal.z());
                colors.push_back(color.redF());
                colors.push_back(color.greenF());
                colors.push_back(color.blueF());
                colors.push_back(color.alphaF());
            }

            // Second triangle's data
            vertex1 = temp_vertices.at(i * segment_slices + j); // Point on first circle
            vertices.push_back(vertex1.x());
            vertices.push_back(vertex1.y());
            vertices.push_back(vertex1.z());

            vertex2 = temp_vertices.at(next_radial_slice * segment_slices + next_axial_slice); // Point on second circle
            vertices.push_back(vertex2.x());
            vertices.push_back(vertex2.y());
            vertices.push_back(vertex2.z());

            vertex3 = temp_vertices.at(i * segment_slices + next_axial_slice); // Point on first circle
            vertices.push_back(vertex3.x());
            vertices.push_back(vertex3.y());
            vertices.push_back(vertex3.z());

            normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
            for (int i = 0; i < 3; ++i) {
                normals.push_back(normal.x());
                normals.push_back(normal.y());
                normals.push_back(normal.z());
                colors.push_back(color.redF());
                colors.push_back(color.greenF());
                colors.push_back(color.blueF());
                colors.push_back(color.alphaF());
            }
        }
    }
}

void ShapeFactory::createArcCylinderCCW(float cylinder_height, const Point& start, const Point& center,
                                        const Point& end, const QMatrix4x4& transform, const QColor& color,
                                        std::vector<float>& vertices, std::vector<float>& colors,
                                        std::vector<float>& normals) {
    const unsigned int cross_sectional_resolution = 20; // number of points that make up a cross-sectional circle
    const unsigned int arc_segments = 75;               // number of cylindrical segments that comprise an arc

    Angle angle;
    if (MathUtils::orientation(start, center, end) == 0) {
        // These are co-linear
        if (start == end) {
            // this is a circle
            angle = 2.0f * M_PI;
        }
        else {
            angle = M_PI;
        }
    }
    else {
        double a = qAtan2(center.x() - start.x(), center.y() - start.y());
        double b = qAtan2(center.x() - end.x(), center.y() - end.y());

        angle = Angle(a - b);
        if (angle < 0) {
            angle = (2.0f * M_PI) + angle;
        }
    }

    float theta = 0; // angle around the cross-sectional circle
    float theta_increment =
        2.0f * float(M_PI) /
        float(cross_sectional_resolution); // the amount to add each iteration through on the cross_sectional circle
    float phi = 0;                         // angle around the arc
    float phi_increment = angle() / arc_segments; // the amount to add each iteration though on the arc

    float major_radius = Point(center.x(), center.y(), 0)
                             .distance(Point(start.x(), start.y(),
                                             0))(); // the distance from the center of the arc to the start/ end points
    float minor_radius = cylinder_height / 2.0f;    // the radius the cross-sectional circle

    QVector3D temp_vertices[arc_segments + 1][cross_sectional_resolution];

    auto height = float(0.0);
    auto height_increment = (end.z() - start.z()) / arc_segments;
    for (auto& ring_vertices : temp_vertices) {
        theta = 0;
        for (auto& vertex : ring_vertices) {
            vertex = transform * QVector3D(qCos(phi) * (major_radius + (minor_radius * qCos(theta))),
                                           qSin(phi) * (major_radius + (minor_radius * qCos(theta))),
                                           (qSin(theta) * minor_radius) + height);
            theta += theta_increment;
        }
        height += height_increment;
        phi += phi_increment;
    }

    // Connect first vertex to cap start
    for (int i = 0; i < cross_sectional_resolution; ++i) {
        appendTriangle(transform * QVector3D(major_radius, 0, 0), temp_vertices[0][i],
                       temp_vertices[0][(i + 1) % cross_sectional_resolution], color, vertices, colors, normals);
    }

    for (int slice_index = 0; slice_index < arc_segments; ++slice_index) {
        auto next_slice = slice_index + 1;
        for (int vertex_index = 0; vertex_index < cross_sectional_resolution; ++vertex_index) {
            auto next_point_index = (vertex_index + 1) % cross_sectional_resolution;

            // Need to build two triangles per side of rectangle
            appendTriangle(temp_vertices[slice_index][vertex_index], temp_vertices[next_slice][vertex_index],
                           temp_vertices[slice_index][next_point_index], color, vertices, colors, normals);

            appendTriangle(temp_vertices[next_slice][next_point_index], temp_vertices[slice_index][next_point_index],
                           temp_vertices[next_slice][vertex_index], color, vertices, colors, normals);
        }
    }

    // Cap the end
    for (int i = 0; i < cross_sectional_resolution; ++i) {
        appendTriangle(temp_vertices[arc_segments][(i + 1) % cross_sectional_resolution],
                       temp_vertices[arc_segments][i],
                       transform * QVector3D(major_radius * qCos(angle()), major_radius * qSin(angle()), height), color,
                       vertices, colors, normals);
    }
}

void ShapeFactory::createArcCylinder(float cylinder_height, const Point& start, const Point& center, const Point& end,
                                     const QMatrix4x4& transform, const QColor& color, std::vector<float>& vertices,
                                     std::vector<float>& colors, std::vector<float>& normals) {
    const unsigned int cross_sectional_resolution = 20; // number of points that make up a cross-sectional circle
    const unsigned int arc_segments = 75;               // number of cylindrical segments that comprise an arc

    Angle angle;
    short orientation = MathUtils::orientation(start, center, end);
    if (orientation == 0) {
        // These are co-linear
        if (start == end) {
            // this is a circle
            angle = 2.0f * M_PI;
        }
        else {
            angle = M_PI;
        }
    }
    else {
        double a = qAtan2(center.x() - start.x(), center.y() - start.y());
        double b = qAtan2(center.x() - end.x(), center.y() - end.y());

        angle = Angle(b - a);
        if (angle < 0) {
            angle = (2.0f * M_PI) + angle;
        }
    }

    float theta = 0; // angle around the cross-sectional circle
    float theta_increment =
        2.0f * float(M_PI) /
        float(cross_sectional_resolution); // the amount to add each iteration through on the cross_sectional circle
    float phi = 2.0f * float(M_PI);        // Since this is clockwise, phi will start at 2 * Pi
    float phi_increment = (angle() / float(arc_segments)); // and decrease by arc_segments number of increments

    float major_radius = Point(center.x(), center.y(), 0)
                             .distance(Point(start.x(), start.y(),
                                             0))(); // the distance from the center of the arc to the start/ end points
    float minor_radius = cylinder_height / 2.0f;    // the radius the cross-sectional circle

    QVector3D temp_vertices[arc_segments + 1][cross_sectional_resolution];

    auto height = float(0.0);
    auto height_increment = (end.z() - start.z()) / arc_segments;
    for (auto& ring_vertices : temp_vertices) {
        theta = 0;
        for (auto& vertex : ring_vertices) {
            vertex = transform * QVector3D(qCos(phi) * (major_radius + (minor_radius * qCos(theta))),
                                           qSin(phi) * (major_radius + (minor_radius * qCos(theta))),
                                           (qSin(theta) * minor_radius) + height);
            theta += theta_increment;
        }
        height += height_increment;
        phi -= phi_increment;
    }

    // Connect first vertex to cap start
    for (int i = 0; i < cross_sectional_resolution; ++i) {
        appendTriangle(temp_vertices[0][(i + 1) % cross_sectional_resolution], temp_vertices[0][i],
                       transform * QVector3D(major_radius, 0, 0), color, vertices, colors, normals);
    }

    for (int slice_index = 0; slice_index < arc_segments; ++slice_index) {
        auto next_slice = slice_index + 1;
        for (int vertex_index = 0; vertex_index < cross_sectional_resolution; ++vertex_index) {
            auto next_point_index = (vertex_index + 1) % cross_sectional_resolution;

            // Need to build two triangles per side of rectangle
            appendTriangle(temp_vertices[slice_index][next_point_index], temp_vertices[next_slice][vertex_index],
                           temp_vertices[slice_index][vertex_index], color, vertices, colors, normals);

            appendTriangle(temp_vertices[next_slice][vertex_index], temp_vertices[slice_index][next_point_index],
                           temp_vertices[next_slice][next_point_index], color, vertices, colors, normals);
        }
    }

    // Cap the end
    for (int i = 0; i < cross_sectional_resolution; ++i) {
        appendTriangle(
            transform * QVector3D(major_radius * qCos(angle() * -1), major_radius * qSin(angle() * -1), height),
            temp_vertices[arc_segments][i], temp_vertices[arc_segments][(i + 1) % cross_sectional_resolution], color,
            vertices, colors, normals);
    }
}

void ShapeFactory::createSplineCylinder(const float diameter, const Point& start, const Point& control_a,
                                        const Point& control_b, const Point& end, const QColor& color,
                                        std::vector<float>& vertices, std::vector<float>& colors,
                                        std::vector<float>& normals) {
    const unsigned int cross_sectional_resolution = 20; // number of points that make up a cross-sectional circle
    const unsigned int spline_segments = 75;            // number of cylindrical segments that comprise an spline

    float theta = 0; // angle around the cross-sectional circle
    float theta_increment =
        2.0f * float(M_PI) /
        float(cross_sectional_resolution); // the amount to add each iteration through on the cross_sectional circle

    QVector3D temp_vertices[spline_segments + 1][cross_sectional_resolution];

    double t = 0.0;
    double increment = 1.0 / spline_segments;
    BezierSegment curve(start, control_a, control_b, end);

    for (auto& ring_vertices : temp_vertices) {
        theta = 0;
        Point center = curve.getPointAlong(t);
        Point next_center = curve.getPointAlong(t + increment);

        for (auto& vertex : ring_vertices) {
            Point p(center.x() + ((diameter / 2) * qCos(theta)), center.y(),
                    center.z() + ((diameter / 2) * qSin(theta)));
            p = p.rotateAround(center, MathUtils::signedInternalAngle(Point(center.x(), center.y() + 10, center.z()),
                                                                      center, next_center));

            vertex = p.toQVector3D();
            theta += theta_increment;
        }
        t += increment;
    }

    // Connect first vertex to cap start
    for (int i = 0; i < cross_sectional_resolution; ++i) {
        appendTriangle(curve.getPointAlong(0).toQVector3D(), temp_vertices[0][i],
                       temp_vertices[0][(i + 1) % cross_sectional_resolution], color, vertices, colors, normals);
    }

    for (int slice_index = 0; slice_index < spline_segments; ++slice_index) {
        auto next_slice = slice_index + 1;
        for (int vertex_index = 0; vertex_index < cross_sectional_resolution; ++vertex_index) {
            auto next_point_index = (vertex_index + 1) % cross_sectional_resolution;

            // Need to build two triangles per side of rectangle
            appendTriangle(temp_vertices[slice_index][vertex_index], temp_vertices[next_slice][vertex_index],
                           temp_vertices[slice_index][next_point_index], color, vertices, colors, normals);

            appendTriangle(temp_vertices[next_slice][next_point_index], temp_vertices[slice_index][next_point_index],
                           temp_vertices[next_slice][vertex_index], color, vertices, colors, normals);
        }
    }

    // Cap the end
    for (int i = 0; i < cross_sectional_resolution; ++i) {
        appendTriangle(temp_vertices[spline_segments][(i + 1) % cross_sectional_resolution],
                       temp_vertices[spline_segments][i], curve.getPointAlong(1.0).toQVector3D(), color, vertices,
                       colors, normals);
    }
}

void ShapeFactory::appendTriangle(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2, const QColor& color,
                                  std::vector<float>& vertices, std::vector<float>& colors,
                                  std::vector<float>& normals) {
    // Convert color to RGBA format
    std::array<float, 4> rgba = {static_cast<float>(color.redF()), static_cast<float>(color.greenF()),
                                 static_cast<float>(color.blueF()), static_cast<float>(color.alphaF())};

    // Add the vertices
    vertices.insert(vertices.end(), {v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z(), v2.x(), v2.y(), v2.z()});

    // Compute the normal
    QVector3D normal = QVector3D::crossProduct(v1 - v0, v2 - v0).normalized();

    // Add the normal and color for each vertex
    for (int i = 0; i < 3; ++i) {
        normals.insert(normals.end(), {normal.x(), normal.y(), normal.z()});
        colors.insert(colors.end(), rgba.begin(), rgba.end());
    }
}

void ShapeFactory::createCylinder(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                                  std::vector<float>& vertices, std::vector<float>& colors,
                                  std::vector<float>& normals) {
    unsigned int segments = 50; // Number of arc segments used to approximate a cylinder
    float theta = 0.0f;         // Measure of angle at which each segment starts; first theta is just 0
    float thetaIncrement = 2.0f * float(M_PI) / float(segments);
    unsigned int center_top, center_bottom, first_top, second_top, first_bottom,
        second_bottom;                    // Used to correctly place vertices in triangles
    std::vector<QVector3D> temp_vertices; // store all the raw values of the vertices used to construct the triangles

    /*
     * The cylinder is drawn segment by segment. Each segment is represented
     * by four triangles and six points: one triangle from the center of the top circle
     * to two points on the top circle, then two triangles to make the vertical wall,
     * then one triangle from the center of the bottom circle to two points on the bottom circle
     *
     * The bottom center is at (0,0,0) and the top center is at (0,0,height)
     */

    // Start with just the top center point
    temp_vertices.push_back(transform * QVector3D(0.0f, 0.0f, height));

    // Each iteration of this loop will create one vertex on the top circle and
    // another vertex on the bottom circle
    for (int i = 0; i < segments; ++i) {
        temp_vertices.push_back(transform *
                                QVector3D(radius * float(qCos(theta)), radius * float(qSin(theta)), height));
        temp_vertices.push_back(transform * QVector3D(radius * float(qCos(theta)), radius * float(qSin(theta)), 0.0f));
        theta += thetaIncrement;
    }

    // End with just the bottom center point
    temp_vertices.push_back(transform * QVector3D(0.0f, 0.0f, 0.0f));

    /* Vertices are arranged like so:
     * 0: top center
     * 1 to 2*slices: odds on top, evens on bottom;
     *                pairs of vertices like 1&2, 3&4, 5&6, etc. have same theta
     * 2*slices+1: bottom center
     *
     * Each loop will make 4 triangles to connect top center, bottom center, and two pairs
     */

    center_top = 0;
    center_bottom = 2 * segments + 1;

    // first_top and first_bottom have same theta but different z
    first_top = 1;
    first_bottom = 2;

    // second_top and second_bottom have same theta but different z
    second_top = 3;
    second_bottom = 4;

    // Create four triangles per slice.
    QVector3D vertex1, vertex2, vertex3; // vertices of a face
    QVector3D normal;                    // normal of a face shared by each of its vertices
    for (int i = 0; i < segments; ++i) {
        // Triangle on top circle
        vertex1 = temp_vertices.at(center_top);
        vertex2 = temp_vertices.at(first_top);
        vertex3 = temp_vertices.at(second_top);
        vertices.push_back(vertex1.x());
        vertices.push_back(vertex1.y());
        vertices.push_back(vertex1.z());
        vertices.push_back(vertex2.x());
        vertices.push_back(vertex2.y());
        vertices.push_back(vertex2.z());
        vertices.push_back(vertex3.x());
        vertices.push_back(vertex3.y());
        vertices.push_back(vertex3.z());

        // All vertices in a triangle share the same normal
        // All vertices have the same color, convenient to put in for loop here
        normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
        for (int i = 0; i < 3; ++i) {
            normals.push_back(normal.x());
            normals.push_back(normal.y());
            normals.push_back(normal.z());
            colors.push_back(color.redF());
            colors.push_back(color.greenF());
            colors.push_back(color.blueF());
            colors.push_back(color.alphaF());
        }

        // Triangles along cylinder side
        vertex1 = temp_vertices.at(second_top);
        vertex2 = temp_vertices.at(first_top);
        vertex3 = temp_vertices.at(first_bottom);
        vertices.push_back(vertex1.x());
        vertices.push_back(vertex1.y());
        vertices.push_back(vertex1.z());
        vertices.push_back(vertex2.x());
        vertices.push_back(vertex2.y());
        vertices.push_back(vertex2.z());
        vertices.push_back(vertex3.x());
        vertices.push_back(vertex3.y());
        vertices.push_back(vertex3.z());

        normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
        for (int i = 0; i < 3; ++i) {
            normals.push_back(normal.x());
            normals.push_back(normal.y());
            normals.push_back(normal.z());
            colors.push_back(color.redF());
            colors.push_back(color.greenF());
            colors.push_back(color.blueF());
            colors.push_back(color.alphaF());
        }

        vertex1 = temp_vertices.at(second_top);
        vertex2 = temp_vertices.at(first_bottom);
        vertex3 = temp_vertices.at(second_bottom);
        vertices.push_back(vertex1.x());
        vertices.push_back(vertex1.y());
        vertices.push_back(vertex1.z());
        vertices.push_back(vertex2.x());
        vertices.push_back(vertex2.y());
        vertices.push_back(vertex2.z());
        vertices.push_back(vertex3.x());
        vertices.push_back(vertex3.y());
        vertices.push_back(vertex3.z());

        normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
        for (int i = 0; i < 3; ++i) {
            normals.push_back(normal.x());
            normals.push_back(normal.y());
            normals.push_back(normal.z());
            colors.push_back(color.redF());
            colors.push_back(color.greenF());
            colors.push_back(color.blueF());
            colors.push_back(color.alphaF());
        }

        // Triangle on bottom circle
        vertex1 = temp_vertices.at(second_bottom);
        vertex2 = temp_vertices.at(first_bottom);
        vertex3 = temp_vertices.at(center_bottom);
        vertices.push_back(vertex1.x());
        vertices.push_back(vertex1.y());
        vertices.push_back(vertex1.z());
        vertices.push_back(vertex2.x());
        vertices.push_back(vertex2.y());
        vertices.push_back(vertex2.z());
        vertices.push_back(vertex3.x());
        vertices.push_back(vertex3.y());
        vertices.push_back(vertex3.z());

        normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
        for (int i = 0; i < 3; ++i) {
            normals.push_back(normal.x());
            normals.push_back(normal.y());
            normals.push_back(normal.z());
            colors.push_back(color.redF());
            colors.push_back(color.greenF());
            colors.push_back(color.blueF());
            colors.push_back(color.alphaF());
        }

        first_top = second_top;
        first_bottom = second_bottom;
        second_top = first_top + 2;
        second_bottom = first_bottom + 2;

        // Last segment connects with first two vertices (vertices 1&2),
        // so this is just doing some modulo
        if (second_top > (2 * segments)) {
            second_top -= 2 * segments;
        }
        if (second_bottom > (2 * segments)) {
            second_bottom -= 2 * segments;
        }
    }
}

void ShapeFactory::createSphere(float radius, int sectorCount, int stackCount, const QMatrix4x4& transform,
                                const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                                std::vector<float>& normals) {
    std::vector<QVector3D> tmpVertices;

    float sectorStep = 2 * M_PI / sectorCount;
    float stackStep = M_PI / stackCount;
    float sectorAngle, stackAngle;

    // compute all vertices first, each vertex contains (x,y,z,s,t) except normal
    for (int i = 0; i <= stackCount; ++i) {
        stackAngle = M_PI / 2 - i * stackStep; // starting from pi/2 to -pi/2
        float xy = radius * cosf(stackAngle);  // r * cos(u)
        float z = radius * sinf(stackAngle);   // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for (int j = 0; j <= sectorCount; ++j) {
            sectorAngle = j * sectorStep; // starting from 0 to 2pi

            QVector3D vertex;
            vertex.setX(xy * cosf(sectorAngle)); // x = r * cos(u) * cos(v)
            vertex.setY(xy * sinf(sectorAngle)); // y = r * cos(u) * sin(v)
            vertex.setZ(z);                      // z = r * sin(u)
            tmpVertices.push_back(transform * vertex);
        }
    }

    vertices.reserve(tmpVertices.size());
    normals.reserve(vertices.size() / 3.0);
    colors.reserve(vertices.size() / 3.0 * 4.0);

    QVector3D v1, v2, v3, v4; // 4 vertex positions and tex coords
    QVector3D n;              // 1 face normal

    int i, j, k, vi1, vi2;
    int index = 0; // index for vertex
    for (i = 0; i < stackCount; ++i) {
        vi1 = i * (sectorCount + 1); // index of tmpVertices
        vi2 = (i + 1) * (sectorCount + 1);

        for (j = 0; j < sectorCount; ++j, ++vi1, ++vi2) {
            // get 4 vertices per sector
            //  v1--v3
            //  |    |
            //  v2--v4
            v1 = tmpVertices[vi1];
            v2 = tmpVertices[vi2];
            v3 = tmpVertices[vi1 + 1];
            v4 = tmpVertices[vi2 + 1];

            // if 1st stack and last stack, store only 1 triangle per sector
            // otherwise, store 2 triangles (quad) per sector
            if (i == 0) { // a triangle for first stack ==========================
                vertices.push_back(v1.x());
                vertices.push_back(v1.y());
                vertices.push_back(v1.z());
                vertices.push_back(v2.x());
                vertices.push_back(v2.y());
                vertices.push_back(v2.z());
                vertices.push_back(v4.x());
                vertices.push_back(v4.y());
                vertices.push_back(v4.z());

                n = QVector3D::crossProduct(v4 - v2, v1 - v2).normalized();
                // put normal
                for (k = 0; k < 3; ++k) { // same normals for 3 vertices
                    normals.push_back(n.x());
                    normals.push_back(n.y());
                    normals.push_back(n.z());
                    colors.push_back(color.redF());
                    colors.push_back(color.greenF());
                    colors.push_back(color.blueF());
                    colors.push_back(color.alphaF());
                }
                index += 3; // for next
            }
            else if (i == (stackCount - 1)) { // a triangle for last stack =========
                vertices.push_back(v1.x());
                vertices.push_back(v1.y());
                vertices.push_back(v1.z());
                vertices.push_back(v2.x());
                vertices.push_back(v2.y());
                vertices.push_back(v2.z());
                vertices.push_back(v3.x());
                vertices.push_back(v3.y());
                vertices.push_back(v3.z());

                n = QVector3D::crossProduct(v3 - v2, v1 - v2).normalized();

                // put normal
                for (k = 0; k < 3; ++k) { // same normals for 3 vertices
                    normals.push_back(n.x());
                    normals.push_back(n.y());
                    normals.push_back(n.z());
                    colors.push_back(color.redF());
                    colors.push_back(color.greenF());
                    colors.push_back(color.blueF());
                    colors.push_back(color.alphaF());
                }
                index += 3; // for next
            }
            else { // 2 triangles for others ====================================
                // put quad vertices: v1-v2-v3-v4
                vertices.push_back(v1.x());
                vertices.push_back(v1.y());
                vertices.push_back(v1.z());
                vertices.push_back(v2.x());
                vertices.push_back(v2.y());
                vertices.push_back(v2.z());
                vertices.push_back(v3.x());
                vertices.push_back(v3.y());
                vertices.push_back(v3.z());

                vertices.push_back(v3.x());
                vertices.push_back(v3.y());
                vertices.push_back(v3.z());
                vertices.push_back(v2.x());
                vertices.push_back(v2.y());
                vertices.push_back(v2.z());
                vertices.push_back(v4.x());
                vertices.push_back(v4.y());
                vertices.push_back(v4.z());
                // 0-1-2, 2-1-3

                // put normal
                n = QVector3D::crossProduct(v3 - v2, v1 - v2).normalized();

                // put normal
                for (k = 0; k < 6; ++k) { // same normals for 6 vertices
                    normals.push_back(n.x());
                    normals.push_back(n.y());
                    normals.push_back(n.z());
                    colors.push_back(color.redF());
                    colors.push_back(color.greenF());
                    colors.push_back(color.blueF());
                    colors.push_back(color.alphaF());
                }

                index += 4; // for next
            }
        }
    }
}

void ShapeFactory::createGcodeCylinder(const float& width, const float& length, const float& height,
                                       const QVector3D& start, const QVector3D& end, const QColor& color,
                                       std::vector<float>& vertices, std::vector<float>& colors,
                                       std::vector<float>& normals) {
    // Compute the transformation matrix for the clipped cylinder
    QMatrix4x4 transform = computeGcodeCylinderTransform(start, end);

    // Radius of the clipped cylinder and number of quads per side
    float radius;
    unsigned int quads_per_side;

    // If the height is greater than the width, the clipped cylinder is a rectangular prism
    if (height > width) {
        radius = std::sqrt((width / 2.0f) * (width / 2.0f) + (height / 2.0f) * (height / 2.0f));
        quads_per_side = 1;
    }
    else { // Otherwise, the clipped cylinder is approximated by 6 quads per side
        radius = width / 2.0f;
        quads_per_side = 6;
    }

    // Compute number of vertices per arc and side of the clipped cylinder
    unsigned int vertices_per_arc = quads_per_side + 1;
    unsigned int vertices_per_side = 2 * vertices_per_arc;

    // Compute the angular range based on the height and radius
    float theta_start = -std::asin((height / 2.0f) / radius);
    float theta_end = -theta_start;
    float theta_increment = (theta_end - theta_start) / quads_per_side;

    // Vectors to hold the vertices of the top and bottom of the clipped cylinder
    std::vector<QVector3D> top_vertices(2 * vertices_per_arc);
    std::vector<QVector3D> bottom_vertices(2 * vertices_per_arc);

    // Generate vertices vertices for the top and bottom of the clipped cylinder
    for (int i = 0; i < vertices_per_arc; ++i) {
        float theta = theta_start + i * theta_increment;

        float x = radius * std::cos(theta);
        float y = radius * std::sin(theta);

        // Right side vertices
        top_vertices[i] = transform * QVector3D(x, y, length);
        bottom_vertices[i] = transform * QVector3D(x, y, 0.0f);

        // Left side vertices
        top_vertices[i + vertices_per_arc] = transform * QVector3D(-x, -y, length);
        bottom_vertices[i + vertices_per_arc] = transform * QVector3D(-x, -y, 0.0f);
    }

    // Compute the top and bottom center points
    QVector3D top_center = transform * QVector3D(0.0f, 0.0f, length);
    QVector3D bottom_center = transform * QVector3D(0.0f, 0.0f, 0.0f);

    // Generate triangle faces for each slice (quads split into two triangles)
    for (unsigned int i = 0; i < vertices_per_side; ++i) {
        unsigned int j = (i + 1) % vertices_per_side;
        appendTriangle(top_center, top_vertices[j], top_vertices[i], color, vertices, colors, normals); // Top triangle
        appendTriangle(top_vertices[i], bottom_vertices[j], bottom_vertices[i], color, vertices, colors,
                       normals); // Quad triangle 1
        appendTriangle(top_vertices[i], top_vertices[j], bottom_vertices[j], color, vertices, colors,
                       normals); // Quad triangle 2
        appendTriangle(bottom_center, bottom_vertices[i], bottom_vertices[j], color, vertices, colors,
                       normals); // Bottom triangle
    }
}

void ShapeFactory::createCone(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                              std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals) {
    unsigned int slices = 50; // Number of segments used to approximate a cone
    float theta = 0;          // Theta of each segment, starts at 0
    float theta_increment = 2 * float(M_PI) / float(slices);
    unsigned int start_vertex;            // Used to build triangles at the end
    unsigned int next_vertex;             // Used to build triangles at the end
    std::vector<QVector3D> temp_vertices; // store all the raw values of the vertices used to construct the triangles

    /*
     * The cone is drawn as a series of pairs of triangles: one with a vertex at the tip and two
     * vertices along the circle. The other with two vertices on the circle and one vertex at the base's center
     */

    // Tip of cone
    temp_vertices.push_back(transform * QVector3D(0.0f, 0.0f, height));

    // Perimeter of base
    for (int i = 0; i < slices; ++i) {
        temp_vertices.push_back(transform * QVector3D(radius * qSin(theta), radius * qCos(theta), 0.0f));
        theta += theta_increment;
    }

    // Center of base
    temp_vertices.push_back(transform * QVector3D(0.0f, 0.0f, 0.0f));

    // Iterativel draw pairs of triangles
    start_vertex = 1;
    next_vertex = 2;
    QVector3D vertex1, vertex2, vertex3;
    QVector3D normal;
    for (int i = 0; i < slices; ++i) {
        // Triangle from tip to two points on circle
        vertex1 = temp_vertices.at(0);
        vertex2 = temp_vertices.at(next_vertex);
        vertex3 = temp_vertices.at(start_vertex);
        vertices.push_back(vertex1.x());
        vertices.push_back(vertex1.y());
        vertices.push_back(vertex1.z());
        vertices.push_back(vertex2.x());
        vertices.push_back(vertex2.y());
        vertices.push_back(vertex2.z());
        vertices.push_back(vertex3.x());
        vertices.push_back(vertex3.y());
        vertices.push_back(vertex3.z());

        // Each vertex in a face has the same normals
        // Each vertex has same color, convenient to push back color data in these loops
        normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
        for (int i = 0; i < 3; ++i) {
            normals.push_back(normal.x());
            normals.push_back(normal.y());
            normals.push_back(normal.z());
            colors.push_back(color.redF());
            colors.push_back(color.greenF());
            colors.push_back(color.blueF());
            colors.push_back(color.alphaF());
        }

        // Triangle from center of base to two points on circle
        vertex1 = temp_vertices.at(start_vertex);
        vertex2 = temp_vertices.at(next_vertex);
        vertex3 = temp_vertices.at((slices + 1));
        vertices.push_back(vertex1.x());
        vertices.push_back(vertex1.y());
        vertices.push_back(vertex1.z());
        vertices.push_back(vertex2.x());
        vertices.push_back(vertex2.y());
        vertices.push_back(vertex2.z());
        vertices.push_back(vertex3.x());
        vertices.push_back(vertex3.y());
        vertices.push_back(vertex3.z());

        normal = QVector3D::crossProduct(vertex3 - vertex2, vertex1 - vertex2).normalized();
        for (int i = 0; i < 3; ++i) {
            normals.push_back(normal.x());
            normals.push_back(normal.y());
            normals.push_back(normal.z());
            colors.push_back(color.redF());
            colors.push_back(color.greenF());
            colors.push_back(color.blueF());
            colors.push_back(color.alphaF());
        }

        start_vertex = next_vertex;
        ++next_vertex;

        // Handle last slice where you come back to the first vertex to be added after the cone tip
        if (next_vertex > slices) {
            next_vertex -= slices;
        }
    }
}

void ShapeFactory::createFullGimbal(float inner_radius, float outer_radius, const QVector3D& center,
                                    std::vector<float>& vertices, std::vector<float>& colors,
                                    std::vector<float>& normals) {
    QMatrix4x4 transform;
    QColor color;

    // Make sure cylinder has same radius as the torii's tube
    float cylinder_radius = (outer_radius - inner_radius) / 2.0f;
    // The rest of these floats are just hard-coded with what looks good
    float cylinder_height = outer_radius + 2 * cylinder_radius;
    float cone_radius = cylinder_radius * 1.5f;
    float cone_height = cylinder_radius * 3.5f;

    color = Constants::Colors::kRed;
    transform.setToIdentity();
    transform.translate(center);

    // Draw red torus in x-z plane
    createTorus(inner_radius, outer_radius, transform, color, vertices, colors, normals);

    // Draw green torus in y-z plane
    color = Constants::Colors::kGreen;
    transform.rotate(90.0f, 0.0f, 0.0f, 1.0f);
    createTorus(inner_radius, outer_radius, transform, color, vertices, colors, normals);
    transform.rotate(-90.0f, 0.0f, 0.0f, 1.0f);

    // Draw blue torus in x-y plane
    color = Constants::Colors::kBlue;
    transform.rotate(90.0f, 1.0f, 0.0f, 0.0f);
    createTorus(inner_radius, outer_radius, transform, color, vertices, colors, normals);
    transform.rotate(-90.0f, 1.0f, 0.0f, 0.0f);

    // Draw blue arrow pointing in the +z direction
    color = Constants::Colors::kBlue;
    createCylinder(cylinder_radius, cylinder_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, cylinder_height);
    createCone(cone_radius, cone_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, -cylinder_height);

    // Draw red arrow pointing in the +y direction
    color = Constants::Colors::kRed;
    transform.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
    createCylinder(cylinder_radius, cylinder_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, cylinder_height);
    createCone(cone_radius, cone_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, -cylinder_height);
    transform.rotate(90.0f, 1.0f, 0.0f, 0.0f);

    // Draw green arrow pointing in the +x direction
    color = Constants::Colors::kGreen;
    transform.rotate(90.0f, 0.0f, 1.0f, 0.0f);
    createCylinder(cylinder_radius, cylinder_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, cylinder_height);
    createCone(cone_radius, cone_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, -cylinder_height);
    transform.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
}

void ShapeFactory::createXYGimbal(float inner_radius, float outer_radius, const QVector3D& center,
                                  std::vector<float>& vertices, std::vector<float>& colors,
                                  std::vector<float>& normals) {
    QMatrix4x4 transform;
    QColor color;

    // Make sure cylinder has same radius as the torii's tube
    float cylinder_radius = (outer_radius - inner_radius) / 2.0f;
    // The rest of these floats are just hard-coded with what looks good
    float cylinder_height = outer_radius + 2 * cylinder_radius;
    float cone_radius = cylinder_radius * 1.5f;
    float cone_height = cylinder_radius * 3.5f;

    transform.setToIdentity();
    transform.translate(center);

    // Draw blue torus in x-y plane
    color = Constants::Colors::kBlue;
    transform.rotate(90.0f, 1.0f, 0.0f, 0.0f);
    createTorus(inner_radius, outer_radius, transform, color, vertices, colors, normals);
    transform.rotate(-90.0f, 1.0f, 0.0f, 0.0f);

    // Draw red arrow pointing in the +y direction
    color = Constants::Colors::kRed;
    transform.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
    createCylinder(cylinder_radius, cylinder_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, cylinder_height);
    createCone(cone_radius, cone_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, -cylinder_height);
    transform.rotate(90.0f, 1.0f, 0.0f, 0.0f);

    // Draw green arrow pointing in the +x direction
    color = Constants::Colors::kGreen;
    transform.rotate(90.0f, 0.0f, 1.0f, 0.0f);
    createCylinder(cylinder_radius, cylinder_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, cylinder_height);
    createCone(cone_radius, cone_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, -cylinder_height);
    transform.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
}

void ShapeFactory::createXZGimbal(float inner_radius, float outer_radius, const QVector3D& center,
                                  std::vector<float>& vertices, std::vector<float>& colors,
                                  std::vector<float>& normals) {
    QMatrix4x4 transform;
    QColor color;

    // Make sure cylinder has same radius as the torii's tube
    float cylinder_radius = (outer_radius - inner_radius) / 2.0f;
    // The rest of these floats are just hard-coded with what looks good
    float cylinder_height = outer_radius + 2 * cylinder_radius;
    float cone_radius = cylinder_radius * 1.5f;
    float cone_height = cylinder_radius * 3.5f;

    color = Constants::Colors::kRed;
    transform.setToIdentity();
    transform.translate(center);

    // Draw red torus in x-z plane
    createTorus(inner_radius, outer_radius, transform, color, vertices, colors, normals);

    // Draw blue arrow pointing in the +z direction
    color = Constants::Colors::kBlue;
    createCylinder(cylinder_radius, cylinder_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, cylinder_height);
    createCone(cone_radius, cone_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, -cylinder_height);

    // Draw green arrow pointing in the +x direction
    color = Constants::Colors::kGreen;
    transform.rotate(90.0f, 0.0f, 1.0f, 0.0f);
    createCylinder(cylinder_radius, cylinder_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, cylinder_height);
    createCone(cone_radius, cone_height, transform, color, vertices, colors, normals);
    transform.translate(0.0f, 0.0f, -cylinder_height);
    transform.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
}

void ShapeFactory::createGridPlane(float length, float width, float x_grid_dist, float y_grid_dist, const QColor& color,
                                   std::vector<float>& vertices, std::vector<float>& colors) {
    float y_min = -width / 2;
    float y_max = width / 2;
    float x_min = -length / 2;
    float x_max = length / 2;
    float z = 0;

    if (x_grid_dist > 0) {
        float totalDist = x_max - x_min;

        if (x_grid_dist < totalDist) {
            float currentX = x_min;

            while (currentX < (x_max + (x_grid_dist / 2))) {
                vertices.push_back(currentX);
                vertices.push_back(y_min);
                vertices.push_back(z);
                vertices.push_back(currentX);
                vertices.push_back(y_max);
                vertices.push_back(z);
                currentX += x_grid_dist;
            }
        }
    }

    if (y_grid_dist > 0) {
        float totalDist = y_max - y_min;

        if (y_grid_dist < totalDist) {
            float currentY = y_min;

            while (currentY < (y_max + (y_grid_dist / 2))) {
                vertices.push_back(x_min);
                vertices.push_back(currentY);
                vertices.push_back(z);
                vertices.push_back(x_max);
                vertices.push_back(currentY);
                vertices.push_back(z);
                currentY += y_grid_dist;
            }
        }
    }

    int colorSize = vertices.size() / 3 * 4;
    colors.reserve(colorSize);
    for (int i = 0; i < colorSize; i += 4) {
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }
}

void ShapeFactory::createBuildVolumeRectangle(const QVector3D& min, const QVector3D& max, const float& x_grid_dist,
                                              const float& x_grid_offset, const float& y_grid_dist,
                                              const float& y_grid_offset, const QColor& color,
                                              std::vector<float>& vertices, std::vector<float>& colors) {
    float printer_x_min = min.x();
    float printer_x_max = max.x();
    float printer_y_min = min.y();
    float printer_y_max = max.y();
    float printer_z_min = min.z();
    float printer_z_max = max.z();

    vertices = {
        // Line from back top left to...
        // back top right
        printer_x_min,
        printer_y_min,
        printer_z_max,
        printer_x_max,
        printer_y_min,
        printer_z_max,

        // front top left
        printer_x_min,
        printer_y_min,
        printer_z_max,
        printer_x_min,
        printer_y_max,
        printer_z_max,

        // back bottom left
        printer_x_min,
        printer_y_min,
        printer_z_max,
        printer_x_min,
        printer_y_min,
        printer_z_min,

        // Line from front top right to...
        // back top right
        printer_x_max,
        printer_y_max,
        printer_z_max,
        printer_x_max,
        printer_y_min,
        printer_z_max,

        // front top left
        printer_x_max,
        printer_y_max,
        printer_z_max,
        printer_x_min,
        printer_y_max,
        printer_z_max,

        // front bottom right
        printer_x_max,
        printer_y_max,
        printer_z_max,
        printer_x_max,
        printer_y_max,
        printer_z_min,

        // Line from back bottom right to...
        // back top right
        printer_x_max,
        printer_y_min,
        printer_z_min,
        printer_x_max,
        printer_y_min,
        printer_z_max,

        // back bottom left
        printer_x_max,
        printer_y_min,
        printer_z_min,
        printer_x_min,
        printer_y_min,
        printer_z_min,

        // front bottom right
        printer_x_max,
        printer_y_min,
        printer_z_min,
        printer_x_max,
        printer_y_max,
        printer_z_min,

        // Line from front bottom left to...
        // front top left
        printer_x_min,
        printer_y_max,
        printer_z_min,
        printer_x_min,
        printer_y_max,
        printer_z_max,

        // back bottom left
        printer_x_min,
        printer_y_max,
        printer_z_min,
        printer_x_min,
        printer_y_min,
        printer_z_min,

        // front bottom right
        printer_x_min,
        printer_y_max,
        printer_z_min,
        printer_x_max,
        printer_y_max,
        printer_z_min,
    };

    if (x_grid_dist > 0) {
        float totalDist = printer_x_max - printer_x_min;

        if (x_grid_dist < totalDist) {
            float currentX = printer_x_min + x_grid_offset;

            while (currentX < printer_x_max) {
                vertices.push_back(currentX);
                vertices.push_back(printer_y_min);
                vertices.push_back(printer_z_min);
                vertices.push_back(currentX);
                vertices.push_back(printer_y_max);
                vertices.push_back(printer_z_min);
                currentX += x_grid_dist;
            }
        }
    }

    if (y_grid_dist > 0) {
        float totalDist = printer_y_max - printer_y_min;

        if (y_grid_dist < totalDist) {
            float currentY = printer_y_min + y_grid_offset;

            while (currentY < printer_y_max) {
                vertices.push_back(printer_x_min);
                vertices.push_back(currentY);
                vertices.push_back(printer_z_min);
                vertices.push_back(printer_x_max);
                vertices.push_back(currentY);
                vertices.push_back(printer_z_min);
                currentY += y_grid_dist;
            }
        }
    }

    int colorSize = vertices.size() / 3 * 4;
    colors.reserve(colorSize);
    for (int i = 0; i < colorSize; i += 4) {
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }
}

void ShapeFactory::createBuildVolumeCylinder(float radius, float height, float x_grid_dist, float y_grid_dist,
                                             const QColor& color, std::vector<float>& vertices,
                                             std::vector<float>& colors) {
    unsigned int segments = 100; // Number of arc segments used to approximate a cylinder
    float theta = 0.0f;          // Measure of angle at which each segment starts; first theta is just 0
    float thetaIncrement = 2.0f * M_PI / float(segments);
    int verticalIncrement = 6;

    vertices.reserve(segments * 6 + verticalIncrement * 6);
    std::vector<float> heights {0.0f, height};

    // draw two circles
    for (float m_h : heights) {
        theta = 0.0f;

        for (int i = 0; i < segments; ++i) {
            vertices.push_back(radius * qCos(theta));
            vertices.push_back(radius * qSin(theta));
            vertices.push_back(m_h);

            theta += thetaIncrement;

            vertices.push_back(radius * qCos(theta));
            vertices.push_back(radius * qSin(theta));
            vertices.push_back(m_h);
        }
    }

    // draw vertical lines
    theta = 0.0;
    float verticalStep = 2.0f * M_PI / float(verticalIncrement);
    for (int i = 0; i < verticalIncrement; ++i) {
        float x = radius * qCos(theta);
        float y = radius * qSin(theta);

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(0.0f);

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(height);

        theta += verticalStep;
    }

    float r_squared = radius * radius;
    float diameter = 2.0 * radius;
    if (x_grid_dist > 0) {
        float x = -radius;

        while (x < diameter) {
            float x_squared = x * x;
            float y = qSqrt(r_squared - x_squared);

            if (std::isnan(y)) {
                x += x_grid_dist;
                continue;
            }

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(0.0f);

            vertices.push_back(x);
            vertices.push_back(-y);
            vertices.push_back(0.0f);

            x += x_grid_dist;
        }
    }

    if (y_grid_dist > 0) {
        float y = -radius;

        while (y < diameter) {
            float y_squared = y * y;
            float x = qSqrt(r_squared - y_squared);

            if (std::isnan(x)) {
                y += y_grid_dist;
                continue;
            }

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(0.0f);

            vertices.push_back(-x);
            vertices.push_back(y);
            vertices.push_back(0.0f);

            y += y_grid_dist;
        }
    }

    // set colors for all vertices
    int colorSize = vertices.size() / 3 * 4;
    colors.reserve(colorSize);
    for (int i = 0; i < colorSize; i += 4) {
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }
}

void ShapeFactory::createBuildVolumeToroidal(float outerRadius, float innerRadius, float x_grid_dist, float y_grid_dist,
                                             float height, const QColor& color, std::vector<float>& vertices,
                                             std::vector<float>& colors) {
    unsigned int segments = 100; // Number of arc segments used to approximate a cylinder
    float theta = 0.0f;          // Measure of angle at which each segment starts; first theta is just 0
    float thetaIncrement = 2.0f * M_PI / float(segments);
    int verticalIncrement = 6;

    vertices.reserve(segments * 6 + verticalIncrement * 6);
    std::vector<float> heights {0.0f, height};

    // draw four circles
    for (float m_h : heights) {
        theta = 0.0f;

        for (int i = 0; i < segments; ++i) {
            float angleX = qCos(theta);
            float angleY = qSin(theta);

            vertices.push_back(outerRadius * angleX);
            vertices.push_back(outerRadius * angleY);
            vertices.push_back(m_h);

            float innerX = innerRadius * angleX;
            float innerY = innerRadius * angleY;

            theta += thetaIncrement;

            vertices.push_back(outerRadius * qCos(theta));
            vertices.push_back(outerRadius * qSin(theta));
            vertices.push_back(m_h);

            vertices.push_back(innerX);
            vertices.push_back(innerY);
            vertices.push_back(m_h);

            vertices.push_back(innerRadius * qCos(theta));
            vertices.push_back(innerRadius * qSin(theta));
            vertices.push_back(m_h);
        }
    }

    // draw vertical lines
    theta = 0.0;
    float verticalStep = 2.0f * M_PI / float(verticalIncrement);
    for (int i = 0; i < verticalIncrement; ++i) {
        float angleX = qCos(theta);
        float angleY = qSin(theta);

        vertices.push_back(outerRadius * angleX);
        vertices.push_back(outerRadius * angleY);
        vertices.push_back(0.0f);

        vertices.push_back(outerRadius * angleX);
        vertices.push_back(outerRadius * angleY);
        vertices.push_back(height);

        vertices.push_back(innerRadius * angleX);
        vertices.push_back(innerRadius * angleY);
        vertices.push_back(0.0f);

        vertices.push_back(innerRadius * angleX);
        vertices.push_back(innerRadius * angleY);
        vertices.push_back(height);

        theta += verticalStep;
    }

    float r_outer_squared = outerRadius * outerRadius;
    float r_inner_squared = innerRadius * innerRadius;
    float diameter = 2.0 * outerRadius;

    if (x_grid_dist > 0) {
        float x = -outerRadius;

        while (x < diameter) {
            float x_squared = x * x;
            float y_outer = qSqrt(r_outer_squared - x_squared);
            float y_inner = qSqrt(r_inner_squared - x_squared);

            if (std::isnan(y_outer)) {
                x += x_grid_dist;
                continue;
            }

            vertices.push_back(x);
            vertices.push_back(-y_outer);
            vertices.push_back(0.0f);

            if (!std::isnan(y_inner)) {
                vertices.push_back(x);
                vertices.push_back(-y_inner);
                vertices.push_back(0.0f);

                vertices.push_back(x);
                vertices.push_back(y_inner);
                vertices.push_back(0.0f);
            }

            vertices.push_back(x);
            vertices.push_back(y_outer);
            vertices.push_back(0.0f);

            x += x_grid_dist;
        }
    }

    if (y_grid_dist > 0) {
        float y = -outerRadius;

        while (y < diameter) {
            float y_squared = y * y;
            float x_outer = qSqrt(r_outer_squared - y_squared);
            float x_inner = qSqrt(r_inner_squared - y_squared);

            if (std::isnan(x_outer)) {
                y += y_grid_dist;
                continue;
            }

            vertices.push_back(-x_outer);
            vertices.push_back(y);
            vertices.push_back(0.0f);

            if (!std::isnan(x_inner)) {
                vertices.push_back(-x_inner);
                vertices.push_back(y);
                vertices.push_back(0.0f);

                vertices.push_back(x_inner);
                vertices.push_back(y);
                vertices.push_back(0.0f);
            }

            vertices.push_back(x_outer);
            vertices.push_back(y);
            vertices.push_back(0.0f);

            y += y_grid_dist;
        }
    }

    // set colors for all vertices
    int colorSize = vertices.size() / 3 * 4;
    colors.reserve(colorSize);
    for (int i = 0; i < colorSize; i += 4) {
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }
}

void ShapeFactory::createArrow(QVector3D begin, QVector3D end, const QColor& color, std::vector<float>& vertices,
                               std::vector<float>& colors) {
    vertices = {// Begin to end
                begin.x(), begin.y(), begin.z(), end.x(), end.y(), end.z()};

    // Translate to end and rotate to match direction.
    QMatrix4x4 cone_tfm;
    cone_tfm.translate(end);

    QVector3D dir_vec = end - begin;
    QVector3D up_vec = QVector3D(0, 0, 1);
    QQuaternion rotation = QQuaternion::fromDirection(dir_vec, up_vec);
    cone_tfm.rotate(rotation);

    std::vector<float> tmp_norm;
    ShapeFactory::createCone(.3, 1, cone_tfm, QColor(), vertices, colors, tmp_norm);

    int colorSize = vertices.size() / 3 * 4;
    colors.reserve(colorSize);
    for (int i = 0; i < colorSize; i += 4) {
        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }
}

QMatrix4x4 ShapeFactory::computeGcodeCylinderTransform(const QVector3D& start, const QVector3D& end) {
    // Convert the start position and displacement to a transform matrix we can use in the standard method
    QMatrix4x4 transform;
    transform.translate(start);

    // Retrieve the tangent (forward) vector from start to end
    QVector3D tangent = end.normalized();

    // Retrieve the normal (up) vector from the global settings
    QVector3D normal = {GSM->getGlobal()->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorX),
                        GSM->getGlobal()->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorY),
                        GSM->getGlobal()->setting<float>(Constants::ProfileSettings::SlicingVector::kSlicingVectorZ)};
    normal.normalize();

    // Compute the right vector
    QVector3D binormal = QVector3D::crossProduct(tangent, normal);
    if (binormal.lengthSquared() < std::numeric_limits<float>::epsilon()) {
        // Up and forward are parallel or anti-parallel; choose a different up vector
        normal = QVector3D(1, 0, 0);
        binormal = QVector3D::crossProduct(tangent, normal);
    }
    binormal.normalize();

    // Recompute up vector to ensure orthogonality
    normal = QVector3D::crossProduct(binormal, tangent);

    // Build the rotation matrix
    QMatrix4x4 rotation;
    rotation.setColumn(0, QVector4D(binormal, 0));
    rotation.setColumn(1, QVector4D(normal, 0));
    rotation.setColumn(2, QVector4D(tangent, 0));
    rotation.setColumn(3, QVector4D(0, 0, 0, 1));

    // Apply the rotation to the transform
    transform *= rotation;

    return transform;
}

void ShapeFactory::createArcCylinder(const float cylinder_height, const Point& start, const Point& center,
                                     const Point& end, bool is_ccw, const QColor& color, std::vector<float>& vertices,
                                     std::vector<float>& colors, std::vector<float>& normals) {
    // Convert the start position and displacement to a transform matrix we can use in the standard method
    QMatrix4x4 transform;
    transform.setToIdentity();
    transform.translate(center.toQVector3D());

    Point a(start.x(), start.y(), center.z());
    Point c(center.x() + (center.distance(a)), center.y(), center.z());
    transform.rotate(MathUtils::CreateQuaternion((c - center).toQVector3D(), (a - center).toQVector3D()));

    if (is_ccw) {
        createArcCylinderCCW(cylinder_height, start, center, end, transform, color, vertices, colors, normals);
    }
    else {
        createArcCylinder(cylinder_height, start, center, end, transform, color, vertices, colors, normals);
    }
}

} // Namespace ORNL
