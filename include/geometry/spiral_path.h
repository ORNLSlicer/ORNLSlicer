#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <qcontainerfwd.h>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace SpiralPath {
namespace detail {
struct RayLoopIntersection {
    Point point;
    double distance;
    int segment_index;
};

inline Point pointAlongSegment(const Point& start, const Point& end, double ratio) {
    return Point(start.x() + ((end.x() - start.x()) * ratio), start.y() + ((end.y() - start.y()) * ratio),
                 start.z() + ((end.z() - start.z()) * ratio));
}

inline double cross2D(double lhs_x, double lhs_y, double rhs_x, double rhs_y) {
    return (lhs_x * rhs_y) - (lhs_y * rhs_x);
}

inline bool normalize2D(double& x, double& y) {
    const double length = std::sqrt((x * x) + (y * y));
    if (length <= std::numeric_limits<double>::epsilon()) { return false; }

    x /= length;
    y /= length;
    return true;
}

inline Point stopPointOnClosingSegment(const Polyline& line, Distance distance_before_start) {
    if (line.isEmpty()) { return Point(); }

    const Point segment_start     = line.back();
    const Point segment_end       = line.front();
    const Distance segment_length = segment_start.distance(segment_end);

    if (segment_length <= distance_before_start) { return segment_start; }

    const double ratio = (segment_length() - distance_before_start()) / segment_length();
    return pointAlongSegment(segment_start, segment_end, ratio);
}

inline bool hasSmoothClosingSegment(const Polyline& line) {
    if (line.size() < 3) { return false; }

    const Angle closing_angle = MathUtils::internalAngle(line[line.size() - 2], line.back(), line.front());
    return closing_angle >= (pi - (45 * deg));
}

inline Polyline reversePreservingStart(const Polyline& line) {
    if (line.size() < 2) { return line; }

    Polyline reversed;
    reversed.reserve(line.size());
    reversed.push_back(line.front());
    for (int i = line.size() - 1; i > 0; --i) { reversed.push_back(line[i]); }

    return reversed;
}

inline void alignOrientation(Polyline& line, bool ccw) {
    if (line.size() >= 3 && line.orientation() != ccw) { line = reversePreservingStart(line); }
}

inline void rotateToClosestPoint(Polyline& line, const Point& query_point) {
    if (line.size() < 2) { return; }

    double closest_distance = std::numeric_limits<double>::max();
    int rotation_index      = 0;
    bool insert_split_point = false;
    Point split_point;
    int insertion_index = 0;

    for (int i = 0, end = line.size(); i < end; ++i) {
        const int next_index           = (i + 1) % line.size();
        auto [closest_point, distance] = MathUtils::nearestPointOnSegment(line[i], line[next_index], query_point);

        if (distance < closest_distance) {
            closest_distance   = distance;
            insert_split_point = false;

            if (closest_point == line[i]) { rotation_index = i; }
            else if (closest_point == line[next_index]) { rotation_index = next_index; }
            else {
                rotation_index     = next_index;
                insert_split_point = true;
                split_point        = closest_point;
                insertion_index    = next_index;
            }
        }
    }

    if (insert_split_point) {
        const int previous_index = insertion_index == 0 ? line.size() - 1 : insertion_index - 1;
        const int next_index     = insertion_index % line.size();

        if (split_point == line[previous_index]) { rotation_index = previous_index; }
        else if (split_point == line[next_index]) { rotation_index = next_index; }
        else { line.insert(insertion_index, split_point); }
    }

    std::rotate(line.begin(), line.begin() + rotation_index, line.end());
}

inline void rotateToClosestExistingPoint(Polyline& line, const Point& query_point) {
    if (line.size() < 2) { return; }

    double closest_distance = std::numeric_limits<double>::max();
    int rotation_index      = 0;

    for (int i = 0, end = line.size(); i < end; ++i) {
        const double distance = line[i].distance(query_point)();
        if (distance < closest_distance) {
            closest_distance = distance;
            rotation_index   = i;
        }
    }

    std::rotate(line.begin(), line.begin() + rotation_index, line.end());
}

inline void rotateToClosestForwardExistingPoint(Polyline& line, const Point& query_point, const Point& direction_start,
                                                const Point& direction_end) {
    if (line.size() < 2) { return; }

    const double direction_x         = direction_end.x() - direction_start.x();
    const double direction_y         = direction_end.y() - direction_start.y();
    const double direction_length_sq = (direction_x * direction_x) + (direction_y * direction_y);
    if (direction_length_sq <= std::numeric_limits<double>::epsilon()) {
        rotateToClosestExistingPoint(line, query_point);
        return;
    }

    double closest_distance          = std::numeric_limits<double>::max();
    double closest_fallback_distance = std::numeric_limits<double>::max();
    int rotation_index               = 0;
    int fallback_index               = 0;
    bool found_forward_point         = false;

    for (int i = 0, end = line.size(); i < end; ++i) {
        const double candidate_x = line[i].x() - query_point.x();
        const double candidate_y = line[i].y() - query_point.y();
        const double projection  = (candidate_x * direction_x) + (candidate_y * direction_y);
        const double distance    = line[i].distance(query_point)();

        if (distance < closest_fallback_distance) {
            closest_fallback_distance = distance;
            fallback_index            = i;
        }

        if (projection >= -std::numeric_limits<double>::epsilon() && distance < closest_distance) {
            closest_distance    = distance;
            rotation_index      = i;
            found_forward_point = true;
        }
    }

    if (!found_forward_point) { rotation_index = fallback_index; }

    std::rotate(line.begin(), line.begin() + rotation_index, line.end());
}

inline bool raySegmentIntersection(const Point& ray_start, double ray_direction_x, double ray_direction_y,
                                   const Point& segment_start, const Point& segment_end, Point& intersection,
                                   double& ray_distance) {
    const double segment_x          = segment_end.x() - segment_start.x();
    const double segment_y          = segment_end.y() - segment_start.y();
    const double denom              = cross2D(ray_direction_x, ray_direction_y, segment_x, segment_y);
    const double to_segment_start_x = segment_start.x() - ray_start.x();
    const double to_segment_start_y = segment_start.y() - ray_start.y();
    constexpr double epsilon        = 1.0e-6;

    if (std::abs(denom) <= epsilon) {
        if (std::abs(cross2D(to_segment_start_x, to_segment_start_y, ray_direction_x, ray_direction_y)) > epsilon) {
            return false;
        }

        const double segment_start_projection =
            (to_segment_start_x * ray_direction_x) + (to_segment_start_y * ray_direction_y);
        const double segment_end_projection = ((segment_end.x() - ray_start.x()) * ray_direction_x) +
                                              ((segment_end.y() - ray_start.y()) * ray_direction_y);

        if (segment_start_projection < -epsilon && segment_end_projection < -epsilon) { return false; }

        const bool use_segment_start =
            segment_start_projection >= -epsilon &&
            (segment_start_projection <= segment_end_projection || segment_end_projection < -epsilon);
        ray_distance = std::max(0.0, use_segment_start ? segment_start_projection : segment_end_projection);
        intersection = use_segment_start ? segment_start : segment_end;
        return true;
    }

    double t = cross2D(to_segment_start_x, to_segment_start_y, segment_x, segment_y) / denom;
    double u = cross2D(to_segment_start_x, to_segment_start_y, ray_direction_x, ray_direction_y) / denom;

    if (t < -epsilon || u < -epsilon || u > 1.0 + epsilon) { return false; }

    t = std::max(0.0, t);
    u = std::clamp(u, 0.0, 1.0);

    ray_distance = t;
    intersection = pointAlongSegment(segment_start, segment_end, u);
    return true;
}

inline bool findRayLoopIntersection(const Polyline& line, const Point& ray_start, double ray_direction_x,
                                    double ray_direction_y, double max_distance, RayLoopIntersection& intersection) {
    bool found              = false;
    double closest_distance = std::numeric_limits<double>::max();

    for (int i = 0, end = line.size(); i < end; ++i) {
        const int next_index = (i + 1) % line.size();
        Point candidate_point;
        double candidate_distance = 0.0;
        if (!raySegmentIntersection(ray_start, ray_direction_x, ray_direction_y, line[i], line[next_index],
                                    candidate_point, candidate_distance)) {
            continue;
        }

        if (candidate_distance <= max_distance + 1.0e-6 && candidate_distance < closest_distance) {
            closest_distance = candidate_distance;
            intersection     = {candidate_point, candidate_distance, i};
            found            = true;
        }
    }

    return found;
}

inline void rotateToSegmentPoint(Polyline& line, int segment_index, const Point& point) {
    if (line.size() < 2) { return; }

    const int next_index = (segment_index + 1) % line.size();
    int rotation_index   = next_index;

    if (point == line[segment_index]) { rotation_index = segment_index; }
    else if (point == line[next_index]) { rotation_index = next_index; }
    else { line.insert(next_index, point); }

    std::rotate(line.begin(), line.begin() + rotation_index, line.end());
}

inline bool rotateToFortyFiveDegreeConnector(const Polyline& current_loop, Polyline& next_loop,
                                             const Point& connector_start, Distance stop_distance) {
    if (current_loop.size() < 2 || next_loop.size() < 2) { return false; }

    double tangent_x = current_loop.front().x() - current_loop.back().x();
    double tangent_y = current_loop.front().y() - current_loop.back().y();
    if (!normalize2D(tangent_x, tangent_y)) { return false; }

    const double normal_x             = -tangent_y;
    const double normal_y             = tangent_x;
    constexpr double forty_five_scale = 0.7071067811865476;
    const double max_connector_length = std::max(stop_distance() * 2.0, 1.0e-6);
    const std::array<std::array<double, 2>, 2> directions {{
        {{(tangent_x + normal_x) * forty_five_scale, (tangent_y + normal_y) * forty_five_scale}},
        {{(tangent_x - normal_x) * forty_five_scale, (tangent_y - normal_y) * forty_five_scale}},
    }};

    bool found = false;
    RayLoopIntersection best {Point(), std::numeric_limits<double>::max(), 0};
    for (const std::array<double, 2>& direction : directions) {
        RayLoopIntersection candidate {Point(), std::numeric_limits<double>::max(), 0};
        if (findRayLoopIntersection(next_loop, connector_start, direction[0], direction[1], max_connector_length,
                                    candidate) &&
            candidate.distance < best.distance) {
            best  = candidate;
            found = true;
        }
    }

    if (!found) { return false; }

    rotateToSegmentPoint(next_loop, best.segment_index, best.point);
    return true;
}

inline Distance validWidthOrFallback(Distance width, Distance fallback_width) {
    return width > 0 ? width : fallback_width;
}

inline Distance transitionDistance(Distance current_width, Distance next_width, Distance fallback_width) {
    return (validWidthOrFallback(current_width, fallback_width) + validWidthOrFallback(next_width, fallback_width)) / 2;
}

inline Point transitionPoint(const Polyline& line, Distance distance_before_start, bool complete_before_connecting) {
    if (complete_before_connecting && !line.isEmpty()) { return line.front(); }

    return stopPointOnClosingSegment(line, distance_before_start);
}

inline bool transitionDirection(const Polyline& line, Distance distance_before_start, bool complete_before_connecting,
                                Point& transition, double& direction_x, double& direction_y) {
    if (line.size() < 2) { return false; }

    transition = transitionPoint(line, distance_before_start, complete_before_connecting);
    const Point& direction_start =
        !complete_before_connecting && transition == line.back() ? line[line.size() - 2] : line.back();
    direction_x = transition.x() - direction_start.x();
    direction_y = transition.y() - direction_start.y();
    return normalize2D(direction_x, direction_y);
}

inline Point prepareConnector(const Polyline& current_loop, Polyline& next_loop, Distance stop_distance,
                              bool complete_before_connecting) {
    const Point rough_connector_start = transitionPoint(current_loop, stop_distance, complete_before_connecting);
    const bool smooth_closing_segment = hasSmoothClosingSegment(current_loop);
    if (next_loop.size() >= 3) {
        if (complete_before_connecting) {
            if (!rotateToFortyFiveDegreeConnector(current_loop, next_loop, rough_connector_start, stop_distance)) {
                rotateToClosestPoint(next_loop, rough_connector_start);
            }
        }
        else if (smooth_closing_segment) {
            rotateToClosestForwardExistingPoint(next_loop, rough_connector_start, current_loop.back(),
                                                current_loop.front());
        }
        else { rotateToClosestPoint(next_loop, rough_connector_start); }
    }

    if (complete_before_connecting || smooth_closing_segment || next_loop.isEmpty()) { return rough_connector_start; }

    return MathUtils::nearestPointOnSegment(current_loop.back(), current_loop.front(), next_loop.front()).first;
}

struct StitchLoop {
    Polyline loop;
    Distance width;
    bool complete_before_connecting = false;
};

inline bool closedPolylineContainsAnyPoint(Polyline container, const Polyline& points) {
    if (container.size() < 3 || points.isEmpty()) { return false; }

    if (container.front() != container.back()) { container.push_back(container.front()); }

    for (const Point& point : points) {
        if (container.inside(point, true)) { return true; }
    }

    return false;
}

inline bool loopsAreNested(const Polyline& lhs, const Polyline& rhs) {
    if (lhs.size() < 3 || rhs.size() < 3) { return false; }

    return closedPolylineContainsAnyPoint(lhs, rhs) || closedPolylineContainsAnyPoint(rhs, lhs);
}

inline bool canConnectLoops(const StitchLoop& current_loop, const StitchLoop& next_loop, const Point& connector_start,
                            Distance stop_distance) {
    if (!loopsAreNested(current_loop.loop, next_loop.loop)) { return false; }

    const Distance connector_length   = connector_start.distance(next_loop.loop.front());
    const double max_connector_length = std::max(stop_distance() * 2.0, 1.0e-6);
    return connector_length() <= max_connector_length;
}
}  // namespace detail

