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

//    int PointOrderOptimizer::linkToPoint(Point current_location, Path path, uint layer_number, const
//    QSharedPointer<SettingsBase>& sb)
//    {
//        int result;
//        PointOrderOptimization pointOrderOptimization =
//        static_cast<PointOrderOptimization>(sb->setting<int>(PS::Optimizations::kPointOrder));

//        switch(pointOrderOptimization)
//        {
//            case PointOrderOptimization::kNextClosest:
//                result = findShortestOrLongestDistance(path, current_location,
//                                                       sb->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
//                                                       sb->setting<Distance>(PS::Optimizations::kMinDistanceThreshold));
//            break;
//            case PointOrderOptimization::kNextFarthest:
//                result = findShortestOrLongestDistance(path, current_location,
//                                                       sb->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
//                                                       sb->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
//                                                       false);
//            break;
//            case PointOrderOptimization::kRandom:
//                result = linkToRandom(path);
//            break;
//            case PointOrderOptimization::kConsecutive:
//                result = linkToConsecutive(path, layer_number,
//                sb->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold));
//            break;
//            default:
//                result = findShortestOrLongestDistance(path, current_location, false, Distance(0));
//            break;
//        }

//        if(sb->setting<bool>(PS::Optimizations::kLocalRandomnessEnable))
//            result = computePerturbation(path, path[result]->start(),
//            sb->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius));

//        return result;
//    }

//    int PointOrderOptimizer::findShortestOrLongestDistance(Path path, Point startPoint, bool minThresholdEnable,
//    Distance minThreshold, bool shortest)
//    {
//        int pointIndex = -1;
//        Distance closest;
//        if(shortest)
//            closest = Distance(DBL_MAX);
//        if(minThresholdEnable)
//            closest = Distance(minThreshold);

//        for(int i = 0, end = path.size(); i < end; ++i)
//        {
//            Distance dist = path[i]->start().distance(startPoint);
//            if(shortest)
//            {
//                if (dist < closest)
//                {
//                    closest = dist;
//                    pointIndex = i;
//                }
//            }
//            else
//            {
//                if (dist > closest)
//                {
//                    closest = dist;
//                    pointIndex = i;
//                }
//            }
//        }

//        //if no candidates found it's because nothing met threshold, so find farthest point
//        if(pointIndex == -1)
//            pointIndex = findShortestOrLongestDistance(path, startPoint, false, Distance(0), false);

//        return pointIndex;
//    }

//    int PointOrderOptimizer::linkToRandom(Path path)
//    {
//        return QRandomGenerator::global()->bounded(path.size());
//    }

//    int PointOrderOptimizer::linkToConsecutive(Path path, uint layer_number, Distance minDist)
//    {
//        int startIndex = layer_number - 2;
//        if(startIndex < 0)
//            startIndex += path.size();
//        else
//            startIndex %= path.size();

//        int previousIndex = startIndex;

//        Distance dist;
//        do
//        {
//            startIndex = (startIndex + 1) % path.size();
//            QSharedPointer<SegmentBase> previous = path.at(previousIndex);
//            QSharedPointer<SegmentBase> current = path.at(startIndex);
//            dist += previous->start().distance(current->start());

//            //looped through whole path
//            if(startIndex == previousIndex)
//            {
//                break;
//            }

//        } while(dist < minDist);

//        return startIndex;
//    }

//    int PointOrderOptimizer::computePerturbation(Path path, Point current_start, Distance radius)
//    {
//        QList<QSharedPointer<SegmentBase>> segments = path.getSegments();
//        QVector<int> candidates;

//        for(int i = 0; i < segments.size(); ++i)
//        {
//            if(segments[i]->start().distance(current_start) < radius)
//                candidates.push_back(i);
//        }

//        return candidates[QRandomGenerator::global()->bounded(candidates.size())];
//    }
} // namespace ORNL
