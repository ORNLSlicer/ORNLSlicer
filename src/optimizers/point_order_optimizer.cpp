#include "optimizers/point_order_optimizer.h"

#include <QRandomGenerator>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>
#include <optional>

#include <qcontainerfwd.h>
#include <qtypes.h>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
constexpr double kDistanceTolerance = 1.0e-6;

Distance planarDistance(const Point& lhs, const Point& rhs) {
    return Distance(std::hypot(static_cast<double>(lhs.x() - rhs.x()), static_cast<double>(lhs.y() - rhs.y())));
}

std::optional<Point> pointAtPlanarDistance(const Point& start, const Point& end, const Point& reference,
                                           Distance distance) {
    const double dx = static_cast<double>(end.x() - start.x());
    const double dy = static_cast<double>(end.y() - start.y());
    const double a  = (dx * dx) + (dy * dy);
    if (a <= kDistanceTolerance) return std::nullopt;

    const double fx           = static_cast<double>(start.x() - reference.x());
    const double fy           = static_cast<double>(start.y() - reference.y());
    const double b            = 2.0 * ((fx * dx) + (fy * dy));
    const double c            = (fx * fx) + (fy * fy) - (distance() * distance());
    const double discriminant = (b * b) - (4.0 * a * c);
    if (discriminant < -kDistanceTolerance) return std::nullopt;

    const double root    = std::sqrt(std::max(0.0, discriminant));
    double candidates[2] = {(-b - root) / (2.0 * a), (-b + root) / (2.0 * a)};
    std::sort(candidates, candidates + 2);

    for (double t : candidates) {
        if (t < -kDistanceTolerance || t > 1.0 + kDistanceTolerance) continue;

        t = std::clamp(t, 0.0, 1.0);
        return Point(start.x() + (dx * t), start.y() + (dy * t), start.z() + ((end.z() - start.z()) * t));
    }

    return std::nullopt;
}
}  // namespace

PointOrderOptimizer::PointOrderSelection PointOrderOptimizer::linkToPoint(
    const Point& current_location, const Polyline& polyline, uint layer_number,
    PointOrderOptimization pointOptimization, bool min_dist_enabled, Distance min_dist_threshold,
    Distance consecutive_dist_threshold, bool local_randomness_enable, Distance randomness_radius,
    bool allow_segment_breaking, const std::optional<Point>& consecutive_reference) {
    PointOrderSelection result;

    bool use_segment_breaking = allow_segment_breaking && polyline.size() > 1 && !min_dist_enabled &&
                                !local_randomness_enable &&
                                (pointOptimization == PointOrderOptimization::kNextClosest ||
                                 pointOptimization == PointOrderOptimization::kCustomPoint);

    if (use_segment_breaking) {
        result = findClosestPointOnClosedLoop(polyline, current_location);
        return result;
    }

    switch (pointOptimization) {
        case PointOrderOptimization::kNextClosest:
            result = selectionFromIndex(
                findShortestOrLongestDistance(polyline, current_location, min_dist_enabled, min_dist_threshold));
            break;
        case PointOrderOptimization::kNextFarthest:
            result = selectionFromIndex(
                findShortestOrLongestDistance(polyline, current_location, min_dist_enabled, min_dist_threshold, false));
            break;
        case PointOrderOptimization::kRandom:
            result = selectionFromIndex(linkToRandom(polyline));
            break;
        case PointOrderOptimization::kConsecutive:
            result = linkToConsecutive(polyline, layer_number, consecutive_dist_threshold, consecutive_reference);
            break;
        case PointOrderOptimization::kCustomPoint:
            result = selectionFromIndex(findShortestOrLongestDistance(polyline, current_location, false, Distance(0)));
            break;
        case PointOrderOptimization::kCustomFarthestPoint:
            result = selectionFromIndex(
                findShortestOrLongestDistance(polyline, current_location, false, Distance(0), false));
            break;
        default:
            result = selectionFromIndex(findShortestOrLongestDistance(polyline, current_location, false, Distance(0)));
            break;
    }

    if (local_randomness_enable && !polyline.isEmpty()) {
        const Point random_origin  = result.insert_split_point ? result.split_point : polyline[result.rotation_index];
        const int randomized_index = computePerturbation(polyline, random_origin, randomness_radius);
        if (randomized_index >= 0) result = selectionFromIndex(randomized_index);
    }

    return result;
}

