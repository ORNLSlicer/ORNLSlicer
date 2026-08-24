#pragma once

#include <QVector>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
struct HelicalPathBoundaryIntersection {
    Point point;
    int segment_end_index = 0;
};

namespace HelicalPathRounding {
Polyline createHelixForRevolutions(const Point& center, Distance radius, Distance start_z, Distance bead_width,
                                   HelicalPathHandedness handedness, Angle start_angle, double revolutions);

QVector<Polyline> clipAtHighestIntersection(const Polyline& helix,
                                            const QVector<HelicalPathBoundaryIntersection>& intersections,
                                            bool has_inside_points, bool has_outside_points, const Point& center,
                                            Distance radius, Distance start_z, Distance bead_width,
                                            HelicalPathHandedness handedness, Angle start_angle,
                                            HelicalPathZClipRounding rounding, Distance min_path_segment_length);
}  // namespace HelicalPathRounding
}  // namespace ORNL
