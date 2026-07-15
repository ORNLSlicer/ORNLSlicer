#pragma once

#include <limits>

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

inline Polyline reversePreservingStart(const Polyline& line) {
    if (line.size() < 2) {
        return line;
    }

    Polyline reversed;
    reversed.reserve(line.size());
    reversed.push_back(line.front());
    for (int i = line.size() - 1; i > 0; --i) {
        reversed.push_back(line[i]);
    }

    return reversed;
}

inline void alignOrientation(Polyline& line, bool ccw) {
    if (line.size() >= 3 && line.orientation() != ccw) {
        line = reversePreservingStart(line);
    }
}

inline void rotateToClosestPoint(Polyline& line, const Point& query_point) {
    if (line.size() < 2) {
        return;
    }

    double closest_distance = std::numeric_limits<double>::max();
    int rotation_index = 0;
    bool insert_split_point = false;
    Point split_point;
    int insertion_index = 0;

    for (int i = 0, end = line.size(); i < end; ++i) {
        const int next_index = (i + 1) % line.size();
        auto [closest_point, distance] = MathUtils::nearestPointOnSegment(line[i], line[next_index], query_point);

        if (distance < closest_distance) {
            closest_distance = distance;
            insert_split_point = false;

            if (closest_point == line[i]) {
                rotation_index = i;
            }
            else if (closest_point == line[next_index]) {
                rotation_index = next_index;
            }
            else {
                rotation_index = next_index;
                insert_split_point = true;
                split_point = closest_point;
                insertion_index = next_index;
            }
        }
    }

    if (insert_split_point) {
        const int previous_index = insertion_index == 0 ? line.size() - 1 : insertion_index - 1;
        const int next_index = insertion_index % line.size();

        if (split_point == line[previous_index]) {
            rotation_index = previous_index;
        }
        else if (split_point == line[next_index]) {
            rotation_index = next_index;
        }
        else {
            line.insert(insertion_index, split_point);
        }
    }

    for (int i = 0; i < rotation_index; ++i) {
        line.move(0, line.size() - 1);
    }
}

inline void rotateToClosestExistingPoint(Polyline& line, const Point& query_point) {
    if (line.size() < 2) {
        return;
    }

    double closest_distance = std::numeric_limits<double>::max();
    int rotation_index = 0;

    for (int i = 0, end = line.size(); i < end; ++i) {
        const double distance = line[i].distance(query_point)();
        if (distance < closest_distance) {
            closest_distance = distance;
            rotation_index = i;
        }
    }

    for (int i = 0; i < rotation_index; ++i) {
        line.move(0, line.size() - 1);
    }
}

inline void rotateToClosestForwardExistingPoint(Polyline& line, const Point& query_point, const Point& direction_start,
                                                const Point& direction_end) {
    if (line.size() < 2) {
        return;
    }

    const double direction_x = direction_end.x() - direction_start.x();
    const double direction_y = direction_end.y() - direction_start.y();
    const double direction_length_sq = (direction_x * direction_x) + (direction_y * direction_y);
    if (direction_length_sq <= std::numeric_limits<double>::epsilon()) {
        rotateToClosestExistingPoint(line, query_point);
        return;
    }

    double closest_distance = std::numeric_limits<double>::max();
    double closest_fallback_distance = std::numeric_limits<double>::max();
    int rotation_index = 0;
    int fallback_index = 0;
    bool found_forward_point = false;

    for (int i = 0, end = line.size(); i < end; ++i) {
        const double candidate_x = line[i].x() - query_point.x();
        const double candidate_y = line[i].y() - query_point.y();
        const double projection = (candidate_x * direction_x) + (candidate_y * direction_y);
        const double distance = line[i].distance(query_point)();

        if (distance < closest_fallback_distance) {
            closest_fallback_distance = distance;
            fallback_index = i;
        }

        if (projection >= -std::numeric_limits<double>::epsilon() && distance < closest_distance) {
            closest_distance = distance;
            rotation_index = i;
            found_forward_point = true;
        }
    }

    if (!found_forward_point) {
        rotation_index = fallback_index;
    }

    for (int i = 0; i < rotation_index; ++i) {
        line.move(0, line.size() - 1);
    }
}

inline Distance validWidthOrFallback(Distance width, Distance fallback_width) {
    return width > 0 ? width : fallback_width;
}