/*!
 * \brief Returns the length of a polyline treated as a closed loop.
 */
inline Distance closedPolylineLength(const Polyline& line) {
    if (line.size() < 2) { return 0; }

    return line.length() + line.back().distance(line.front());
}

/*!
 * \brief Returns the point where a spiral transition should begin.
 */
inline Point transitionStartPoint(const Polyline& line, Distance distance_before_start,
                                  bool complete_before_connecting = false) {
    return detail::transitionPoint(line, distance_before_start, complete_before_connecting);
}

/*!
 * \brief Rotates a closed loop so its post-wipe branch continues forward to the next loop's start.
 * \return Whether a forward branch seam was found.
 */
inline bool rotateToForwardBranchSeam(Polyline& line, const Point& next_start, Distance stop_distance,
                                      Distance forward_wipe_distance, bool complete_before_connecting = false,
                                      Distance min_segment_length = 0) {
    if (line.size() < 3) { return false; }

    constexpr double minimum_forward_dot     = 1.0e-6;
    constexpr double target_forward_dot      = 0.7071067811865476;
    constexpr double angled_dot_tolerance    = 0.1;
    constexpr double max_angled_length_ratio = 1.5;

    struct SeamCandidate {
        int segment_index;
        Point point;
    };

    // Score lightweight seam locations and rotate the loop only once after selecting the best candidate.
    SeamCandidate nearest_candidate {0, line.front()};
    SeamCandidate angled_candidate {0, line.front()};
    bool found_nearest           = false;
    bool found_angled            = false;
    double nearest_branch_length = std::numeric_limits<double>::max();
    double nearest_forward_dot   = 0.0;
    double angled_branch_length  = std::numeric_limits<double>::max();
    double angled_dot_error      = std::numeric_limits<double>::max();

    auto branchQuality = [&](const SeamCandidate& candidate, double& forward_dot, double& branch_length) {
        const int point_count       = line.size();
        const bool at_segment_start = candidate.point == line[candidate.segment_index];
        const int previous_index =
            at_segment_start ? (candidate.segment_index + point_count - 1) % point_count : candidate.segment_index;
        const int before_previous_index = (previous_index + point_count - 1) % point_count;
        const Point& previous           = line[previous_index];

        Point transition;
        double direction_x          = 0.0;
        double direction_y          = 0.0;
        const double closing_length = previous.distance(candidate.point)();
        if (complete_before_connecting) {
            transition  = candidate.point;
            direction_x = transition.x() - previous.x();
            direction_y = transition.y() - previous.y();
        }
        else if (closing_length <= stop_distance()) {
            transition                   = previous;
            const Point& before_previous = line[before_previous_index];
            direction_x                  = transition.x() - before_previous.x();
            direction_y                  = transition.y() - before_previous.y();
        }
        else {
            transition  = detail::pointAlongSegment(previous, candidate.point,
                                                    (closing_length - stop_distance()) / closing_length);
            direction_x = transition.x() - previous.x();
            direction_y = transition.y() - previous.y();
        }
        if (!detail::normalize2D(direction_x, direction_y)) { return false; }

        const Point branch_start(transition.x() + (direction_x * forward_wipe_distance()),
                                 transition.y() + (direction_y * forward_wipe_distance()), transition.z());
        const double branch_x = next_start.x() - branch_start.x();
        const double branch_y = next_start.y() - branch_start.y();
        branch_length         = std::hypot(branch_x, branch_y);
        if (branch_length <= std::numeric_limits<double>::epsilon()) { return false; }

        forward_dot = ((direction_x * branch_x) + (direction_y * branch_y)) / branch_length;
        return forward_dot > minimum_forward_dot;
    };

    auto considerCandidate = [&](const SeamCandidate& candidate) {
        double forward_dot   = 0.0;
        double branch_length = 0.0;
        if (!branchQuality(candidate, forward_dot, branch_length)) { return; }

        if (branch_length < nearest_branch_length ||
            (std::abs(branch_length - nearest_branch_length) <= minimum_forward_dot &&
             forward_dot > nearest_forward_dot)) {
            nearest_candidate     = candidate;
            found_nearest         = true;
            nearest_branch_length = branch_length;
            nearest_forward_dot   = forward_dot;
        }

        const double dot_error = std::abs(forward_dot - target_forward_dot);
        if (dot_error <= angled_dot_tolerance &&
            (branch_length < angled_branch_length ||
             (std::abs(branch_length - angled_branch_length) <= minimum_forward_dot && dot_error < angled_dot_error))) {
            angled_candidate     = candidate;
            found_angled         = true;
            angled_branch_length = branch_length;
            angled_dot_error     = dot_error;
        }
    };

    considerCandidate({0, line.front()});

    const double stop_offset = complete_before_connecting ? 0.0 : stop_distance();
    const double extension   = forward_wipe_distance() - stop_offset;
    const double min_length  = std::max(0.0, min_segment_length());

    for (int segment_index = 0, end = line.size(); segment_index < end; ++segment_index) {
        const Point& segment_start  = line[segment_index];
        const Point& segment_end    = line[(segment_index + 1) % end];
        double direction_x          = segment_end.x() - segment_start.x();
        double direction_y          = segment_end.y() - segment_start.y();
        const double segment_length = std::hypot(direction_x, direction_y);
        if (segment_length <= std::numeric_limits<double>::epsilon()) { continue; }
        direction_x /= segment_length;
        direction_y /= segment_length;

        const double target_x      = next_start.x() - segment_start.x();
        const double target_y      = next_start.y() - segment_start.y();
        const double target_along  = (target_x * direction_x) + (target_y * direction_y);
        const double target_across = std::abs((target_x * direction_y) - (target_y * direction_x));
        const double min_before    = stop_offset + min_length;
        const double max_before    = segment_length - min_length;

        auto considerSegmentSeam = [&](double desired_forward_distance) {
            const double seam_distance = target_along - extension - desired_forward_distance;
            if (seam_distance <= min_before || seam_distance >= max_before) { return; }

            const Point seam = detail::pointAlongSegment(segment_start, segment_end, seam_distance / segment_length);
            considerCandidate({segment_index, seam});
        };

        considerSegmentSeam(std::max(target_across * 1.0e-4, 1.0e-3));
        if (target_across > minimum_forward_dot) { considerSegmentSeam(target_across); }
    }

    for (int rotation_index = 1, end = line.size(); rotation_index < end; ++rotation_index) {
        considerCandidate({rotation_index, line[rotation_index]});
    }

    if (!found_nearest) { return false; }

    const bool use_angled_candidate =
        found_angled && angled_branch_length <= (nearest_branch_length * max_angled_length_ratio) + minimum_forward_dot;
    const SeamCandidate& selected = use_angled_candidate ? angled_candidate : nearest_candidate;
    detail::rotateToSegmentPoint(line, selected.segment_index, selected.point);
    return true;
}

