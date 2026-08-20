#pragma once

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {

class TravelSegment;
/*!
 * \class PathOrderOptimizer
 * \brief This class is used to link together paths with travel segments.
 *
 * \note These are how the optimizations are handled:
 * \note Non-implemented optimizations default to shortest distance
 * \list kNextClosest: finds the closest point to link to
 * \list kNextFarthest: finds the farthest point to link to
 * \list kOutsideIn: topological heirarchy ordered most exterior to most interior
 * \list kInsideOut: topological heirarchy ordered most interior to most exterior
 * \list kRandom: links to a random vertex each time
 */
class PathOrderOptimizer {
  public:
    //! \brief Constructor
    //! \note Layer number is only needed if kConsecutive linking is used
    PathOrderOptimizer(Point& start, uint layer_number, const QSharedPointer<SettingsBase>& sb);

    //! \brief Links next path based on path's region type: infill/skin, skeleton, or perimeter/inset
    //! \param paths: Already existing paths to consider for intersection. Used by infill/skin patterns
    //! to determine whether to insert travels or build moves between paths.
    //! \return Linked path with travel
    Path linkNextPath(QVector<Path> paths = QVector<Path>());

    /*!
     * @brief Links the next open radial arc using the configured path and point order settings.
     * @param center Cylinder center used to turn outside-in and inside-out into angular sweep ordering.
     * @return Linked radial path with a travel prepended.
     */
    Path linkNextRadialPath(const Point& center);

    //! \brief Set paths to evaluate
    //! \param paths: Copy of paths to evaluate
    void setPathsToEvaluate(QVector<Path> paths);

    //! \brief Set parameters (when computing infill)
    //! \param infillPattern: Pattern to evaluate
    //! \param border_geometry: Geometry to consider for intersection testing
    void setParameters(InfillPatterns infillPattern, PolygonList border_geometry);

    //! \brief Set parameters (when computing least recently visited optimization strategy)
    //! \param previousIslands: List of previously visited islands
    void setParameters(PolygonList previousIslands);

    //! \brief Gets remaining paths
    //! \return current paths remaining
    int getCurrentPathCount();

    //! \brief Gets current location
    //! \return current location
    Point& getCurrentLocation();

    //! \brief Link path as part of spiral (works only for a single perimeter)
    //! \param path: Path to link
    //! \param last_spiral: whether last region was spiralized or not
    //! \return spiralized path
    Path linkSpiralPath2D(bool last_spiral);

    //! \brief Set start override for seam selection (custom start point)
    //! \param pt: Seam point
    void setStartOverride(Point pt);

    //! \brief Set start override for seam selection for subsequent Point Optimizer (custom start point)
    //! \param pt: Seam point
    void setStartPointOverride(Point pt);

  private:
    //! \brief Node to hold topological data
    //! \param m_poly: Polygon representing current path. Used for inside/outside comparison.
    //! \param m_path_index: Path index in m_paths that node represents.
    //! \param m_children: Child nodes that are contained within the current node.
    struct TopologicalNode {
        Polygon m_poly;
        int m_path_index;
        QVector<QSharedPointer<TopologicalNode>> m_children;

        TopologicalNode() {}
        TopologicalNode(int index, Polygon poly) {
            m_path_index = index;
            m_poly = poly;
        }
    };

    //! \brief Links the POO position to the given path with a travel (used for closed contours)
    Path linkTo();

    //! \brief Links together and orders next, single infill path
    //! \param paths: Paths to consider
    //! \return Next path linked via travel
    Path linkNextInfillPath(QVector<Path>& paths);

    //! \brief Links together and orders next, single skeleton path
    //! \return Next path linked via travel
    Path linkNextSkeletonPath();

    /*!
     * @brief Selects an open radial arc and endpoint using radial-compatible path-order semantics.
     * @param center Cylinder center used for angular ordering.
     * @return Selected path index and true when the path should start from its front endpoint.
     */
    QPair<int, bool> radialOpenPath(const Point& center);

    /*!
     * @brief Returns the query point used by radial path ordering.
     * @param optimization Path order strategy currently being evaluated.
     * @return Current, override, or custom path-order location.
     */
    Point radialPathQueryPoint(PathOrderOptimization optimization) const;

    /*!
     * @brief Returns the query point used by radial point ordering.
     * @param optimization Point order strategy currently being evaluated.
     * @return Current, override, or custom point-order location.
     */
    Point radialPointQueryPoint(PointOrderOptimization optimization) const;

    /*!
     * @brief Returns the closer endpoint distance from a query point to an open path.
     * @param query Query point.
     * @param path Open path to evaluate.
     * @return Shortest endpoint distance.
     */
    Distance nearestOpenEndpointDistance(const Point& query, const Path& path) const;

    /*!
     * @brief Returns the angular midpoint of an open radial arc about a center.
     * @param path Open radial arc path.
     * @param center Cylinder center.
     * @return Representative midpoint angle in radians.
     */
    double radialArcMidpointAngle(const Path& path, const Point& center) const;

    /*!
     * @brief Normalizes an angular delta to [0, 2*pi).
     * @param delta Angle delta in radians.
     * @return Positive equivalent angle delta.
     */
    double positiveAngularDelta(double delta) const;

    //! \brief Links one line infill path
    //! \return Next path linked via travel
    Path linkNextInfillLines(QVector<Path>& paths);

    //! \brief Links one concentric infill path
    //! \return Next path linked via travel
    Path linkNextInfillConcentric();

    //! \brief Determines whether a link intersects with the infill or border geometry
    //! \param link_start: Start of link
    //! \param link_end: End of link
    //! \param infill_geometry: Existing infill pathing to test against
    //! \param border_geometry: Boundary geometry to test against
    //! \return whether or not intersection occurs
    bool linkIntersects(Point link_start, Point link_end, QVector<Path> infill_geometry, PolygonList border_geometry);

    //! \brief Finds the index and end of the the closest path to m_current_location
    //! \return Index for closest path and whether or not to link to beginning or end
    QPair<int, bool> closestOpenPath(QVector<Path> paths);

    //! \brief Adds a travel segment to the index point and shuffles the paths to be in order
    void addTravel(int index, Path& path);

    //! \brief Links to a path using shortest or longest distance
    //! \param shortest: Whether to look for shortest or longest (shortest by default)
    //! \return Index for vertex in closest path and index for path itself
    int findShortestOrLongestDistance(bool shortest = true);

    //! \brief Links to a random vertex each time
    //! \return Index for vertex in closest path and index for path itself
    int linkToRandom();

    //! \brief Links to a path using shortest or longest distance
    //! \param shortest: Whether to look for shortest or longest (shortest by default)
    //! \return Index for vertex in closest path and index for path itself
    int findInteriorExterior(bool ExtToInt = true);

    //! \brief Computes the topological heirarchy necessary for outside-in and inside-out optimization schemes
    //! \return Returns pointer to root node of n-ary tree representing heirarchy
    QSharedPointer<TopologicalNode> computeTopologicalHeirarchy();

    //! \brief Inserts node into n-ary tree to build topological heirarchy
    //! \param root: Current parent under evaluation
    //! \param current: Current node to insert
    void insert(QSharedPointer<TopologicalNode> root, QSharedPointer<TopologicalNode> current);

    //! \brief Performs a level order walk of topological heirarchy to create final path ordering
    //! \param root: Root node to begin walk at
    void levelOrder(QSharedPointer<TopologicalNode> root);

    //! \brief Settings the optimizer will use.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief The last point we linked
    Point& m_current_location;

    //! \brief The layer number the POO is on. Used by consecutive linker
    uint m_layer_number;

    //! \brief The layer number links completed by the least recently visited linker
    uint m_links_done;

    //! \brief Internal copy of the paths to evaluated
    QVector<Path> m_paths;

    //! \brief Internal copy of the pattern to evaluate
    InfillPatterns m_pattern;

    //! \brief Internal copy of the border geometry to test for intersections
    PolygonList m_border_geometry;

    //! \brief Track whether the override has been used or not.  Allows reset between regions
    bool m_override_used;

    //! \brief Track whether the point-order override has been set.
    bool m_point_override_used;

    //! \brief Override location to use for linking instead of current location (path and point optimizer)
    Point m_override_location, m_point_override_location;

    //! \brief Current region type for path. Determines which version of link is called
    RegionType m_current_region_type;

    //! \brief The layer number we are currently on
    int m_layer_num;

    //! \brief Holds partial/final topological order for level order walk
    QVector<QVector<int>> m_topo_order;

    //! \brief Used by level order walk to maintain current level of evaluation
    int m_topo_level;

    //! \brief Whether or not heirarchy has been computed. Must only be computed once and can simply be queried for each
    //! subsequent path.
    bool m_has_computed_heirarchy;
};
} // namespace ORNL
