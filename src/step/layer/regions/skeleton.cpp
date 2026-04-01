#include "step/layer/regions/skeleton.h"

#include "boost/graph/undirected_dfs.hpp"
#include "boost/polygon/voronoi.hpp"
#include "geometry/geometry_debug.h"
#include "geometry/path_modifier.h"
#include "geometry/segments/line.h"
#include "optimizers/polyline_order_optimizer.h"
#include "utilities/mathutils.h"

template <> struct boost::polygon::geometry_concept<ORNL::Point> {
    typedef point_concept type;
};

template <> struct boost::polygon::point_traits<ORNL::Point> {
    using coordinate_type = int;

    static coordinate_type get(const ORNL::Point& point, orientation_2d orient) {
        return (orient == HORIZONTAL) ? point.x() : point.y();
    }
};

template <> struct boost::polygon::geometry_concept<ORNL::Polyline> {
    typedef segment_concept type;
};

template <> struct boost::polygon::segment_traits<ORNL::Polyline> {
    using coordinate_type = int;
    using point_type = ORNL::Point;

    static point_type get(const ORNL::Polyline& segment, direction_1d dir) {
        return dir.to_int() ? segment.first() : segment.last();
    }
};

namespace ORNL {
Skeleton::Skeleton(const QSharedPointer<SettingsBase>& sb, const int index,
                   const QVector<SettingsPolygon>& settings_polygons, bool isWireFed)
    : RegionBase(sb, index, settings_polygons) {
    m_wire_region = isWireFed;
}

QString Skeleton::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kSkeleton);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kSkeleton);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kSkeleton);
    }
    gcode += writer->writeAfterRegion(RegionType::kSkeleton);
    return gcode;
}

void Skeleton::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_layer_num = layer_num;

    m_paths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kSkeletonNum));

    incorporateLostGeometry();

    if (!m_geometry.isEmpty()) {
        simplifyInputGeometry();

        const SkeletonInput& input = static_cast<SkeletonInput>(m_sb->setting<int>(PS::Skeleton::kSkeletonInput));
        switch (input) {
            case SkeletonInput::kSegments:
                computeSegmentVoronoi();
                break;
            case SkeletonInput::kPoints:
                computePointVoronoi();
                break;
        }

        if (!m_skeleton_geometry.isEmpty()) {
            generateSkeletonGraph();
            extractCycles();
            extractSimplePaths();
            simplifyOutputGeometry();

            //! Uncomment to inspect Skeleton structure. See instructions in header file.
            // inspectSkeleton(layer_num);

            if (m_computed_anchor_lines.size() != 0) {
                m_computed_geometry.push_front(m_computed_anchor_lines.first());
                m_computed_geometry.push_back(m_computed_anchor_lines.last());
            }
        }
        else {
            qDebug() << "\t\tNo permitted skeletons generated from geometry on layer " << m_layer_num;
        }
    }
    else {
        qDebug() << "\t\tNo geometry for skeletons to compute";
    }
}

void Skeleton::computeSegmentVoronoi() {
    const Distance& bead_width = m_sb->setting<Distance>(PS::Skeleton::kBeadWidth);

    // Filter for Voronoi edges whose minimum distance to the boundary is less than the assigned bead width
    auto filter = [bead_width](const Polyline& source1, const Polyline& source2, const Point& edge_start,
                               const Point& edge_end) {
        double min_dist = std::numeric_limits<double>::max();

        // Compute minimum distance between each source segment and the Voronoi edge
        for (const auto& source : {source1, source2}) {
            for (const auto& pt : {edge_start, edge_end}) {
                min_dist =
                    std::min(min_dist, MathUtils::nearestPointOnSegment(source.first(), source.last(), pt).second);
            }
        }

        // Return true if the minimum distance is less than half the bead width
        return (min_dist * 2.0) < bead_width;
    };

    // Gather input geometry
    QVector<Polyline> bounding_edges = m_geometry.getEdges();

    // Construct Voronoi Diagram
    boost::polygon::voronoi_diagram<double> vd;
    construct_voronoi(bounding_edges.begin(), bounding_edges.end(), &vd);

    // Gather skeleton segments
    for (auto cell_iter = vd.cells().cbegin(); cell_iter != vd.cells().end(); ++cell_iter) {
        if (!cell_iter->is_degenerate()) {
            auto edge = cell_iter->incident_edge();

            do {
                if (edge->is_primary()) {
                    if (edge->is_finite()) {
                        // Ensure skeleton segments are only generated once since voronoi diagram implements twin edges
                        if (edge->cell()->source_index() < edge->twin()->cell()->source_index()) {
                            const Polyline& source1 = bounding_edges[edge->cell()->source_index()];
                            const Polyline& source2 = bounding_edges[edge->twin()->cell()->source_index()];

                            //! Ensure skeleton segments run parallel to the border geometry
                            if (source1.first() != source2.first() && source1.first() != source2.last() &&
                                source1.last() != source2.first() && source1.last() != source2.last()) {
                                Point start(edge->vertex0()->x(), edge->vertex0()->y());
                                Point end(edge->vertex1()->x(), edge->vertex1()->y());

                                // If adaptive bead width is enabled, include all skeleton segments.
                                // Otherwise, filter skeleton segments whose minimum distance to the border geometry is
                                // less than half the bead width.
                                if (m_sb->setting<bool>(PS::Skeleton::kSkeletonAdapt)) { // Adaptive bead width
                                    m_skeleton_geometry += m_geometry & Polyline({start, end});
                                }
                                else if (!filter(source1, source2, start, end)) { // Static bead width
                                    m_skeleton_geometry += m_geometry & Polyline({start, end});
                                }
                            }
                        }
                    }
                }
                edge = edge->next();
            } while (edge != cell_iter->incident_edge());
        }
    }

    //! Remove any skeleton segments that may overlap with border geometry
    //! Extremely rare and may be unnecessary with input geometry cleaning
    QVector<Polyline>::iterator segment_iter = m_skeleton_geometry.begin(),
                                segment_iter_end = m_skeleton_geometry.end();
    while (segment_iter != segment_iter_end) {
        if (bounding_edges.contains(*segment_iter) || bounding_edges.contains(segment_iter->reverse())) {
            segment_iter = m_skeleton_geometry.erase(segment_iter);
        }
        else {
            ++segment_iter;
        }
    }
}

