#pragma once

#include <qtypes.h>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {

/*!
 * \class PointOrderOptimizer
 * \brief This class is used to select specific points on a polyline to link to
 *
 * \note These are how the optimizations are handled:
 * \note Non-implemented optimizations default to shortest distance
 * \list Next Closest – Pick the point on the polyline that is closest to the current location.
 * \list Next Farthest – Pick the point on the polyline that is farthest from the current location.
 * \list Random – Randomly select a point on each polyline.
 * \list Consecutive – Move the start point a minimum user defined distance from the previous layer’s start point.
 * \list Custom Point – User defines a custom point, and the optimizer picks the point on each polyline that is closest
 * to the defined point.
 */
class PointOrderOptimizer {
  public:
    struct PointOrderSelection {
        int rotation_index = 0;
        bool insert_split_point = false;
        Point split_point;
        int insertion_index = 0;
    };

    //! \brief Constructor
    //! \note Layer number is only needed if kConsecutive linking is used
    //! \param current_location: The current location
    //! \param polyline: The polyline to link to
    //! \param layer_number: The layer number of the current polyline
    //! \param sb: The settings base to use
    //! \return Point selection details for rotating and optionally splitting the polyline
    static PointOrderSelection linkToPoint(const Point& current_location, const Polyline& polyline, uint layer_number,
                                           PointOrderOptimization pointOptimization, bool min_dist_enabled,
                                           Distance min_dist_threshold, Distance consecutive_dist_threshold,
                                           bool local_randomness_enable, Distance randomness_radius,
                                           bool allow_segment_breaking = false);

    //! \brief Constructor
    //! \param current_location: The current location
    //! \param polyline: The polyline to link to
    //! \param pointOptimization: the optimization method to use
    //! \return True if the order should be reversed or false if the order is correct
    static bool findSkeletonPointOrder(const Point& current_location, const Polyline& polyline,
                                       PointOrderOptimization pointOptimization, bool min_dist_enabled,
                                       Distance min_dist_threshold);

  private:
    //! \brief Finds the shortest or longest distance between the current location and the points in the polyline
    //! \param polyline: The polyline to find the shortest or longest distance in
    //! \param startPoint: The current location
    //! \param minThresholdEnable: Whether or not to use the minimum threshold
    //! \param minThreshold: The minimum threshold to use
    //! \param shortest: Whether or not to find the shortest distance
    //! \return The index of the point with the shortest or longest distance
    static int findShortestOrLongestDistance(const Polyline& polyline, const Point& startPoint, bool minThresholdEnable,
                                             Distance minThreshold, bool shortest = true);

    //! \brief Finds the closest point on a closed-loop polyline, optionally splitting a segment to reach it
    //! \param polyline: The closed-loop polyline to evaluate
    //! \param queryPoint: Point to project onto the loop
    //! \return Rotation/split selection for the closest point on the loop
    static PointOrderSelection findClosestPointOnClosedLoop(const Polyline& polyline, const Point& queryPoint);

    //! \brief Creates a point selection that rotates to an existing point without splitting a segment
    //! \param pointIndex: Index of the selected point in the polyline
    //! \return Rotation-only point selection
    static PointOrderSelection selectionFromIndex(int pointIndex);

    //! \brief Finds the closest end point of an open-loop polyline
    //! \param polyline: The polyline to find the closest end point
    //! \param currentPoint: The current location
    //! \param minThresholdEnable: Whether or not to use the minimum threshold
    //! \param minThreshold: The minimum threshold to use
    //! \return The index of the point with the shortest or longest distance
    static int findClosestEnd(const Polyline& polyline, const Point& currentPoint, bool minThresholdEnable,
                              Distance minThreshold);

    //! \brief Links to a random point in the polyline
    //! \param polyline: The polyline to link to
    //! \return The index of the point to link to
    static int linkToRandom(const Polyline& polyline);

    //! \brief Links to the next consecutive point in the polyline based on the layer number
    //! \param polyline: The polyline to link to
    //! \param layer_number: The layer number of the current polyline
    //! \param minDist: The minimum distance the consecutive point must be from the previous layer's start point
    //! \return The index of the point to link to
    static int linkToConsecutive(const Polyline& polyline, uint layer_number, Distance minDist);

    //! \brief Computes the perturbation of the point after the current optimization scheme is run
    //! \param polyline: The polyline to select candidates for perturbation
    //! \param current_start: The current start point
    //! \param radius: The radius of the perturbation
    //! \return The index of the point to link to
    static int computePerturbation(const Polyline& polyline, const Point& current_start, Distance radius);
};

} // namespace ORNL
