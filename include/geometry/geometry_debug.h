#pragma once

#include <QPair>
#include <QString>
#include <qcontainerfwd.h>

#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"

namespace ORNL {
namespace GeometryDebug {

using EdgeList = QVector<QPair<Point, Point>>;

/// @brief Print a single polyline via Qt's debug output as Desmos-compatible line segments.
/// @details Each line segment is printed as a separate polygon in the format "polygon((x1,y1),(x2,y2))".
/// @param polyline The polyline to print. Polylines with fewer than 2 points will not be printed.
/// @param label An optional label to prepend to the output for identification purposes.
void printDesmos(const Polyline& polyline, QString label = "");

/// @brief Print a collection of polylines via Qt's debug output as Desmos-compatible line segments.
/// @details Each polyline is printed as a separate polygon in the format "polygon((x1,y1),(x2,y2))".
/// @param polylines The collection of polylines to print.
/// @param label An optional label to prepend to the output for identification purposes.
void printDesmos(const QVector<Polyline>& polylines, QString label = "");

/// @brief Print grouped polyline geometry via Qt's debug output as Desmos-compatible line segments.
/// @details Each group of polylines is printed as a separate polygon in the format "polygon((x1,y1),(x2,y2))".
/// @param geometry The grouped polyline geometry to print.
/// @param label An optional label to prepend to the output for identification purposes.
void printDesmos(const QVector<QVector<Polyline>>& geometry, QString label = "");

/// @brief Print a polygon via Qt's debug output as Desmos-compatible line segments.
/// @details Each line segment of the polygon is printed as a separate polygon in the format "polygon((x1,y1),(x2,y2))".
/// @param polygon The polygon to print.
/// @param label An optional label to prepend to the output for identification purposes.
void printDesmos(const Polygon& polygon, QString label = "");

/// @brief Print a list of polygons via Qt's debug output as Desmos-compatible line segments.
/// @details Each line segment of each polygon is printed as a separate polygon in the format
/// "polygon((x1,y1),(x2,y2))".
/// @param polygon_list The list of polygons to print.
/// @param label An optional label to prepend to the output for identification purposes.
void printDesmos(const PolygonList& polygon_list, QString label = "");

/// @brief Print an edge list via Qt's debug output as Desmos-compatible line segments.
/// @details Each edge is printed as a separate polygon in the format "polygon((x1,y1),(x2,y2))".
/// @param edge_list The edge list to print.
/// @param label An optional label to prepend to the output for identification purposes.
void printDesmos(const EdgeList& edge_list, QString label = "");

} // namespace GeometryDebug
} // namespace ORNL