void Skeleton::computePointVoronoi() {
    //! Gather input geometry
    QVector<Point> points;
    for (Polygon& poly : m_geometry) {
        points += poly;
    }

    //! Construct Voronoi Diagram
    boost::polygon::voronoi_diagram<double> vd;
    construct_voronoi(points.begin(), points.end(), &vd);

    //! Gather skeleton segments
    for (auto cell_iter = vd.cells().cbegin(); cell_iter != vd.cells().end(); ++cell_iter) {
        auto edge = cell_iter->incident_edge();

        do {
            if (edge->is_primary()) {
                if (edge->is_finite()) {
                    //! Ensures skeleton segments are only generated once since voronoi diagram implements twin edges
                    if (edge->cell()->source_index() < edge->twin()->cell()->source_index()) {
                        Point start(edge->vertex0()->x(), edge->vertex0()->y());
                        Point end(edge->vertex1()->x(), edge->vertex1()->y());

                        //! Ensure skeleton segments fit within the border geometry
                        m_skeleton_geometry += m_geometry & Polyline({start, end});
                    }
                }
                else { //! Infinite edge case
                    auto v0 = edge->vertex0();

                    //! Ensures skeleton segments are only generated once since voronoi diagram implements twin edges
                    if (v0) {
                        Point start(v0->x(), v0->y());

                        //! Determine segment end point from segment seeds
                        const Point& seed1 = points[edge->cell()->source_index()];
                        const Point& seed2 = points[edge->twin()->cell()->source_index()];
                        Point end((seed1.x() + seed2.x()) / 2, (seed1.y() + seed2.y()) / 2);

                        //! Generate skeleton segments that fit within the geometry
                        m_skeleton_geometry += m_geometry & Polyline({start, end});
                    }
                }
            }

            edge = edge->next();
        } while (edge != cell_iter->incident_edge());
    }
}

void Skeleton::incorporateLostGeometry() {
    //! Integrate lost geometry with m_geometry, ensuring they share no common geometry
    for (Polygon& poly : m_geometry.lost_geometry) {
        if ((m_geometry & poly).isEmpty()) {
            m_geometry += poly;
        }
    }
}

