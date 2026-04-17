#pragma once

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "optimizers/point_order_optimizer.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {

// travel (tilt/rotate), CCW, previousIsland for least recently visited, altered paths for Isha's algorithm, travel's
// when doing spiral3D m_current_location needs to be updated after every return

/*!
 * \class PolylineOrderOptimizer
 * \brief This class is used to link together Polylines with travel segments.
 *
 * \note These are how the optimizations are handled:
 * \note Non-implemented optimizations default to shortest distance
 * \list kNextClosest: finds the closest point to link to
 * \list kNextFarthest: finds the farthest point to link to
 * \list kOutsideIn: topological heirarchy ordered most exterior to most interior
 * \list kInsideOut: topological heirarchy ordered most interior to most exterior
 * \list kRandom: links to a random vertex each time
 */
class PolylineOrderOptimizer {
  public:
    //! \brief Constructor
    //! \note Layer number is only needed if kConsecutive linking is used
    PolylineOrderOptimizer(Point& start, uint layer_number);

    //! \brief Links next Polyline based on Polyline's region type: infill/skin, skeleton, or perimeter/inset
    //! \param Polylines: Already existing Polylines to consider for intersection. Used by infill/skin patterns
    //! to determine whether to insert travels or build moves between Polylines.
    //! \return Linked Polyline with travel
    Polyline linkNextPolyline(QVector<Polyline> lines = QVector<Polyline>());

    //! \brief Set Polylines to evaluate
    //! \param Polylines: Copy of Polylines to evaluate
    void setGeometryToEvaluate(QVector<Polyline> geometry, RegionType type, PathOrderOptimization optimization);

    /// @brief Set parameters (when computing infill)
    /// @param infillPattern: Pattern to evaluate
    /// @param border_geometry: Geometry to consider for intersection testing
    /// @param minDistance: minimum distance of resulting polyline
    /// @param minTravelDistance: minimum distance for travel to be used instead of link
    /// @param enable_partitioned_linking: whether to enable partitioned linking of line infill
    void setInfillParameters(InfillPatterns infillPattern, PolygonList border_geometry, Distance minInfillPathDistance,
                             Distance minTravelDistance, bool enable_partitioned_linking = false);

    void setPointParameters(PointOrderOptimization pointOptimization, bool minDistanceEnable,
                            Distance minDistanceThreshold, Distance consecutiveThreshold, bool randomnessEnable,
                            Distance randomnessRadius, bool enableSegmentBreaking);

    //! \brief Gets remaining Polylines
    //! \return current Polylines remaining
    int getCurrentPolylineCount();

    //! \brief Link Polyline as part of spiral (works only for a single perimeter)
    //! \param Polyline: Polyline to link
    //! \param layerHeight: height of layer to interpolate points across
    //! \param pointOrder: point order optimization to be used for first layer
    //! \return spiralized Polyline
    Polyline linkSpiralPolyline2D(bool last_spiral, Distance layerHeight, PointOrderOptimization pointOrder);

    //! \brief Set start override for seam selection (custom start point)
    //! \param pt: Seam point
    void setStartOverride(Point pt);

    //! \brief Set start override for seam selection for subsequent Point Optimizer (custom start point)
    //! \param pt: Seam point
    void setStartPointOverride(Point pt);

  private:
    //! \brief Node to hold topological data
    //! \param m_poly: Polygon representing current Polyline. Used for inside/outside comparison.
    //! \param m_Polyline_index: Polyline index in m_Polylines that node represents.
    //! \param m_children: Child nodes that are contained within the current node.
    struct TopologicalNode {
        Polyline m_poly;
        int m_Polyline_index;
        QVector<QSharedPointer<TopologicalNode>> m_children;

        TopologicalNode() {}
        TopologicalNode(int index, Polyline poly) {
            m_Polyline_index = index;
            m_poly = poly;
        }
    };

    //! \brief Links the POO position to the given Polyline with a travel (used for closed contours)
    Polyline linkTo();

    //! \brief Applies a point-order selection by optionally splitting a segment and rotating the closed loop
    //! \param polyline: Closed-loop polyline to mutate
    //! \param selection: Rotation/split instructions from the point optimizer
    void applyPointOrderSelection(Polyline& polyline, const PointOrderOptimizer::PointOrderSelection& selection) const;

    //! \brief Links together and orders next, single infill Polyline
    //! \param Polylines: Polylines to consider
    //! \return Next Polyline linked via travel
    Polyline linkNextInfillPolyline(QVector<Polyline>& polylines);

