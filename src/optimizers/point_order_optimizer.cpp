#include "optimizers/point_order_optimizer.h"

#include <cfloat>

#include <QRandomGenerator>
#include <qcontainerfwd.h>
#include <qtypes.h>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {

int PointOrderOptimizer::linkToPoint(Point current_location, Polyline polyline, uint layer_number,
                                     PointOrderOptimization pointOptimization, bool min_dist_enabled,
                                     Distance min_dist_threshold, Distance consecutive_dist_threshold,
                                     bool local_randomness_enable, Distance randomness_radius) {
    int result;

    switch (pointOptimization) {
        case PointOrderOptimization::kNextClosest:
            result = findShortestOrLongestDistance(polyline, current_location, min_dist_enabled, min_dist_threshold);
            break;
        case PointOrderOptimization::kNextFarthest:
            result =
                findShortestOrLongestDistance(polyline, current_location, min_dist_enabled, min_dist_threshold, false);
            break;
        case PointOrderOptimization::kRandom:
            result = linkToRandom(polyline);
            break;
        case PointOrderOptimization::kConsecutive:
            result = linkToConsecutive(polyline, layer_number, consecutive_dist_threshold);
            break;
        default:
            result = findShortestOrLongestDistance(polyline, current_location, false, Distance(0));
            break;
    }

    if (local_randomness_enable)
        result = computePerturbation(polyline, polyline[result], randomness_radius);

    return result;
}

bool PointOrderOptimizer::findSkeletonPointOrder(Point current_location, Polyline polyline,
                                                 PointOrderOptimization pointOptimization, bool min_dist_enabled,
                                                 Distance min_dist_threshold) {
    bool result = false;
    int index;

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

int PointOrderOptimizer::findShortestOrLongestDistance(Polyline polyline, Point startPoint, bool minThresholdEnable,
                                                       Distance minThreshold, bool shortest) {
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

int PointOrderOptimizer::findClosestEnd(Polyline polyline, Point currentPoint, bool minThresholdEnable,
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

int PointOrderOptimizer::linkToRandom(Polyline polyline) {
    return QRandomGenerator::global()->bounded(polyline.size());
}

int PointOrderOptimizer::linkToConsecutive(Polyline polyline, uint layer_number, Distance minDist) {
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

int PointOrderOptimizer::computePerturbation(Polyline polyline, Point current_start, Distance radius) {
    QVector<int> candidates;

    for (int i = 0; i < polyline.size(); ++i) {
        if (polyline[i].distance(current_start) < radius)
            candidates.push_back(i);
    }

    return candidates[QRandomGenerator::global()->bounded(candidates.size())];
}

} // namespace ORNL