void Skeleton::simplifyInputGeometry() {
    const Distance& cleaning_dist = m_sb->setting<Distance>(PS::Skeleton::kSkeletonInputCleaningDistance);

    //! Too large of a cleaning distance may decimate inner/outer polygons such that they
    //! contain no points or intersect each other. Check that cleaning distance is appropriate.
    bool valid = true;
    for (Point& p1 : m_geometry.first()) {
        for (Polygon& poly : m_geometry.mid(1)) {
            for (Point& p2 : poly) {
                if (cleaning_dist > p1.distance(p2)) {
                    valid = false;
                    break;
                }
            }
            if (!valid)
                break;
        }
        if (!valid)
            break;
    }

    if (valid) {
        m_geometry = m_geometry.cleanPolygons(cleaning_dist);
    }
    else {
        qDebug() << "Layer " << m_layer_num << " Skeleton input geometry cleaning distance too large.";
    }

    //! Chamfer long axis corner of triangles
    for (Polygon& poly : m_geometry) {
        if (poly.size() == 3) {
            Point A = poly[0];
            Point B = poly[1];
            Point C = poly[2];

            Angle ABC = MathUtils::internalAngle(A, B, C);
            Angle BCA = MathUtils::internalAngle(B, C, A);
            Angle CAB = MathUtils::internalAngle(C, A, B);

            if (ABC < BCA && ABC < CAB) {
                poly = MathUtils::chamferCorner(A, B, C, 10);
                poly.prepend(A);
                poly.append(C);
            }
            else if (BCA < ABC && BCA < CAB) {
                poly = MathUtils::chamferCorner(B, C, A, 10);
                poly.prepend(B);
                poly.append(A);
            }
            else {
                poly = MathUtils::chamferCorner(C, A, B, 10);
                poly.prepend(C);
                poly.append(B);
            }
        }
    }

    //! Chamfer sharp corners
    const Angle& threshold = m_sb->setting<Angle>(PS::Skeleton::kSkeletonInputChamferingAngle);
    for (Polygon& geometry : m_geometry) {
        Polyline poly = geometry.toPolyline();
        Polygon new_geometry;

        for (uint i = 1, max = poly.size() - 1; i < max; ++i) {
            if (MathUtils::internalAngle(poly[i - 1], poly[i], poly[i + 1]) < threshold) {
                new_geometry.append(MathUtils::chamferCorner(poly[i - 1], poly[i], poly[i + 1], 10));
            }
            else {
                new_geometry.append(poly[i]);
            }
        }

        if (MathUtils::internalAngle(geometry.last(), geometry[0], geometry[1]) < threshold) {
            new_geometry.append(MathUtils::chamferCorner(geometry.last(), geometry[0], geometry[1], 10));
        }
        else {
            new_geometry.append(geometry.first());
        }

        geometry = new_geometry;
    }
}

void Skeleton::simplifyOutputGeometry() {
    const Distance& cleaning_distance = m_sb->setting<Distance>(PS::Skeleton::kSkeletonOutputCleaningDistance);

    for (Polyline& poly_line : m_computed_geometry) {
        poly_line = poly_line.simplify(cleaning_distance);
    }
}

void Skeleton::generateSkeletonGraph() {
    //! Tracks points that have been added to the graph
    QMap<Point, SkeletonVertex> vertices;

    for (Polyline& edge : m_skeleton_geometry) {
        SkeletonVertex v0;
        if (vertices.contains(edge.first())) {
            v0 = vertices[edge.first()];
        }
        else {
            v0 = add_vertex(edge.first(), m_skeleton_graph);
            vertices.insert(edge.first(), v0);
        }

        SkeletonVertex v1;
        if (vertices.contains(edge.last())) {
            v1 = vertices[edge.last()];
        }
        else {
            v1 = add_vertex(edge.last(), m_skeleton_graph);
            vertices.insert(edge.last(), v1);
        }

        add_edge(v0, v1, edge, m_skeleton_graph);
    }
}

void Skeleton::cleanSkeletonGraph(Distance cleaning_distance) {
    Distance distSqrd = std::pow(cleaning_distance(), 2);

    auto [vi, vi_end] = vertices(m_skeleton_graph);

    //! Create vertex modification map
    QMap<SkeletonVertex, bool> vertex_mod_map;
    while (vi != vi_end) {
        vertex_mod_map.insert(*vi, false);
        ++vi;
    }

    std::tie(vi, vi_end) = vertices(m_skeleton_graph);
    while (vi != vi_end) {
        auto v_root = vi;
        vi++;

        //! Clean skeleton graph according to ClipperLib2's cleanPolygons function.
        //! Vertices are analyzed in sets of 3: v0--v_root--v1 with pivot vertices (degree != 2) being ignored.
        //! If a vertice has undergone a modification it is marked so as not to be modified again. This prevents
        //! skeletal drifting.
        if (boost::degree(*v_root, m_skeleton_graph) == 2 && !vertex_mod_map[*v_root]) {
            auto [e0, e1] = boost::out_edges(*v_root, m_skeleton_graph);
            --e1;

            SkeletonVertex v0 = boost::target(*e0, m_skeleton_graph);
            SkeletonVertex v1 = boost::target(*e1, m_skeleton_graph);

            if (MathUtils::pointsAreClose(m_skeleton_graph[*v_root], m_skeleton_graph[v0], distSqrd) ||
                MathUtils::pointsAreClose(m_skeleton_graph[*v_root], m_skeleton_graph[v1], distSqrd) ||
                MathUtils::pointsAreClose(m_skeleton_graph[v0], m_skeleton_graph[v1], distSqrd) ||
                MathUtils::slopesNearCollinear(m_skeleton_graph[v0], m_skeleton_graph[*v_root], m_skeleton_graph[v1],
                                               distSqrd)) {
                add_edge(v0, v1, {Polyline({m_skeleton_graph[v0], m_skeleton_graph[v1]})}, m_skeleton_graph);
                remove_edge(*e0, m_skeleton_graph);
                remove_edge(*e1, m_skeleton_graph);
                remove_vertex(*v_root, m_skeleton_graph);

                vertex_mod_map[v0] = true;
                vertex_mod_map[v1] = true;
                std::tie(vi, vi_end) = vertices(m_skeleton_graph);
            }
            else {
                vertex_mod_map[*v_root] = true;
            }
        }
    }
}

