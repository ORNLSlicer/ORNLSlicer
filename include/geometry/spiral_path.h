#pragma once

#include <qcontainerfwd.h>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace SpiralPath {
namespace detail {
inline Point pointAlongSegment(const Point& start, const Point& end, double ratio) {
    return Point(start.x() + ((end.x() - start.x()) * ratio), start.y() + ((end.y() - start.y()) * ratio),
                 start.z() + ((end.z() - start.z()) * ratio));
}

inline Point stopPointOnClosingSegment(const Polyline& line, Distance distance_before_start) {
    if (line.isEmpty()) {
        return Point();
    }

    const Point segment_start = line.back();
    const Point segment_end = line.front();
    const Distance segment_length = segment_start.distance(segment_end);

    if (segment_length <= distance_before_start) {
        return segment_start;
    }

    const double ratio = (segment_length() - distance_before_start()) / segment_length();
    return pointAlongSegment(segment_start, segment_end, ratio);
}

inline bool hasSmoothClosingSegment(const Polyline& line) {
    if (line.size() < 3) {
        return false;
    }

    return MathUtils::nearCollinear(line[line.size() - 2], line.back(), line.front(), 45 * deg);
}
} // namespace detail

/*!
 * \brief Returns the length of a polyline treated as a closed loop.
 */
inline Distance closedPolylineLength(const Polyline& line) {
    if (line.size() < 2) {
        return 0;
    }

    return line.length() + line.back().distance(line.front());
}

/*!
 * \brief Links ordered closed loops into one open spiral-style polyline.
 *
 * \param ordered_loops Closed loops without a repeated final point.
 * \param final_stop_distance Distance to leave unprinted before the loop start when closing smoothly.
 * \return One polyline that walks each loop and transitions to the next loop before fully closing.
 */
inline Polyline linkClosedPolylines(const QVector<Polyline>& ordered_loops, Distance final_stop_distance) {
    Polyline spiral;

    for (int loop_index = 0, end = ordered_loops.size(); loop_index < end; ++loop_index) {
        const Polyline& loop = ordered_loops[loop_index];
        if (loop.size() < 3) {
            continue;
        }

        spiral += loop;

        if (loop_index + 1 < end) {
            const Point next_start = ordered_loops[loop_index + 1].front();
            Point connector_start;
            if (detail::hasSmoothClosingSegment(loop)) {
                connector_start = detail::stopPointOnClosingSegment(loop, final_stop_distance);
            }
            else {
                connector_start = MathUtils::nearestPointOnSegment(loop.back(), loop.front(), next_start).first;
            }

            if (spiral.back() != connector_start) {
                spiral += connector_start;
            }
        }
        else {
            const Point final_stop = detail::stopPointOnClosingSegment(loop, final_stop_distance);

            if (spiral.back() != final_stop) {
                spiral += final_stop;
            }
        }
    }

    return spiral;
}
} // namespace SpiralPath
} // namespace ORNL