/*!
 * \brief Replaces a non-angled post-wipe fallback by rotating the receiving loop to a local 45-degree intersection.
 * \return Whether the existing branch was already angled or the receiving seam was successfully adjusted.
 */
inline bool angleForwardBranchConnection(const Polyline& line, Polyline& next_loop, Distance stop_distance,
                                         Distance forward_wipe_distance, bool complete_before_connecting = false,
                                         Distance min_segment_length = 0) {
    if (line.size() < 3 || next_loop.size() < 3) { return false; }

    constexpr double minimum_forward_dot     = 1.0e-6;
    constexpr double target_forward_dot      = 0.7071067811865476;
    constexpr double angled_dot_tolerance    = 0.1;
    constexpr double max_angled_length_ratio = 1.5;

    Point transition;
    double direction_x = 0.0;
    double direction_y = 0.0;
    if (!detail::transitionDirection(line, stop_distance, complete_before_connecting, transition, direction_x,
                                     direction_y)) {
        return false;
    }
    const Point branch_start(transition.x() + (direction_x * forward_wipe_distance()),
                             transition.y() + (direction_y * forward_wipe_distance()), transition.z());
    const double min_segment_length_value = std::max(0.0, min_segment_length());

    auto branchQuality = [&](const Polyline& candidate, double& forward_dot, double& approach_dot,
                             double& branch_length) {
        if (candidate.size() < 2) { return false; }

        const double branch_x = candidate.front().x() - branch_start.x();
        const double branch_y = candidate.front().y() - branch_start.y();
        branch_length         = std::hypot(branch_x, branch_y);
        if (branch_length <= std::numeric_limits<double>::epsilon()) { return false; }

        double approach_x = candidate[1].x() - candidate.front().x();
        double approach_y = candidate[1].y() - candidate.front().y();
        if (!detail::normalize2D(approach_x, approach_y)) { return false; }

        forward_dot  = ((direction_x * branch_x) + (direction_y * branch_y)) / branch_length;
        approach_dot = ((approach_x * branch_x) + (approach_y * branch_y)) / branch_length;
        return true;
    };

    double existing_forward_dot  = 0.0;
    double existing_approach_dot = 0.0;
    double existing_length       = 0.0;
    if (!branchQuality(next_loop, existing_forward_dot, existing_approach_dot, existing_length)) { return false; }

    const double existing_dot_error = std::max(std::abs(existing_forward_dot - target_forward_dot),
                                               std::abs(existing_approach_dot - target_forward_dot));
    if (existing_length >= min_segment_length_value && existing_dot_error <= angled_dot_tolerance) { return true; }

    const double max_connector_length =
        std::min(std::max(stop_distance() * 2.0, 1.0e-6), existing_length * max_angled_length_ratio);
    const double normal_x = -direction_y;
    const double normal_y = direction_x;
    const std::array<std::array<double, 2>, 2> directions {{
        {{(direction_x + normal_x) * target_forward_dot, (direction_y + normal_y) * target_forward_dot}},
        {{(direction_x - normal_x) * target_forward_dot, (direction_y - normal_y) * target_forward_dot}},
    }};

    Polyline best_candidate;
    double best_length    = std::numeric_limits<double>::max();
    double best_dot_error = std::numeric_limits<double>::max();

    auto considerSeam = [&](int segment_index, const Point& seam) {
        const Point& segment_start = next_loop[segment_index];
        const Point& segment_end   = next_loop[(segment_index + 1) % next_loop.size()];
        if (seam != segment_start && seam != segment_end &&
            (segment_start.distance(seam)() < min_segment_length_value ||
             seam.distance(segment_end)() < min_segment_length_value)) {
            return;
        }

        Polyline candidate = next_loop;
        detail::rotateToSegmentPoint(candidate, segment_index, seam);
        double forward_dot = 0.0, approach_dot = 0.0, branch_length = 0.0;
        if (!branchQuality(candidate, forward_dot, approach_dot, branch_length) || forward_dot <= minimum_forward_dot ||
            branch_length < min_segment_length_value || branch_length > max_connector_length + minimum_forward_dot) {
            return;
        }

        const double dot_error =
            std::max(std::abs(forward_dot - target_forward_dot), std::abs(approach_dot - target_forward_dot));
        if (dot_error <= angled_dot_tolerance &&
            (dot_error < best_dot_error ||
             (std::abs(dot_error - best_dot_error) <= minimum_forward_dot && branch_length < best_length))) {
            best_candidate = candidate;
            best_length    = branch_length;
            best_dot_error = dot_error;
        }
    };

    for (int segment_index = 0, end = next_loop.size(); segment_index < end; ++segment_index) {
        const Point& segment_start = next_loop[segment_index];
        const Point& segment_end   = next_loop[(segment_index + 1) % end];
        for (const std::array<double, 2>& candidate_direction : directions) {
            Point intersection;
            double ray_distance = 0.0;
            if (detail::raySegmentIntersection(branch_start, candidate_direction[0], candidate_direction[1],
                                               segment_start, segment_end, intersection, ray_distance) &&
                ray_distance <= max_connector_length + minimum_forward_dot) {
                considerSeam(segment_index, intersection);
            }
        }

        const double segment_length  = segment_start.distance(segment_end)();
        const double endpoint_offset = std::max(min_segment_length_value, 1.0e-3);
        if (segment_length > endpoint_offset * 2.0) {
            considerSeam(segment_index,
                         detail::pointAlongSegment(segment_start, segment_end, endpoint_offset / segment_length));
            considerSeam(segment_index, detail::pointAlongSegment(segment_start, segment_end,
                                                                  1.0 - (endpoint_offset / segment_length)));
        }
    }

    if (best_candidate.isEmpty()) { return false; }
    next_loop = best_candidate;
    return true;
}