template <typename TVertex, typename TEdge, typename TGraph> struct dfs_visitor : public boost::dfs_visitor<> {
    dfs_visitor(QVector<SkeletonEdge>& longest_path_) : longest_path(longest_path_) {}

    using VertexColorMap = std::map<SkeletonVertex, boost::default_color_type>;
    VertexColorMap vertex_coloring;

    using EdgeColorMap = std::map<SkeletonEdge, boost::default_color_type>;
    EdgeColorMap edge_coloring;

    void tree_edge(const TEdge& e, const TGraph& g) {
        TVertex source = boost::source(e, g), target = boost::target(e, g);

        predecessor_map[target] = predecessor_map[source];
        predecessor_map[target].push(source);

        if (typeid(g) == typeid(SkeletonSubgraph) && boost::degree(target, g) == 1) {
            getSimplePath(target, g);
        }
    }

    void back_edge(const TEdge& e, const TGraph& g) {
        TVertex source = boost::source(e, g), target = boost::target(e, g);

        predecessor_map[target] = predecessor_map[source];
        predecessor_map[target].push(source);

        getCycle(target, g);
    }

    void getCycle(const TVertex& v, const TGraph& g) {
        TVertex source = v, target;

        QVector<SkeletonEdge> cycle;
        Distance cycle_length = 0;
        while (predecessor_map[v].top() != v) {
            target = predecessor_map[v].pop();
            TEdge e = edge(source, target, g).first;
            cycle += e;
            cycle_length += g[e].length();
            source = target;
        }

        TEdge e = edge(source, v, g).first;
        cycle += e;
        cycle_length += g[e].length();

        if (cycle_length > longest_path_length) {
            longest_path = cycle;
            longest_path_length = cycle_length;
        }
    }

    void getSimplePath(const TVertex v, const TGraph& g) {
        TVertex source = v, target;

        QVector<SkeletonEdge> simple_path;
        Distance length = 0;
        while (!predecessor_map[v].isEmpty()) {
            target = predecessor_map[v].pop();
            TEdge e = edge(source, target, g).first;
            length += g[e].length();
            simple_path += e;
            source = target;
        }

        if (length > longest_path_length) {
            longest_path = simple_path;
            longest_path_length = length;
        }
    }

  private:
    QMap<SkeletonVertex, QStack<SkeletonVertex>> predecessor_map;
    QVector<SkeletonEdge>& longest_path;
    Distance longest_path_length = 0;
};

void Skeleton::extractCycles() {
    // Extract cycles in order of longest to shortest
    while (true) {
        QVector<SkeletonEdge> longest_cycle;
        dfs_visitor<SkeletonVertex, SkeletonEdge, SkeletonGraph> vis(longest_cycle);
        boost::undirected_dfs(m_skeleton_graph, vis, make_assoc_property_map(vis.vertex_coloring),
                              make_assoc_property_map(vis.edge_coloring));

        if (!longest_cycle.isEmpty()) {
            extractPath(longest_cycle);
        }
        else {
            break; // All cycles have been removed
        }

        // Check if cycle removal empties graph
        if (boost::num_edges(m_skeleton_graph) == 0 || boost::num_vertices(m_skeleton_graph) == 0) {
            break;
        }
    }
}

void Skeleton::extractSimplePaths() {
    //! Extract simple paths in order of longest to shortest
    while (boost::num_edges(m_skeleton_graph) > 0) {
        //! Create Vertex Index Map. Needed for SubGraph Map.
        std::map<SkeletonVertex, int> vertex_index_map;
        boost::associative_property_map<std::map<SkeletonVertex, int>> index_map(vertex_index_map);
        auto [v, v_end] = boost::vertices(m_skeleton_graph);
        for (int i = 0; v != v_end; ++v, ++i) {
            boost::put(index_map, *v, i);
        }

        //! Create SubGraph Map
        std::map<SkeletonVertex, int> vertex_subgraph_map;
        boost::associative_property_map<std::map<SkeletonVertex, int>> subgraph_map(vertex_subgraph_map);
        int number_of_subgraphs =
            boost::connected_components(m_skeleton_graph, subgraph_map, boost::vertex_index_map(index_map));

        //! Create SubGraph
        SkeletonSubgraph subgraph(m_skeleton_graph, boost::keep_all(),
                                  SkeletonSubgraphFilter(QMap<SkeletonVertex, int>(vertex_subgraph_map)));

        //! Find open edge to start dfs from
        SkeletonSubgraph::vertex_iterator vi, vi_end;
        boost::tie(vi, vi_end) = vertices(subgraph);
        SkeletonSubgraph::vertex_descriptor start = *vi;
        while (vi != vi_end) {
            if (boost::degree(*vi, subgraph) == 1) {
                start = *vi;
                break;
            }

            ++vi;
        }

        //! Collect longest simple path
        QVector<SkeletonEdge> longest_simple_path;
        dfs_visitor<SkeletonSubgraph::vertex_descriptor, SkeletonSubgraph::edge_descriptor, SkeletonSubgraph> vis(
            longest_simple_path);
        boost::undirected_dfs(subgraph, vis, make_assoc_property_map(vis.vertex_coloring),
                              make_assoc_property_map(vis.edge_coloring), start);

        if (!longest_simple_path.isEmpty()) {
            extractPath(longest_simple_path);
        }
    }
}

