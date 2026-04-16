#pragma once

#include <tuple>
#include <vector>

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>

#include "graphics/graphics_object.h"

namespace ORNL {

/*! \brief Namespace meant to encapsulate algorithms for determining which triangles of a mesh intersect with a mouse
 * ray
 *
 *  This namespace is meant to be used with an OpenGL view. To perform its operations it is necessary to have
 * transformation matrices, so a view matrix is passed into certain methods when required.
 */
namespace PartPicker {
//! \brief Figure out how far the nearest triangle in the vector is form the mouse point AND return the intersection
std::tuple<float, QVector3D> pickDistanceAndIntersection(const QMatrix4x4& projection, const QMatrix4x4& view,
                                                         QPointF ndc_mouse_pos, const std::vector<Triangle>& triangles,
                                                         bool ortho = false);

//! \brief Returns the distance to the nearest triangle and the triangle itself
std::tuple<float, Triangle> pickDistanceAndTriangle(const QMatrix4x4& projection, const QMatrix4x4& view,
                                                    QPointF ndc_mouse_pos, const std::vector<Triangle>& triangles,
                                                    bool ortho = false);

//! \brief Returns the distance to the nearest triangle, the nearest triangle iself, and the point of intersection.
std::tuple<float, Triangle, QVector3D>
pickDistanceTriangleAndIntersection(const QMatrix4x4& projection, const QMatrix4x4& view, QPointF ndc_mouse_pos,
                                    const std::vector<Triangle>& triangles, bool ortho = false);

//! \brief Calculate distance from nearest triangle in the vector to the mouse point
float pickDistance(const QMatrix4x4& projection, const QMatrix4x4& view, QPointF ndc_mouse_pos,
                   const std::vector<Triangle>& triangles, bool ortho = false);

//! \brief Returns the closest triangle in the vector to the mouse position
Triangle pickTriangle(const QMatrix4x4& projection, const QMatrix4x4& view, QPointF ndc_mouse_pos,
                      const std::vector<Triangle>& triangles);

//! \brief Returns the distance, triangle, and intersect starting from s and in direction v.
std::tuple<float, Triangle, QVector3D> castRay(const QVector3D& s, const QVector3D& v,
                                               const std::vector<Triangle>& triangles);

/*! \brief Calculate how far along the ray the ray intersects the given triangle
 *
 * Implements Moller-Trumbore for triangle intersecting with line. See below webpage for a walkthrough.
 * https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection
 */
float findDistanceToTriangle(const QVector3D& ray_start, const QVector3D& ray_dir, Triangle triangle);

/*! Returns what direction the mouse ray is pointing and the start position of the ray
 *
 * The start position is just assumed to be the current camera position, which is derived from the view matrix
 */
std::tuple<QVector3D, QVector3D> getDirectionAndStart(const QMatrix4x4& projection, QPointF ndc_mouse_pos,
                                                      const QMatrix4x4& view, bool ortho);
} // namespace PartPicker
} // namespace ORNL
