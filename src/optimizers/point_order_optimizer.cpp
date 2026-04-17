#include "optimizers/point_order_optimizer.h"

#include <algorithm>
#include <cfloat>
#include <limits>

#include <QRandomGenerator>
#include <qcontainerfwd.h>
#include <qtypes.h>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {

PointOrderOptimizer::PointOrderSelection PointOrderOptimizer::linkToPoint(
    const Point& current_location, const Polyline& polyline, uint layer_number,
    PointOrderOptimization pointOptimization, bool min_dist_enabled, Distance min_dist_threshold,
    Distance consecutive_dist_threshold, bool local_randomness_enable, Distance randomness_radius,
    bool allow_segment_breaking) {
    PointOrderSelection result;

    bool use_segment_breaking =
        allow_segment_breaking && polyline.size() > 1 && !min_dist_enabled && !local_randomness_enable &&
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
            result = selectionFromIndex(linkToConsecutive(polyline, layer_number, consecutive_dist_threshold));
            break;
        case PointOrderOptimization::kCustomPoint:
            result = selectionFromIndex(findShortestOrLongestDistance(polyline, current_location, false, Distance(0)));
            break;
        default:
            result = selectionFromIndex(findShortestOrLongestDistance(polyline, current_location, false, Distance(0)));
            break;
    }

    if (local_randomness_enable)
        result = selectionFromIndex(computePerturbation(polyline, polyline[result.rotation_index], randomness_radius));

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
            if (findClosestEnd(polyline, current_location, min_dist_enabled, min_dist_threshold) == 0)
                return true;
            else
                return false;
            break;
        case PointOrderOptimization::kRandom:
            if (QRandomGenerator::global()->generate() % 2)
                result = true;
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
    if (shortest)
        closest = Distance(DBL_MAX);
    if (minThresholdEnable)
        closest = Distance(minThreshold);

    for (int i = 0, end = polyline.size(); i < end; ++i) {
        Distance dist = polyline[i].distance(startPoint);
        if (shortest) {
            if (dist < closest) {
                closest = dist;
                pointIndex = i;
            }
        }
        else {
            if (dist > closest) {
                closest = dist;
                pointIndex = i;
            }
        }
    }

    // if no candidates found it's because nothing met threshold, so find farthest point
    if (pointIndex == -1)
        pointIndex = findShortestOrLongestDistance(polyline, startPoint, false, Distance(0), false);

    return pointIndex;
}

PointOrderOptimizer::PointOrderSelection PointOrderOptimizer::findClosestPointOnClosedLoop(const Polyline& polyline,
                                                                                           const Point& queryPoint) {
    PointOrderSelection result;
    if (polyline.isEmpty())
        return result;

    double closest_distance = std::numeric_limits<double>::max();

    for (int i = 0, end = polyline.size(); i < end; ++i) {
        int next_index = (i + 1) % polyline.size();
        auto [closest_point, distance] = MathUtils::nearestPointOnSegment(polyline[i], polyline[next_index], queryPoint);

        if (distance < closest_distance) {
            closest_distance = distance;

            if (closest_point == polyline[i]) {
                result = selectionFromIndex(i);
            }
            else if (closest_point == polyline[next_index]) {
                result = selectionFromIndex(next_index);
            }
            else {
                result.rotation_index = next_index;
                result.insert_split_point = true;
                result.split_point = closest_point;
                result.insertion_index = next_index;
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

int PointOrderOptimizer::linkToRandom(const Polyline& polyline) { return QRandomGenerator::global()->bounded(polyline.size()); }

int PointOrderOptimizer::linkToConsecutive(const Polyline& polyline, uint layer_number, Distance minDist) {
    int startIndex = layer_number - 2;
    if (startIndex < 0)
        startIndex += polyline.size();
    else
        startIndex %= polyline.size();

    int previousIndex = startIndex;

    Distance dist;
    do {
        startIndex = (startIndex + 1) % polyline.size();
        dist += polyline[previousIndex].distance(polyline[startIndex]);

        // looped through whole polyline
        if (startIndex == previousIndex) {
            break;
        }

    } while (dist < minDist);

    return startIndex;
}

int PointOrderOptimizer::computePerturbation(const Polyline& polyline, const Point& current_start, Distance radius) {
    QVector<int> candidates;

    for (int i = 0; i < polyline.size(); ++i) {
        if (polyline[i].distance(current_start) < radius)
            candidates.push_back(i);
    }

    return candidates[QRandomGenerator::global()->bounded(candidates.size())];
}

} // namespace ORNL
