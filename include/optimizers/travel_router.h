#pragma once

#include <QVector>
#include <functional>
#include <optional>

#include "geometry/point.h"
#include "units/unit.h"

namespace ORNL {
//! \brief Routes planar travels around previously printed perimeter strokes.
class TravelRouter {
   public:
    enum class Outcome { kClear, kDetour, kReducedClearance, kUnroutable, kNonPlanar, kTravelLimit };

    void addObstacle(const Point& start, const Point& end, Distance width);

    //! \brief Follow a completed contour from its bead to an adjacent inset, or return no supported route.
    std::optional<QVector<Point>> routeOnPerimeter(const Point& start, const Point& end, Distance inset_width,
                                                   const std::function<bool(const Point&)>& allowed = {}) const;

    //! \brief Returns interior waypoints, or an empty vector when direct travel is clear or no detour exists.
    //! \param standoff Preferred extra clearance beyond the printed bead edge.
    //! \param allowed Convex travel-envelope constraint for inserted waypoints; original endpoints are preserved.
    //! \param outcome Optional result classification, including the reason for direct-travel fallback.
    QVector<Point> route(const Point& start, const Point& end, Distance standoff = 0,
                         const std::function<bool(const Point&)>& allowed = {}, Outcome* outcome = nullptr) const;

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