/*!
 * \brief Rotates a receiving loop to a safe, preferably 45-degree seam from an already-emitted forward wipe.
 * \return Whether the receiving loop is nested, forward of the wipe, and within the supplied connection distance.
 */
inline bool prepareForwardBranchConnection(const Polyline& current_loop, Polyline& next_loop, const Point& branch_start,
                                           const Point& direction_start, Distance max_connector_length,
                                           Distance min_segment_length = 0) {
    if (current_loop.size() < 3 || next_loop.size() < 3 || !detail::loopsAreNested(current_loop, next_loop)) {
        return false;
    }

    constexpr double minimum_forward_dot  = 1.0e-6;
    constexpr double target_forward_dot   = 0.7071067811865476;
    constexpr double angled_dot_tolerance = 0.1;

    double direction_x = branch_start.x() - direction_start.x();
    double direction_y = branch_start.y() - direction_start.y();
    if (!detail::normalize2D(direction_x, direction_y)) { return false; }

    const double max_length               = std::max(max_connector_length(), 1.0e-6);
    const double min_segment_length_value = std::max(0.0, min_segment_length());
    const double normal_x                 = -direction_y;
    const double normal_y                 = direction_x;
    const std::array<std::array<double, 2>, 2> directions {{
        {{(direction_x + normal_x) * target_forward_dot, (direction_y + normal_y) * target_forward_dot}},
        {{(direction_x - normal_x) * target_forward_dot, (direction_y - normal_y) * target_forward_dot}},
    }};

    Polyline best_candidate;
    double best_length    = std::numeric_limits<double>::max();
    double best_dot_error = std::numeric_limits<double>::max();

    auto considerSeam = [&](int segment_index, const Point& seam) {
        const Point& segment_start = next_loop[segment_index];
        const Point& segment_end   = next_loop[(segment_index + 1) % next_loop.size()];
        if (seam != segment_start && seam != segment_end &&
            (segment_start.distance(seam)() < min_segment_length_value ||
             seam.distance(segment_end)() < min_segment_length_value)) {
            return;
        }

        Polyline candidate = next_loop;
        detail::rotateToSegmentPoint(candidate, segment_index, seam);

        const double branch_x      = candidate.front().x() - branch_start.x();
        const double branch_y      = candidate.front().y() - branch_start.y();
        const double branch_length = std::hypot(branch_x, branch_y);
        if (branch_length <= std::numeric_limits<double>::epsilon() || branch_length < min_segment_length_value ||
            branch_length > max_length) {
            return;
        }

        double approach_x = candidate[1].x() - candidate.front().x();
        double approach_y = candidate[1].y() - candidate.front().y();
        if (!detail::normalize2D(approach_x, approach_y)) { return; }

        const double forward_dot  = ((direction_x * branch_x) + (direction_y * branch_y)) / branch_length;
        const double approach_dot = ((approach_x * branch_x) + (approach_y * branch_y)) / branch_length;
        if (forward_dot <= minimum_forward_dot) { return; }

        const double dot_error =
            std::max(std::abs(forward_dot - target_forward_dot), std::abs(approach_dot - target_forward_dot));
        if (dot_error <= angled_dot_tolerance &&
            (dot_error < best_dot_error ||
             (std::abs(dot_error - best_dot_error) <= minimum_forward_dot && branch_length < best_length))) {
            best_candidate = candidate;
            best_length    = branch_length;
            best_dot_error = dot_error;
        }
    };

    for (int segment_index = 0, end = next_loop.size(); segment_index < end; ++segment_index) {
        const Point& segment_start = next_loop[segment_index];
        const Point& segment_end   = next_loop[(segment_index + 1) % end];
        for (const std::array<double, 2>& candidate_direction : directions) {
            Point intersection;
            double ray_distance = 0.0;
            if (detail::raySegmentIntersection(branch_start, candidate_direction[0], candidate_direction[1],
                                               segment_start, segment_end, intersection, ray_distance) &&
                ray_distance <= max_length + minimum_forward_dot) {
                considerSeam(segment_index, intersection);
            }
        }
    }

    if (!best_candidate.isEmpty()) {
        next_loop = best_candidate;
        return true;
    }

    Polyline fallback = next_loop;
    detail::rotateToClosestForwardExistingPoint(fallback, branch_start, direction_start, branch_start);
    const double branch_x      = fallback.front().x() - branch_start.x();
    const double branch_y      = fallback.front().y() - branch_start.y();
    const double branch_length = std::hypot(branch_x, branch_y);
    const double forward_dot   = (direction_x * branch_x) + (direction_y * branch_y);
    if (forward_dot <= minimum_forward_dot || branch_length < min_segment_length_value ||
        branch_length > max_length + minimum_forward_dot) {
        return false;
    }

    next_loop = fallback;
    return true;
}