void Skeleton::extractPath(QVector<SkeletonEdge> path_) {
    Polyline path;

    SkeletonEdge e = path_.takeFirst();
    path << m_skeleton_graph[e.m_source] << m_skeleton_graph[e.m_target];
    remove_edge(e, m_skeleton_graph);

    //! Remove isolated vertices
    if (boost::degree(e.m_source, m_skeleton_graph) == 0) {
        remove_vertex(e.m_source, m_skeleton_graph);
    }
    if (boost::degree(e.m_target, m_skeleton_graph) == 0) {
        remove_vertex(e.m_target, m_skeleton_graph);
    }

    for (SkeletonEdge& e : path_) {
        path << m_skeleton_graph[e.m_target];
        remove_edge(e, m_skeleton_graph);

        //! Remove isolated vertices
        if (boost::degree(e.m_source, m_skeleton_graph) == 0) {
            remove_vertex(e.m_source, m_skeleton_graph);
        }
        if (boost::degree(e.m_target, m_skeleton_graph) == 0) {
            remove_vertex(e.m_target, m_skeleton_graph);
        }
    }

    m_computed_geometry.append(path);
}

void Skeleton::inspectSkeleton() {
    GeometryDebug::printDesmos(m_geometry, "Layer " + QString::number(m_layer_num) + " Skeleton Input Geometry");
    GeometryDebug::printDesmos(m_computed_geometry,
                               "Layer " + QString::number(m_layer_num) + " Skeleton Output Geometry");
}

void Skeleton::inspectSkeletonGraph() {
    GeometryDebug::printDesmos(m_geometry, "Layer " + QString::number(m_layer_num) + " Skeleton Input Geometry");

    GeometryDebug::EdgeList edge_list;
    edge_list.reserve(boost::num_edges(m_skeleton_graph));

    boost::graph_traits<SkeletonGraph>::edge_iterator edge_iter, edge_iter_end;
    boost::tie(edge_iter, edge_iter_end) = edges(m_skeleton_graph);
    while (edge_iter != edge_iter_end) {
        const SkeletonVertex source = boost::source(*edge_iter, m_skeleton_graph);
        const SkeletonVertex target = boost::target(*edge_iter, m_skeleton_graph);
        edge_list.append(QPair<Point, Point>(m_skeleton_graph[source], m_skeleton_graph[target]));
        ++edge_iter;
    }

    GeometryDebug::printDesmos(edge_list, "Layer " + QString::number(m_layer_num) + " Skeleton Graph");
}

void Skeleton::populateSegmentSettings(QSharedPointer<SettingsBase> segment_sb,
                                       const QSharedPointer<SettingsBase>& parent_sb, bool adapted,
                                       const Distance& adapted_width, const Velocity& adapted_speed) {
    // Populate segment settings with the provided settings base
    segment_sb->populate(parent_sb);

    // Set segment settings
    segment_sb->setSetting(SS::kWidth,
                           adapted ? adapted_width : parent_sb->setting<Distance>(PS::Skeleton::kBeadWidth));
    segment_sb->setSetting(SS::kSpeed, adapted ? adapted_speed : parent_sb->setting<Velocity>(PS::Skeleton::kSpeed));
    segment_sb->setSetting(SS::kHeight, parent_sb->setting<Distance>(PS::Layer::kLayerHeight));
    segment_sb->setSetting(SS::kAccel, parent_sb->setting<Acceleration>(PRS::Acceleration::kSkeleton));
    segment_sb->setSetting(SS::kExtruderSpeed, parent_sb->setting<AngularVelocity>(PS::Skeleton::kExtruderSpeed));
    segment_sb->setSetting(SS::kMaterialNumber, parent_sb->setting<int>(MS::MultiMaterial::kSkeletonNum));
    segment_sb->setSetting(SS::kRegionType, RegionType::kSkeleton);
    segment_sb->setSetting(SS::kAdapted, adapted);
}