bool PointOrderOptimizer::findSkeletonPointOrder(const Point& current_location, const Polyline& polyline,
                                                 PointOrderOptimization pointOptimization, bool min_dist_enabled,
                                                 Distance min_dist_threshold) {
    bool result = false;

    switch (pointOptimization) {
        case PointOrderOptimization::kNextClosest:
            if (findClosestEnd(polyline, current_location, min_dist_enabled, min_dist_threshold) == 0)
                return false;
            else
                return true;
            break;
        case PointOrderOptimization::kNextFarthest:
        case PointOrderOptimization::kCustomFarthestPoint:
            if (findClosestEnd(polyline, current_location, min_dist_enabled, min_dist_threshold) == 0)
                return true;
            else
                return false;
            break;
        case PointOrderOptimization::kRandom:
            if (QRandomGenerator::global()->generate() % 2) result = true;
            break;
        case PointOrderOptimization::kCustomPoint:
            if (findClosestEnd(polyline, current_location, min_dist_enabled, min_dist_threshold) == 0)
                return false;
            else
                return true;
            break;
        default:
            result = false;
            break;
    }

    return result;
}

int PointOrderOptimizer::findShortestOrLongestDistance(const Polyline& polyline, const Point& startPoint,
                                                       bool minThresholdEnable, Distance minThreshold, bool shortest) {
    int pointIndex = -1;
    Distance closest;
    if (shortest) closest = Distance(DBL_MAX);
    if (minThresholdEnable) closest = Distance(minThreshold);

    for (int i = 0, end = polyline.size(); i < end; ++i) {
        Distance dist = polyline[i].distance(startPoint);
        if (shortest) {
            if (dist < closest) {
                closest    = dist;
                pointIndex = i;
            }
        }
        else {
            if (dist > closest) {
                closest    = dist;
                pointIndex = i;
            }
        }
    }

    // if no candidates found it's because nothing met threshold, so find farthest point
    if (pointIndex == -1) pointIndex = findShortestOrLongestDistance(polyline, startPoint, false, Distance(0), false);

    return pointIndex;
}

PointOrderOptimizer::PointOrderSelection PointOrderOptimizer::findClosestPointOnClosedLoop(const Polyline& polyline,
                                                                                           const Point& queryPoint) {
    PointOrderSelection result;
    if (polyline.isEmpty()) return result;

    double closest_distance = std::numeric_limits<double>::max();

    for (int i = 0, end = polyline.size(); i < end; ++i) {
        int next_index = (i + 1) % polyline.size();
        auto [closest_point, distance] =
            MathUtils::nearestPointOnSegment(polyline[i], polyline[next_index], queryPoint);

        if (distance < closest_distance) {
            closest_distance = distance;

            if (closest_point == polyline[i]) { result = selectionFromIndex(i); }
            else if (closest_point == polyline[next_index]) { result = selectionFromIndex(next_index); }
            else {
                result.rotation_index     = next_index;
                result.insert_split_point = true;
                result.split_point        = closest_point;
                result.insertion_index    = next_index;
            }
        }
    }

    return result;
}

PointOrderOptimizer::PointOrderSelection PointOrderOptimizer::selectionFromIndex(int pointIndex) {
    PointOrderSelection result;
    result.rotation_index = std::max(pointIndex, 0);
    return result;
}