/*!
 * \brief Returns whether a post-wipe branch can safely connect two adjacent closed loops.
 *
 * The branch must move forward from the current loop, remain local to the transition width, and join nested geometry.
 */
inline bool canConnectAfterForwardWipe(const Polyline& current_loop, const Polyline& next_loop, Distance stop_distance,
                                       Distance forward_wipe_distance, bool complete_before_connecting = false,
                                       Distance min_segment_length = 0) {
    if (current_loop.size() < 3 || next_loop.size() < 3 || !detail::loopsAreNested(current_loop, next_loop)) {
        return false;
    }

    Point transition;
    double direction_x = 0.0;
    double direction_y = 0.0;
    if (!detail::transitionDirection(current_loop, stop_distance, complete_before_connecting, transition, direction_x,
                                     direction_y)) {
        return false;
    }
    const Point branch_start(transition.x() + (direction_x * forward_wipe_distance()),
                             transition.y() + (direction_y * forward_wipe_distance()), transition.z());
    const double branch_x      = next_loop.front().x() - branch_start.x();
    const double branch_y      = next_loop.front().y() - branch_start.y();
    const double branch_length = std::hypot(branch_x, branch_y);
    const double forward_dot   = (direction_x * branch_x) + (direction_y * branch_y);
    const double max_length    = std::max(stop_distance() * 2.0, 1.0e-6);
    const double min_length    = std::max(min_segment_length(), 0.0);

    return forward_dot > 1.0e-6 && branch_length >= min_length && branch_length <= max_length + 1.0e-6;
}