LSegmentList Skeleton::createSegments(const Point& start, const Point& end,
                                      const QSharedPointer<SettingsBase>& parent_sb) const {
    LSegmentList segments;

    // ---------- Static bead width ----------
    if (!parent_sb->setting<bool>(PS::Skeleton::kSkeletonAdapt)) {
        LSegmentPtr segment = LSegmentPtr::create(start, end);
        populateSegmentSettings(segment->getSb(), parent_sb);
        segments.append(segment);
        return segments;
    }

    // ---------- Adaptive bead width ----------
    const Distance ref_width = parent_sb->setting<Distance>(PS::Skeleton::kBeadWidth);
    const Velocity ref_speed = parent_sb->setting<Velocity>(PS::Skeleton::kSpeed);
    const double speed_factor = ref_speed() * ref_width(); // Inverse-proportional speed factor
    const double min_speed = ref_speed() * 0.01;           // 1% of reference speed

    // Discretize the input segment
    const Distance step = parent_sb->setting<Distance>(PS::Skeleton::kSkeletonAdaptStepSize);
    const int steps = std::max(1, int(std::ceil(start.distance(end)() / step())));
    const double dx = (end.x() - start.x()) / steps;
    const double dy = (end.y() - start.y()) / steps;
    const double tol = ref_width() * 0.01; // Consolidation tolerance = 1% of reference bead width

    QVector<Point> pts;
    pts.reserve(steps + 1);
    QVector<Distance> bw;
    bw.reserve(steps + 1);

    for (int i = 0; i <= steps; ++i) {
        Point p(start.x() + i * dx, start.y() + i * dy, 0);

        // Nearest distance to border geometry
        double radius = std::numeric_limits<double>::max();
        for (const Polyline& edge : m_geometry.getEdges()) {
            radius = std::min(radius, MathUtils::nearestPointOnSegment(edge.first(), edge.last(), p).second);
        }
        pts.push_back(p);
        bw.push_back(radius * 2.0); // Bead width = 2 * radius
    }
    pts.back() = end; // Exact end-point

    // Helper lambda to create a subsegment with adapted settings
    auto createSubsegment = [&](size_t start_idx, size_t end_idx) {
        const double width =
            std::clamp(std::min(bw[start_idx](), bw[end_idx]()), 1e-9, std::numeric_limits<double>::max());
        const double speed = std::max(speed_factor / width, min_speed);

        // Create the subsegment and apply adapted settings
        LSegmentPtr segment = LSegmentPtr::create(pts[start_idx], pts[end_idx]);
        populateSegmentSettings(segment->getSb(), parent_sb, true, width, speed);

        return segment;
    };

    // Create and consolidate subsegments
    size_t start_idx = 0;
    for (size_t end_idx = 1; end_idx <= steps; ++end_idx) {
        const bool boundary = std::fabs(bw[end_idx]() - bw[start_idx]()) > tol;
        const bool last = (end_idx == steps);
        if (boundary || last) {
            segments.append(createSubsegment(start_idx, end_idx));
            start_idx = end_idx;
        }
    }

    return segments;
}

Path Skeleton::createPath(Polyline line) {
    // ---------- No Settings Regions ----------
    if (m_settings_polygons.isEmpty()) {
        Path path;

        for (size_t i = 0; i < line.size() - 1; ++i) {
            for (const LSegmentPtr& segment : createSegments(line[i], line[i + 1], m_sb)) {
                path.append(segment);
            }
        }
        return path;
    }

    // ---------- Settings Regions ----------
    return createPathWithLocalizedSettings(line);
}

Path Skeleton::createPathWithLocalizedSettings(const Polyline& line) {
    Path path;

    // Iterate through each segment of the polyline
    for (size_t i = 0; i < line.size() - 1; ++i) {
        const Point& start = line[i];
        const Point& end = line[i + 1];

        // Clip the segment against the settings polygons
        QVector<Point> cuts;
        for (const SettingsPolygon& polygon : m_settings_polygons) {
            cuts += polygon.clipLine(start, end);
        }

        // Sort cuts based on their distance from the start point
        std::sort(cuts.begin(), cuts.end(),
                  [start](const Point& a, const Point& b) { return start.distance(a) < start.distance(b); });

        // Create an ordered list of points including start, cuts, and end
        QVector<Point> points;
        points << start << cuts << end;

        // Assemble subsegments from the points and apply regional settings
        for (size_t j = 0; j + 1 < points.size(); ++j) {
            const Point& p0 = points[j];
            const Point& p1 = points[j + 1];
            const Point mid = (p0 + p1) * 0.5;

            // Assign the subsegment default settings from the main settings base
            QSharedPointer<SettingsBase> parent_sb = QSharedPointer<SettingsBase>::create(*m_sb);

            // Populate the subsegment settings with local settings
            for (const SettingsPolygon& polygon : m_settings_polygons) {
                if (polygon.inside(mid)) {
                    parent_sb->populate(polygon.getSettings());
                    break;
                }
            }

            for (const LSegmentPtr& segment : createSegments(p0, p1, parent_sb)) {
                path.append(segment);
            }
        }
    }

    return path;
}

