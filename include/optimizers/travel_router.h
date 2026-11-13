#pragma once

#include <QVector>

#include "geometry/point.h"
#include "units/unit.h"

namespace ORNL {
//! \brief Routes planar travels around previously printed perimeter strokes.
class TravelRouter {
   public:
    void addObstacle(const Point& start, const Point& end, Distance width);

    //! \brief Returns interior waypoints, or an empty vector when direct travel is clear or no detour exists.
    //! \param standoff Preferred extra clearance beyond the printed bead edge.
    QVector<Point> route(const Point& start, const Point& end, Distance standoff = 0) const;

   private:
    struct Obstacle {
        Point start;
        Point end;
        double half_width;
        double rounding;
    };
    QVector<Obstacle> m_obstacles;
};
}  // namespace ORNL