/*!
 * \brief Links ordered closed loops into open spiral-style polyline groups.
 *
 * Adjacent loops are joined with an extruding connector. When the candidate connector would jump between unrelated or
 * non-adjacent loops, the current group ends at the connector start so the caller can emit a travel to the next group.
 *
 * \param ordered_loops Closed loops without a repeated final point.
 * \param loop_widths Bead widths for the ordered loops.
 * \param fallback_width Width used when a per-loop width is not supplied.
 * \param complete_before_connecting Close each loop before adding the connector to the next loop.
 * \return One or more polylines, each representing a connectable spiral group.
 */
inline QVector<Polyline> linkClosedPolylineGroups(const QVector<Polyline>& ordered_loops,
                                                  const QVector<Distance>& loop_widths, Distance fallback_width,
                                                  const QVector<bool>& complete_before_connecting) {
    QVector<detail::StitchLoop> loops;
    loops.reserve(ordered_loops.size());
    for (int i = 0, end = ordered_loops.size(); i < end; ++i) {
        loops.push_back({ordered_loops[i], i < loop_widths.size() ? loop_widths[i] : fallback_width,
                         i < complete_before_connecting.size() ? complete_before_connecting[i] : false});
    }

    bool has_reference_orientation = false;
    bool reference_orientation     = false;
    for (const detail::StitchLoop& stitch_loop : loops) {
        if (stitch_loop.loop.size() >= 3) {
            reference_orientation     = stitch_loop.loop.orientation();
            has_reference_orientation = true;
            break;
        }
    }
    if (has_reference_orientation) {
        for (detail::StitchLoop& stitch_loop : loops) {
            detail::alignOrientation(stitch_loop.loop, reference_orientation);
        }
    }

    QVector<Polyline> spiral_groups;
    Polyline spiral;

    while (!loops.isEmpty() && loops.front().loop.size() < 3) { loops.removeFirst(); }

    if (loops.isEmpty()) { return spiral_groups; }

    detail::StitchLoop current_loop = loops.takeFirst();
    while (true) {
        spiral += current_loop.loop;

        while (!loops.isEmpty() && loops.front().loop.size() < 3) { loops.removeFirst(); }

        if (!loops.isEmpty()) {
            detail::StitchLoop next_loop          = loops.takeFirst();
            detail::StitchLoop prepared_next_loop = next_loop;
            const Distance stop_distance =
                detail::transitionDistance(current_loop.width, next_loop.width, fallback_width);
            const Point connector_start = detail::prepareConnector(
                current_loop.loop, prepared_next_loop.loop, stop_distance, current_loop.complete_before_connecting);

            if (detail::canConnectLoops(current_loop, prepared_next_loop, connector_start, stop_distance)) {
                if (spiral.back() != connector_start) { spiral += connector_start; }
                current_loop = prepared_next_loop;
            }
            else {
                const Point final_stop = detail::transitionPoint(
                    current_loop.loop, detail::validWidthOrFallback(current_loop.width, fallback_width),
                    current_loop.complete_before_connecting);

                if (spiral.back() != final_stop) { spiral += final_stop; }
                spiral_groups.push_back(spiral);
                spiral.clear();
                current_loop = next_loop;
            }
        }
        else {
            const Point final_stop = detail::transitionPoint(
                current_loop.loop, detail::validWidthOrFallback(current_loop.width, fallback_width),
                current_loop.complete_before_connecting);

            if (spiral.back() != final_stop) { spiral += final_stop; }
            spiral_groups.push_back(spiral);
            break;
        }
    }

    return spiral_groups;
}