QVector<Path> Skeleton::filterPath(const Path& path) {
    QVector<Path> filtered_paths;
    Path filtered_path;

    const Distance& min_length = m_sb->setting<Distance>(PS::Skeleton::kMinPathLength);

    // Helper lambda to flush the current filtered path
    auto flushPath = [&]() {
        if (filtered_path.size() > 0) {
            // Discard paths that do not meet the minimum length requirement
            if (filtered_path.calculateLength() < min_length) {
                filtered_path.clear();
                return;
            }

            // Ensure closed paths have correct orientation
            if (filtered_path.isClosed()) {
                filtered_path.setCCW(Polygon(filtered_path).orientation());
            }
            filtered_paths.append(filtered_path);
            filtered_path.clear();
        }
    };

    // Filter each segment in the path
    for (const auto& segment : path) {
        auto segment_sb = segment->getSb();

        // If the segment is not adapted, simply append it to the filtered path
        if (!segment_sb->setting<bool>(SS::kAdapted)) {
            filtered_path.append(segment);
            continue;
        }

        // Retrieve adapted width limits
        const Distance min_width = segment_sb->setting<Distance>(PS::Skeleton::kSkeletonAdaptMinWidth);
        const Distance max_width = segment_sb->setting<Distance>(PS::Skeleton::kSkeletonAdaptMaxWidth);

        // Compute adapted speed limits based on min/max widths
        const Distance ref_width = segment_sb->setting<Distance>(PS::Skeleton::kBeadWidth);
        const Velocity ref_speed = segment_sb->setting<Velocity>(PS::Skeleton::kSpeed);
        const double speed_factor = ref_speed() * ref_width(); // Inverse-proportional speed factor
        const Velocity min_speed = speed_factor / min_width();
        const Velocity max_speed = speed_factor / max_width();

        // Retrieve the segment's width and filters
        const Distance width = segment_sb->setting<Distance>(SS::kWidth);
        const SkeletonFilter min_filter =
            segment_sb->setting<SkeletonFilter>(PS::Skeleton::kSkeletonAdaptMinWidthFilter);
        const SkeletonFilter max_filter =
            segment_sb->setting<SkeletonFilter>(PS::Skeleton::kSkeletonAdaptMaxWidthFilter);

        // Apply filtering logic
        if (width >= min_width && width <= max_width) { // Within bounds
            filtered_path.append(segment);
        }
        else if (width < min_width && min_filter == SkeletonFilter::kClamp) { // Below minimum width
            segment_sb->setSetting(SS::kWidth, min_width);
            segment_sb->setSetting(SS::kSpeed, min_speed);
            filtered_path.append(segment);
        }
        else if (width > max_width && max_filter == SkeletonFilter::kClamp) { // Above maximum width
            segment_sb->setSetting(SS::kWidth, max_width);
            segment_sb->setSetting(SS::kSpeed, max_speed);
            filtered_path.append(segment);
        }
        else { // Outside bounds and not clamped
            flushPath();
        }
    }

    flushPath(); // Push any trailing path
    return filtered_paths;
}

void Skeleton::setAnchorWireFeed(QVector<Polyline> anchor_lines) { m_computed_anchor_lines = anchor_lines; }

void Skeleton::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                        QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) {
    PolylineOrderOptimizer poo(current_location, layerNumber);

    PathOrderOptimization pathOrderOptimization =
        static_cast<PathOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPathOrder));
    if (pathOrderOptimization == PathOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPathXLocation),
                            getSb()->setting<double>(PS::Optimizations::kCustomPathYLocation));

        poo.setStartOverride(startOverride);
    }

    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPointOrder));

    if (pointOrderOptimization == PointOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPointXLocation),
                            getSb()->setting<double>(PS::Optimizations::kCustomPointYLocation));

        poo.setStartPointOverride(startOverride);
    }

    //! Uncomment if erroneous skeletons are being generated outside geometry
    if (!outerMostClosedContour.isEmpty()) {
        PolygonList _outerMostClosedContour;
        for (const Path& path : outerMostClosedContour) {
            _outerMostClosedContour += Polygon(path);
        }

        //! Removes skeletons generated outside outerMostClosedContour
        QVector<Polyline> containedPaths;
        for (Polyline line : m_computed_geometry) {
            if (std::all_of(line.begin(), line.end(), [_outerMostClosedContour](const Point& pt) mutable {
                    return _outerMostClosedContour.inside(pt);
                })) {
                containedPaths += line;
            }
        }
        m_computed_geometry = containedPaths;
    }

    poo.setPointParameters(pointOrderOptimization, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                           getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                           getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                           getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                           getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius));

    poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kSkeleton,
                              static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline();
        if (result.size() > 0) {
            QVector<Path> paths = filterPath(createPath(result));

            if (paths.size() > 0) {
                for (Path path : paths) {
                    QVector<Path> temp_path;
                    calculateModifiers(path, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3), temp_path);
                    PathModifierGenerator::GenerateTravel(path, current_location,
                                                          m_sb->setting<Velocity>(PS::Travel::kSpeed));
                    current_location = path.back()->end();
                    m_paths.push_back(path);
                }
            }
        }
    }
}