int PointOrderOptimizer::findClosestEnd(const Polyline& polyline, const Point& currentPoint, bool minThresholdEnable,
                                        Distance minThreshold) {
    // Check which end is closer. Return 0 for front and non-zero for back.
    // Both could be closer than the minimum distance, but ultimately one has to be chosen.
    // If the front is closer, but less than the minimum distance, use the back.
    // If the back is closer, but less than the minimum distance, use the front.

    if (currentPoint.distance(polyline.front()) < currentPoint.distance(polyline.back())) {
        if (minThresholdEnable && currentPoint.distance(polyline.front()) < minThreshold)
            return polyline.size();
        else
            return 0;
    }
    else {
        if (minThresholdEnable && currentPoint.distance(polyline.back()) < minThreshold)
            return 0;
        else
            return polyline.size();
    }
}

int PointOrderOptimizer::linkToRandom(const Polyline& polyline) {
    return QRandomGenerator::global()->bounded(polyline.size());
}

PointOrderOptimizer::PointOrderSelection PointOrderOptimizer::linkToConsecutive(
    const Polyline& polyline, uint layer_number, Distance minDist, const std::optional<Point>& previous_start) {
    if (polyline.isEmpty()) return PointOrderSelection();

    if (polyline.size() == 1) return selectionFromIndex(0);

    if (!previous_start.has_value() || minDist <= 0)
        return selectionFromIndex(linkToConsecutiveByIndex(polyline, layer_number, minDist));

    Point reference(previous_start->x(), previous_start->y(), polyline.front().z());
    PointOrderSelection nearest_selection = findClosestPointOnClosedLoop(polyline, reference);

    Point segment_start;
    int segment_end_index;
    if (nearest_selection.insert_split_point) {
        segment_start     = nearest_selection.split_point;
        segment_end_index = nearest_selection.insertion_index;
    }
    else {
        const int start_index = nearest_selection.rotation_index % polyline.size();
        segment_start         = polyline[start_index];
        segment_end_index     = (start_index + 1) % polyline.size();
    }

    if (planarDistance(segment_start, reference) >= minDist) return nearest_selection;

    Distance farthest_distance = Distance(-1.0);
    int farthest_index         = 0;

    for (int segment_count = 0; segment_count < polyline.size(); ++segment_count) {
        const Point& segment_end    = polyline[segment_end_index];
        const Distance end_distance = planarDistance(segment_end, reference);

        if (end_distance > farthest_distance) {
            farthest_distance = end_distance;
            farthest_index    = segment_end_index;
        }

        if (end_distance >= minDist) {
            std::optional<Point> split_point = pointAtPlanarDistance(segment_start, segment_end, reference, minDist);
            if (split_point.has_value() && *split_point != segment_start && *split_point != segment_end) {
                PointOrderSelection selection;
                selection.rotation_index     = segment_end_index;
                selection.insert_split_point = true;
                selection.split_point        = *split_point;
                selection.insertion_index    = segment_end_index;
                return selection;
            }

            return selectionFromIndex(segment_end_index);
        }

        segment_start     = segment_end;
        segment_end_index = (segment_end_index + 1) % polyline.size();
    }

    return selectionFromIndex(farthest_index);
}

int PointOrderOptimizer::linkToConsecutiveByIndex(const Polyline& polyline, uint layer_number, Distance minDist) {
    if (polyline.isEmpty()) return 0;

    int startIndex = (static_cast<int>(layer_number) - 2) % polyline.size();
    if (startIndex < 0) startIndex += polyline.size();

    int previousIndex = startIndex;

    Distance dist;
    do {
        startIndex = (startIndex + 1) % polyline.size();
        dist += polyline[previousIndex].distance(polyline[startIndex]);

        // looped through whole polyline
        if (startIndex == previousIndex) { break; }

    } while (dist < minDist);

    return startIndex;
}

int PointOrderOptimizer::computePerturbation(const Polyline& polyline, const Point& current_start, Distance radius) {
    QVector<int> candidates;

    for (int i = 0; i < polyline.size(); ++i) {
        if (polyline[i].distance(current_start) < radius) candidates.push_back(i);
    }

    if (candidates.isEmpty()) return -1;

    return candidates[QRandomGenerator::global()->bounded(candidates.size())];
}

}  // namespace ORNL