inline QVector<Polyline> linkClosedPolylineGroups(const QVector<Polyline>& ordered_loops,
                                                  const QVector<Distance>& loop_widths, Distance fallback_width,
                                                  bool complete_before_connecting = false) {
    QVector<bool> completion_flags(ordered_loops.size(), complete_before_connecting);
    return linkClosedPolylineGroups(ordered_loops, loop_widths, fallback_width, completion_flags);
}

inline QVector<Polyline> linkClosedPolylineGroups(const QVector<Polyline>& ordered_loops, Distance final_stop_distance,
                                                  bool complete_before_connecting = false) {
    QVector<Distance> loop_widths;
    loop_widths.reserve(ordered_loops.size());
    for (int i = 0, end = ordered_loops.size(); i < end; ++i) { loop_widths.push_back(final_stop_distance); }

    return linkClosedPolylineGroups(ordered_loops, loop_widths, final_stop_distance, complete_before_connecting);
}

/*!
 * \brief Links ordered closed loops into one open spiral-style polyline.
 *
 * \param ordered_loops Closed loops without a repeated final point.
 * \param loop_widths Bead widths for the ordered loops.
 * \param fallback_width Width used when a per-loop width is not supplied.
 * \param complete_before_connecting Close each loop before adding the connector to the next loop.
 * \return One polyline that walks each loop and transitions to the next loop before fully closing.
 */