void Skeleton::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    // Ramping
    if (m_sb->setting<bool>(ES::Ramping::kTrajectoryAngleEnabled)) {
        PathModifierGenerator::GenerateTrajectorySlowdown(path, m_sb);
    }

    // Slowdown
    if (m_sb->setting<bool>(MS::Slowdown::kSkeletonEnable)) {
        PathModifierGenerator::GenerateSlowdown(path, m_sb->setting<Distance>(MS::Slowdown::kSkeletonDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kSkeletonLiftDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kSkeletonCutoffDistance),
                                                m_sb->setting<Velocity>(MS::Slowdown::kSkeletonSpeed),
                                                m_sb->setting<AngularVelocity>(MS::Slowdown::kSkeletonExtruderSpeed),
                                                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                                                m_sb->setting<double>(MS::Slowdown::kSlowDownAreaModifier));
    }

    // Tip Wipe
    if (m_sb->setting<bool>(MS::TipWipe::kSkeletonEnable)) {
        const auto& tw_direction = static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kSkeletonDirection));
        const auto& tw_distance = m_sb->setting<Distance>(MS::TipWipe::kSkeletonDistance);
        const auto& tw_speed = m_sb->setting<Velocity>(MS::TipWipe::kSkeletonSpeed);
        const auto& tw_extruder_speed = m_sb->setting<AngularVelocity>(MS::TipWipe::kSkeletonExtruderSpeed);
        const auto& tw_angle = m_sb->setting<Angle>(MS::TipWipe::kSkeletonAngle);
        const auto& tw_lift_height = m_sb->setting<Distance>(MS::TipWipe::kSkeletonLiftHeight);
        const auto& tw_cutoff_distance = m_sb->setting<Distance>(MS::TipWipe::kSkeletonCutoffDistance);

        if (tw_direction == TipWipeDirection::kForward || tw_direction == TipWipeDirection::kOptimal) {
            PathModifierGenerator::GenerateForwardTipWipeOpenLoop(path, PathModifiers::kForwardTipWipe, tw_distance,
                                                                  tw_speed, tw_extruder_speed, tw_lift_height,
                                                                  tw_cutoff_distance);
        }
        else if (tw_direction == TipWipeDirection::kAngled) {
            PathModifierGenerator::GenerateTipWipe(path, PathModifiers::kAngledTipWipe, tw_distance, tw_speed, tw_angle,
                                                   tw_extruder_speed, tw_lift_height, tw_cutoff_distance);
        }
        else {
            PathModifierGenerator::GenerateTipWipe(path, PathModifiers::kReverseTipWipe, tw_distance, tw_speed,
                                                   tw_angle, tw_extruder_speed, tw_lift_height, tw_cutoff_distance);
        }
    }

    // Startup
    if (m_sb->setting<bool>(MS::Startup::kSkeletonEnable)) {
        const auto& su_distance = m_sb->setting<Distance>(MS::Startup::kSkeletonDistance);
        const auto& su_speed = m_sb->setting<Velocity>(MS::Startup::kSkeletonSpeed);
        const auto& speed = m_sb->setting<Velocity>(PS::Skeleton::kSpeed);
        const auto& su_extruder_speed = m_sb->setting<AngularVelocity>(MS::Startup::kSkeletonExtruderSpeed);
        const auto& extruder_speed = m_sb->setting<AngularVelocity>(PS::Skeleton::kExtruderSpeed);
        const auto& su_steps = m_sb->setting<int>(MS::Startup::kSkeletonSteps);
        const auto& sm_enable_width_height = m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight);
        const auto& su_area_modifier = m_sb->setting<double>(MS::Startup::kStartUpAreaModifier);

        if (m_sb->setting<bool>(MS::Startup::kSkeletonRampUpEnable)) {
            PathModifierGenerator::GenerateInitialStartupWithRampUp(path, su_distance, su_speed, speed,
                                                                    su_extruder_speed, extruder_speed, su_steps,
                                                                    sm_enable_width_height, su_area_modifier);
        }
        else {
            PathModifierGenerator::GenerateInitialStartup(path, su_distance, su_speed, su_extruder_speed,
                                                          sm_enable_width_height, su_area_modifier);
        }
    }

    // Lead In
    if (m_sb->setting<bool>(PS::Skeleton::kLeadInEnable)) {
        const auto& li_distance = m_sb->setting<Distance>(PS::Skeleton::kLeadInDistance);
        const auto& li_speed = m_sb->setting<Velocity>(PS::Skeleton::kLeadInSpeed);
        const auto& li_extruder_speed = m_sb->setting<AngularVelocity>(PS::Skeleton::kLeadInExtruderSpeed);
        const auto& li_enable_width_height = m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight);
        const auto& li_area_modifier = m_sb->setting<double>(PS::Skeleton::kLeadInAreaModifier);
        PathModifierGenerator::GenerateOpenLoopLeadIn(path, li_distance, li_speed, li_extruder_speed,
                                                        li_enable_width_height, li_area_modifier);
    }
}
} // namespace ORNL