inline Distance transitionDistance(Distance current_width, Distance next_width, Distance fallback_width) {
    return (validWidthOrFallback(current_width, fallback_width) + validWidthOrFallback(next_width, fallback_width)) / 2;
}

inline Point prepareConnector(const Polyline& current_loop, Polyline& next_loop, Distance stop_distance) {
    Point connector_start = stopPointOnClosingSegment(current_loop, stop_distance);
    if (next_loop.size() >= 3) {
        rotateToClosestForwardExistingPoint(next_loop, connector_start, current_loop.back(), current_loop.front());
    }

    return connector_start;
}

struct StitchLoop {
    Polyline loop;
    Distance width;
};
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
 * \brief Returns the point on a closed loop's closing segment where a spiral transition should begin.
 */
inline Point transitionStartPoint(const Polyline& line, Distance distance_before_start) {
    return detail::stopPointOnClosingSegment(line, distance_before_start);
}

/*!
 * \brief Links ordered closed loops into one open spiral-style polyline.
 *
 * \param ordered_loops Closed loops without a repeated final point.
 * \param loop_widths Bead widths for the ordered loops.
 * \param fallback_width Width used when a per-loop width is not supplied.
 * \return One polyline that walks each loop and transitions to the next loop before fully closing.
 */
inline Polyline linkClosedPolylines(const QVector<Polyline>& ordered_loops, const QVector<Distance>& loop_widths,
                                    Distance fallback_width) {
    QVector<detail::StitchLoop> loops;
    loops.reserve(ordered_loops.size());
    for (int i = 0, end = ordered_loops.size(); i < end; ++i) {
        loops.push_back({ordered_loops[i], i < loop_widths.size() ? loop_widths[i] : fallback_width});
    }

    bool has_reference_orientation = false;
    bool reference_orientation = false;
    for (const detail::StitchLoop& stitch_loop : loops) {
        if (stitch_loop.loop.size() >= 3) {
            reference_orientation = stitch_loop.loop.orientation();
            has_reference_orientation = true;
            break;
        }
    }
    if (has_reference_orientation) {
        for (detail::StitchLoop& stitch_loop : loops) {
            detail::alignOrientation(stitch_loop.loop, reference_orientation);
        }
    }

    Polyline spiral;

    while (!loops.isEmpty() && loops.front().loop.size() < 3) {
        loops.removeFirst();
    }

    if (loops.isEmpty()) {
        return spiral;
    }

    detail::StitchLoop current_loop = loops.takeFirst();
    while (true) {
        spiral += current_loop.loop;

        while (!loops.isEmpty() && loops.front().loop.size() < 3) {
            loops.removeFirst();
        }

        if (!loops.isEmpty()) {
            detail::StitchLoop next_loop = loops.takeFirst();
            const Distance stop_distance =
                detail::transitionDistance(current_loop.width, next_loop.width, fallback_width);
            const Point connector_start = detail::prepareConnector(current_loop.loop, next_loop.loop, stop_distance);

            if (spiral.back() != connector_start) {
                spiral += connector_start;
            }

            current_loop = next_loop;
        }
        else {
            const Point final_stop = detail::stopPointOnClosingSegment(
                current_loop.loop, detail::validWidthOrFallback(current_loop.width, fallback_width));

            if (spiral.back() != final_stop) {
                spiral += final_stop;
            }
            break;
        }
    }

    return spiral;
}

inline Polyline linkClosedPolylines(const QVector<Polyline>& ordered_loops, Distance final_stop_distance) {
    QVector<Polyline> loops = ordered_loops;
    Polyline spiral;

    for (int loop_index = 0, end = loops.size(); loop_index < end; ++loop_index) {
        const Polyline& loop = loops[loop_index];
        if (loop.size() < 3) {
            continue;
        }

        spiral += loop;

        if (loop_index + 1 < end) {
            Polyline next_loop = loops[loop_index + 1];
            Point rough_connector_start = detail::stopPointOnClosingSegment(loop, final_stop_distance);
            detail::rotateToClosestForwardExistingPoint(next_loop, rough_connector_start, loop.back(), loop.front());
            const Point next_start = next_loop.front();
            Point connector_start;
            if (detail::hasSmoothClosingSegment(loop)) {
                connector_start = rough_connector_start;
            }
            else {
                connector_start = MathUtils::nearestPointOnSegment(loop.back(), loop.front(), next_start).first;
            }

            if (spiral.back() != connector_start) {
                spiral += connector_start;
            }

            loops[loop_index + 1] = next_loop;
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
