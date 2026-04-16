#pragma once

#include <qtypes.h>

#include "geometry/point.h"

namespace ORNL {
/*!
 * \class ClosePolygonResult
 * \brief The result of trying to find a point on a closed polygon line.
 * This gives back the point index, the polygon index, and the point of the
 * connection. The line on which the point lays is between point_idx - 1 and
 * point_idx.
 */
class ClosePolygonResult {
  public:
    ClosePolygonResult();

    Point intersection_point;
    int polygon_idx;
    uint point_idx;
};
} // namespace ORNL