    //! \brief Links together and orders next, single skeleton Polyline
    //! \return Next Polyline linked via travel
    Polyline linkNextSkeletonPolyline();

    //! \brief Links single Polyline in line infill
    //! \return Next Polyline linked via travel
    Polyline linkNextInfillLines(QVector<Polyline>& polylines);

    //! \brief Links single Polyline in concentric infill
    //! \return Next Polyline linked via travel
    Polyline linkNextInfillConcentric();

    //! \brief Determines whether a link intersects with the infill or border geometry
    //! \param link_start: Start of link
    //! \param link_end: End of link
    //! \param infill_geometry: Existing infill Polylineing to test against
    //! \param border_geometry: Boundary geometry to test against
    //! \return whether or not intersection occurs
    bool linkIntersects(Point link_start, Point link_end, QVector<Polyline> infill_geometry,
                        PolygonList border_geometry);

    //! \brief Finds the index and end of the the closest Polyline to m_current_location
    //! \param polylines: vector of polylines to test for closeness
    //! \param currentLocation: current location to test distance from
    //! \return Index for closest Polyline and whether or not to link to beginning or end
    QPair<int, bool> closestOpenPolyline(QVector<Polyline> polylines, Point currentLocation);

    //! \brief Links to a Polyline using shortest or longest distance
    //! \param shortest: Whether to look for shortest or longest (shortest by default)
    //! \return Index for vertex in closest Polyline and index for Polyline itself
    int findShortestOrLongestDistance(bool shortest = true);

    //! \brief Links to a random vertex each time
    //! \return Index for vertex in closest Polyline and index for Polyline itself
    int linkToRandom();

    //! \brief Links to a Polyline using shortest or longest distance
    //! \param shortest: Whether to look for shortest or longest (shortest by default)
    //! \return Index for vertex in closest Polyline and index for Polyline itself
    int findInteriorExterior(bool ExtToInt = true);

    //! \brief Computes the topological heirarchy necessary for outside-in and inside-out optimization schemes
    //! \return Returns pointer to root node of n-ary tree representing heirarchy
    QSharedPointer<TopologicalNode> computeTopologicalHeirarchy();

    //! \brief Inserts node into n-ary tree to build topological heirarchy
    //! \param root: Current parent under evaluation
    //! \param current: Current node to insert
    void insert(QSharedPointer<TopologicalNode> root, QSharedPointer<TopologicalNode> current);

    //! \brief Performs a level order walk of topological heirarchy to create final Polyline ordering
    //! \param root: Root node to begin walk at
    void levelOrder(QSharedPointer<TopologicalNode> root);

    //! \brief The last point we linked
    Point& m_current_location;

    //! \brief The layer number the POO is on. Used by consecutive linker
    uint m_layer_number;

    //! \brief Internal copy of the Polylines to evaluated
    QVector<Polyline> m_polylines;

    Distance m_min_distance;
    Distance m_min_travel_distance;
    PathOrderOptimization m_optimization;

    //! \brief Settings needed for Point Optimization
    PointOrderOptimization m_point_optimization;
    bool m_min_point_distance_enable, m_randomness_enable, m_segment_breaking_enable = false;
    Distance m_min_point_distance, m_consecutive_threshold, m_randomness_radius;

    //! \brief Internal copy of the pattern to evaluate
    InfillPatterns m_pattern;

    //! \brief Internal copy of the border geometry to test for intersections
    PolygonList m_border_geometry;

    //! \brief Track whether the override has been used or not.  Allows reset between regions
    bool m_override_used;

    //! \brief Override location to use for linking instead of current location (Polyline and point optimizer)
    Point m_override_location, m_point_override_location;

    //! \brief Current region type for Polyline. Determines which version of link is called
    RegionType m_current_region_type;

    //! \brief The layer number we are currently on
    int m_layer_num;

    //! \brief Holds partial/final topological order for level order walk
    QVector<QVector<int>> m_topo_order;

    //! \brief Used by level order walk to maintain current level of evaluation
    int m_topo_level;

    //! \brief Whether or not heirarchy has been computed. Must only be computed once and can simply be queried for each
    //! subsequent Polyline.
    bool m_has_computed_heirarchy;

    /// @brief Whether to enable partitioned linking of line infill, which links lines in the same partition before
    /// linking between partitions
    bool m_enable_partitioned_linking = false;
};
} // namespace ORNL
