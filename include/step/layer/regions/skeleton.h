#pragma once

#include "QStack"
#include "boost/graph/adjacency_list.hpp"
#include "boost/graph/connected_components.hpp"
#include "boost/graph/filtered_graph.hpp"
#include "boost/graph/graph_traits.hpp"
#include "boost/graph/subgraph.hpp"
#include "boost/property_map/property_map.hpp"
#include "geometry/segments/line.h"
#include "step/layer/regions/region_base.h"

namespace ORNL {
using SkeletonGraph = boost::adjacency_list<boost::listS, boost::listS, boost::undirectedS, Point, Polyline>;
using SkeletonVertex = boost::graph_traits<SkeletonGraph>::vertex_descriptor;
using SkeletonEdge = boost::graph_traits<SkeletonGraph>::edge_descriptor;

struct SkeletonSubgraphFilter {
    SkeletonSubgraphFilter() = default;

    SkeletonSubgraphFilter(QMap<SkeletonVertex, int> vertex_subgraph_map_)
        : vertex_subgraph_map(vertex_subgraph_map_) {}

    bool operator()(const SkeletonVertex& v) const { return vertex_subgraph_map[v] == 0; }

    QMap<SkeletonVertex, int> vertex_subgraph_map;
};

using SkeletonSubgraph = boost::filtered_graph<SkeletonGraph, boost::keep_all, SkeletonSubgraphFilter>;

class Skeleton : public RegionBase {
  public:
    //! \brief Constructor
    //! \param sb: the settings
    //! \param index: index for region order
    //! \param settings_polygons: a vector of settings polygons to apply
    //! \param gridInfo: optional external file information
    Skeleton(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons,
             const SingleExternalGridInfo& gridInfo, bool iswireFed = false);

    //! \brief Writes the gcode for the skeleton
    //! \param writer is the instance of the Writer Base to be used for writing skeleton region GCode
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the skeleton region
    void compute(uint layer_num, QSharedPointer<SyncManager>& sync) override;

    //! \brief Computes a Voronoi Diagram from a set of segments
    void computeSegmentVoronoi();

    //! \brief Computes a Voronoi Diagram from a set of points (Experimental)
    void computePointVoronoi();

    //! \brief Incorporates lost geometry into m_geometry
    void incorporateLostGeometry();

    //! \brief Cleans input geometry using ClipperLib2's cleanPolygons function
    void simplifyInputGeometry();

    //! \brief Cleans output geometry according to ClipperLib2's cleanPolygons function
    void simplifyOutputGeometry();

    //! \brief Generates a graph representation of skeleton geometry for cleaning
    void generateSkeletonGraph();

    //! \brief Cleans graph of skeleton geometry according to ClipperLib's CleanPolygon function
    //! \param cleaning_distance is the determinant distance used for cleaning the skeleton graph
    //! \note Deprecated: cleaning now takes place in simplifyOutputGeometry
    void cleanSkeletonGraph(Distance cleaning_distance);

    //! \brief Extracts cycles from m_skeleton_graph
    void extractCycles();

    //! \brief Extracts simple paths from m_skeleton_graph
    void extractSimplePaths();

    //! \brief Extracts passed path from m_skeleton_graph
    //! \param path is the computed skeleton path to be extracted from the skeleton graph and placed in
    //! m_computed_geometry
    void extractPath(QVector<SkeletonEdge> path);

    /// @brief Used for internal inspection of skeleton geometry. To be used after compute and before optimize.
    /// @details Prints the input geometry and computed skeleton geometry to Qt's debug output in a Desmos-compatible
    /// format. Each line segment is printed as a separate polygon in the format "polygon((x1,y1),(x2,y2))". Polylines
    /// with fewer than 2 points will not be printed.
    void inspectSkeleton();

    /// @brief Used for internal inspection of skeleton graph. To be used after generateSkeletonGraph and before
    /// optimize.
    /// @details Prints the input geometry and skeleton graph edges to Qt's debug output in a Desmos-compatible format.
    /// Each edge is printed as a separate polygon in the format "polygon((x1,y1),(x2,y2))".
    void inspectSkeletonGraph();

    //! \brief Optimizes the region.
    //! \param layerNumber: current layer number
    //! \param innerMostClosedContour: used for subsequent path modifiers
    //! \param outerMostClosedContour: used for subsequent path modifiers
    //! \param current_location: most recent location
    //! \param shouldNextPathBeCCW: state as to CW or CCW of previous path for use with additional DOF
    void optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                  QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) override;

    /**
     * @brief Populates the segment settings with local settings.
     * @param[in,out] segment_sb: The segment settings base to populate.
     * @param[in] parent_sb: The parent settings base to apply.
     * @param[in] adapted: Whether or not the segment is adapted.
     * @param[in] adapted_width: The adapted width to apply if the segment is adapted.
     * @param[in] adapted_speed: The adapted speed to apply if the segment is adapted.
     * @note If adapted is true, adapted_width and adapted_speed must be provided.
     */
    static void populateSegmentSettings(QSharedPointer<SettingsBase> segment_sb,
                                        const QSharedPointer<SettingsBase>& parent_sb, bool adapted = false,
                                        const Distance& adapted_width = Distance(),
                                        const Velocity& adapted_speed = Velocity());

    /**
     * @brief Create segments from a start and end point with the given settings base.
     * @param[in] start: The start point of the segment.
     * @param[in] end: The end point of the segment.
     * @param[in] parent_sb: The settings base to apply to the segments.
     * @return A list of segments created from the start and end points.
     */
    LSegmentList createSegments(const Point& start, const Point& end,
                                const QSharedPointer<SettingsBase>& parent_sb) const;

    /**
     * @brief Create a path from a polyline.
     * @param[in] line: The polyline representing the path.
     * @return A path created from the polyline.
     */
    Path createPath(Polyline line) override;

    /**
     * @brief Create a path with localized settings applied to segments based on settings regions.
     * @param[in] line Polyline representing the path.
     * @return Path with localized settings applied.
     * @warning Handles cases of overlapping settings regions by applying the first region found.
     */
    Path createPathWithLocalizedSettings(const Polyline& line);

    /**
     * @brief Filter path segments based on bead width.
     * @param[in] path: the path to be filtered.
     * @return A vector of paths that have been filtered based on bead width.
     */
    QVector<Path> filterPath(const Path& path);

    //! \brief Sets pathing for anchor lines
    //! \param anchor_lines: polylines for wire feed
    void setAnchorWireFeed(QVector<Polyline> anchor_lines);

  private:
    //! \brief Creates modifiers
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    //! \param innerMostClosedContour used for Prestarts (currently only skins/infill)
    void calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) override;

    //! \brief Holds raw skeleton geometry produced by the Voronoi Generator
    QVector<Polyline> m_skeleton_geometry;

    //! \brief Holds graph representation of skeleton geometry
    SkeletonGraph m_skeleton_graph;

    //! \brief Holds computed geometry
    QVector<Polyline> m_computed_geometry;

    //! \brief Whether or not skeletons belong to wire fed areas or not
    bool m_wire_region;

    //! \brief Precomputed paths for wire feed at anchors
    QVector<Polyline> m_computed_anchor_lines;

    //! @brief The current layer number being processed
    uint m_layer_num;
};
} // namespace ORNL
