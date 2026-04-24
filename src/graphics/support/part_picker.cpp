#include "graphics/support/part_picker.h"

#include <limits>
#include <tuple>
#include <vector>

#include <qmatrix4x4.h>
#include <qpoint.h>
#include <qvectornd.h>

#include "graphics/graphics_object.h"

namespace ORNL {

std::tuple<float, QVector3D> PartPicker::pickDistanceAndIntersection(const QMatrix4x4& projection,
                                                                     const QMatrix4x4& view, QPointF ndc_mouse_pos,
                                                                     const std::vector<Triangle>& triangles,
                                                                     bool ortho) {
    QVector3D start_pos;
    QVector3D dir;

    // First step is to get equation of the ray, which will be
    // start_pos + t*dir, where t is some number and dir is the direction
    std::tie(start_pos, dir) = getDirectionAndStart(projection, ndc_mouse_pos, view, ortho);

    auto ret = castRay(start_pos, dir, triangles);
    return std::make_tuple(std::get<0>(ret), std::get<2>(ret));
}

float PartPicker::pickDistance(const QMatrix4x4& projection, const QMatrix4x4& view, QPointF ndc_mouse_pos,
                               const std::vector<Triangle>& triangles, bool ortho) {
    // Only return the first part of the tuple, which is the distance
    return std::get<0>(pickDistanceAndTriangle(projection, view, ndc_mouse_pos, triangles, ortho));
}

Triangle PartPicker::pickTriangle(const QMatrix4x4& projection, const QMatrix4x4& view, QPointF ndc_mouse_pos,
                                  const std::vector<Triangle>& triangles) {
    // Only return the second part of the tuple, which is the triangle
    return std::get<1>(pickDistanceAndTriangle(projection, view, ndc_mouse_pos, triangles));
}

std::tuple<float, Triangle> PartPicker::pickDistanceAndTriangle(const QMatrix4x4& projection, const QMatrix4x4& view,
                                                                QPointF ndc_mouse_pos,
                                                                const std::vector<Triangle>& triangles, bool ortho) {
    QVector3D start_pos;
    QVector3D dir;

    // First step is to get equation of the ray, which will be
    // start_pos + t*dir, where t is some number and dir is the direction
    std::tie(start_pos, dir) = getDirectionAndStart(projection, ndc_mouse_pos, view, ortho);

    auto ret = castRay(start_pos, dir, triangles);
    return std::make_tuple(std::get<0>(ret), std::get<1>(ret));
}

std::tuple<float, Triangle, QVector3D>
PartPicker::pickDistanceTriangleAndIntersection(const QMatrix4x4& projection, const QMatrix4x4& view,
                                                QPointF ndc_mouse_pos, const std::vector<Triangle>& triangles,
                                                bool ortho) {
    QVector3D start_pos;
    QVector3D dir;

    // First step is to get equation of the ray, which will be
    // start_pos + t*dir, where t is some number and dir is the direction
    std::tie(start_pos, dir) = getDirectionAndStart(projection, ndc_mouse_pos, view, ortho);

    auto ret = castRay(start_pos, dir, triangles);
    return ret;
}

std::tuple<float, Triangle, QVector3D> PartPicker::castRay(const QVector3D& s, const QVector3D& v,
                                                           const std::vector<Triangle>& triangles) {
    float min_dist = std::numeric_limits<float>::infinity();

    Triangle selected_triangle;
    selected_triangle.a = QVector3D(-100, -100, -100);
    selected_triangle.b = QVector3D(100, -100, -100);
    selected_triangle.c = QVector3D(-100, 100, -100);

    for (const Triangle& tri : triangles) {
        float dist_to_triangle = findDistanceToTriangle(s, v, tri);

        if (min_dist > dist_to_triangle) {
            min_dist = dist_to_triangle;
            selected_triangle = tri;
        }
    }

    QVector3D intersect = s + min_dist * v;

    return std::make_tuple(min_dist, selected_triangle, intersect);
}

float PartPicker::findDistanceToTriangle(const QVector3D& ray_start, const QVector3D& ray_dir, Triangle triangle) {
    const float epsilon = std::numeric_limits<float>::epsilon();
    QVector3D edge1;
    QVector3D edge2;
    QVector3D p, q; // vectors with no real physical meaning, but used to solve equation
    float denominator;
    float distance = std::numeric_limits<float>::infinity();
    float u, v;          // coordinate of intersection in barycentric coordinate system, must be in interval [0,1]
    QVector3D transform; // will represent moving one of the triangle's vertices to the origin of the barycentric
                         // coordinate system

    // This is Moller-Trumbore

    // These edges should make sure the culling is done properly. The goal is for
    // the normal to point away from the inside of the part.

    // I say *should* because I didn't actually check the math, but these edges
    // work in practice.
    edge1 = triangle.b - triangle.a;
    edge2 = triangle.c - triangle.a;
    p = QVector3D::crossProduct(ray_dir, edge2);
    denominator = QVector3D::dotProduct(edge1, p);

    // If denominator is less than zero, that means the triangle faces away
    // from our ray and should be culled

    // If denominator is (close to) zero, that means the ray and triangle are parallel
    if (denominator < epsilon) {
        return distance;
    }

    transform = ray_start - triangle.a;

    // u and v are the barycentric coordinates whose sum should not
    // exceed 1 and neither of which should be negative
    u = QVector3D::dotProduct(transform, p) / denominator;
    if ((u < 0) || (u > 1)) {
        return distance;
    }

    q = QVector3D::crossProduct(transform, edge1);
    v = QVector3D::dotProduct(ray_dir, q) / denominator;
    if ((v < 0) || (u + v > 1)) {
        return distance;
    }

    // Finally, we can compute how far along the ray we intersect with the triangle
    distance = QVector3D::dotProduct(q, edge2) / denominator;

    // If distance is negative, that means the triangle that we "hit" is *behind*
    // us, and we don't care if we "hit" something behind us, so just return
    if (distance < epsilon) {
        return std::numeric_limits<float>::infinity();
    }
    return distance;
}

std::tuple<QVector3D, QVector3D> PartPicker::getDirectionAndStart(const QMatrix4x4& projection, QPointF ndc_mouse_pos,
                                                                  const QMatrix4x4& view, bool ortho) {
    QVector4D p_near_ndc = QVector4D(ndc_mouse_pos.x(), ndc_mouse_pos.y(), -1, 1); // z near = -1
    QVector4D p_far_ndc = QVector4D(ndc_mouse_pos.x(), ndc_mouse_pos.y(), 1, 1);   // z far = 1

    QVector4D p_near_h = view.inverted() * projection.inverted() * p_near_ndc;
    QVector4D p_far_h = view.inverted() * projection.inverted() * p_far_ndc;

    QVector3D p0, p1;
    if (ortho) {
        p0 = p_near_h.toVector3D();
        p1 = p_far_h.toVector3D();
    }
    else {
        p0 = p_near_h.toVector3DAffine();
        p1 = p_far_h.toVector3DAffine();
    }
    return std::make_tuple(p0, (p1 - p0).normalized());
}
} // namespace ORNL