inline Polyline linkClosedPolylines(const QVector<Polyline>& ordered_loops, const QVector<Distance>& loop_widths,
                                    Distance fallback_width, bool complete_before_connecting = false) {
    QVector<detail::StitchLoop> loops;
    loops.reserve(ordered_loops.size());
    for (int i = 0, end = ordered_loops.size(); i < end; ++i) {
        loops.push_back({ordered_loops[i], i < loop_widths.size() ? loop_widths[i] : fallback_width});
    }

    bool has_reference_orientation = false;
    bool reference_orientation     = false;
    for (const detail::StitchLoop& stitch_loop : loops) {
        if (stitch_loop.loop.size() >= 3) {
            reference_orientation     = stitch_loop.loop.orientation();
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

    while (!loops.isEmpty() && loops.front().loop.size() < 3) { loops.removeFirst(); }

    if (loops.isEmpty()) { return spiral; }

    detail::StitchLoop current_loop = loops.takeFirst();
    while (true) {
        spiral += current_loop.loop;

        while (!loops.isEmpty() && loops.front().loop.size() < 3) { loops.removeFirst(); }

        if (!loops.isEmpty()) {
            detail::StitchLoop next_loop = loops.takeFirst();
            const Distance stop_distance =
                detail::transitionDistance(current_loop.width, next_loop.width, fallback_width);
            const Point connector_start =
                detail::prepareConnector(current_loop.loop, next_loop.loop, stop_distance, complete_before_connecting);

            if (spiral.back() != connector_start) { spiral += connector_start; }

            current_loop = next_loop;
        }
        else {
            const Point final_stop = detail::transitionPoint(
                current_loop.loop, detail::validWidthOrFallback(current_loop.width, fallback_width),
                complete_before_connecting);

            if (spiral.back() != final_stop) { spiral += final_stop; }
            break;
        }
    }

    return spiral;
}

inline Polyline linkClosedPolylines(const QVector<Polyline>& ordered_loops, Distance final_stop_distance,
                                    bool complete_before_connecting = false) {
    QVector<Polyline> loops = ordered_loops;
    Polyline spiral;

    for (int loop_index = 0, end = loops.size(); loop_index < end; ++loop_index) {
        const Polyline& loop = loops[loop_index];
        if (loop.size() < 3) { continue; }

        spiral += loop;

        if (loop_index + 1 < end) {
            Polyline next_loop = loops[loop_index + 1];
            const Point connector_start =
                detail::prepareConnector(loop, next_loop, final_stop_distance, complete_before_connecting);

            if (spiral.back() != connector_start) { spiral += connector_start; }

            loops[loop_index + 1] = next_loop;
        }
        else {
            const Point final_stop = detail::transitionPoint(loop, final_stop_distance, complete_before_connecting);

            if (spiral.back() != final_stop) { spiral += final_stop; }
        }
    }

    return spiral;
}

}  // namespace SpiralPath
}  // namespace ORNL
