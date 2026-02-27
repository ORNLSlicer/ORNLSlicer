#include "optimizers/polyline_order_optimizer.h"

#include "QRandomGenerator"
#include "geometry/polygon_list.h"
#include "optimizers/point_order_optimizer.h"
#include "utilities/mathutils.h"

namespace ORNL {
PolylineOrderOptimizer::PolylineOrderOptimizer(Point& start, uint layer_number)
    : m_current_location(start), m_layer_number(layer_number), m_override_used(false) {
    m_layer_num = layer_number;
}

int PolylineOrderOptimizer::getCurrentPolylineCount() { return m_polylines.size(); }

void PolylineOrderOptimizer::setGeometryToEvaluate(QVector<Polyline> polylines, RegionType type,
                                                   PathOrderOptimization optimization) {
    m_polylines = polylines;
    m_current_region_type = type;
    m_optimization = optimization;
    m_has_computed_heirarchy = false;
    m_topo_level = 0;
}

void PolylineOrderOptimizer::setInfillParameters(InfillPatterns infillPattern, PolygonList border_geometry,
                                                 Distance minInfillPathDistance, Distance minTravelDistance,
                                                 bool enable_partitioned_linking) {
    m_pattern = infillPattern;
    m_border_geometry = border_geometry;
    m_min_distance = minInfillPathDistance;
    m_min_travel_distance = minTravelDistance;
    m_enable_partitioned_linking = enable_partitioned_linking;
}

void PolylineOrderOptimizer::setPointParameters(PointOrderOptimization pointOptimization, bool minDistanceEnable,
                                                Distance minDistanceThreshold, Distance consecutiveThreshold,
                                                bool randomnessEnable, Distance randomnessRadius) {
    m_point_optimization = pointOptimization;
    m_min_point_distance_enable = minDistanceEnable;
    m_min_point_distance = minDistanceThreshold;
    m_consecutive_threshold = consecutiveThreshold;
    m_randomness_enable = randomnessEnable;
    m_randomness_radius = randomnessRadius;
}

void PolylineOrderOptimizer::setStartOverride(Point pt) {
    m_override_location = pt;
    m_override_used = true;
}

void PolylineOrderOptimizer::setStartPointOverride(Point pt) { m_point_override_location = pt; }

Polyline PolylineOrderOptimizer::linkNextPolyline(QVector<Polyline> polylines) {
    if (m_polylines.size() > 0) {
        switch (m_current_region_type) {
            case RegionType::kInfill:
            case RegionType::kSkin:
                return linkNextInfillPolyline(polylines);
                break;

            case RegionType::kSkeleton:
                return linkNextSkeletonPolyline();
                break;

            default:
                return linkTo();
                break;
        }
    }

    return Polyline();
}

Polyline PolylineOrderOptimizer::linkNextInfillPolyline(QVector<Polyline>& polylines) {
    Polyline nextPolyline;
    switch (m_pattern) {
        case InfillPatterns::kLines:
            nextPolyline = linkNextInfillLines(polylines);
            break;
        case InfillPatterns::kGrid:
            nextPolyline = linkNextInfillLines(polylines);
            break;
        case InfillPatterns::kConcentric:
        case InfillPatterns::kInsideOutConcentric:
            nextPolyline = linkNextInfillConcentric();
            break;
        case InfillPatterns::kTriangles:
            nextPolyline = linkNextInfillLines(polylines);
            break;
        case InfillPatterns::kHexagonsAndTriangles:
            nextPolyline = linkNextInfillLines(polylines);
            break;
        case InfillPatterns::kHoneycomb:
            nextPolyline = linkNextInfillLines(polylines);
            break;
        case InfillPatterns::kRadialHatch:
            nextPolyline = linkNextInfillConcentric();
            break;
        default:
            nextPolyline = linkNextInfillLines(polylines);
            break;
    }

    if (nextPolyline.length() < m_min_distance)
        nextPolyline.clear();

    return nextPolyline;
}

Polyline PolylineOrderOptimizer::linkNextInfillLines(QVector<Polyline>& polylines) {
    Polyline new_polyline;
    QVector<Polyline> empty_polylines;
    PolygonList empty_polygon_list;

    Point temp_current_location = m_current_location;

    // first motion will always be a travel, so add it regardless
    if (m_polylines.size() > 0) {
        int index;
        bool start;

        if (m_enable_partitioned_linking) {
            std::tie(index, start) = closestOpenPolyline(m_polylines, temp_current_location);
        }
        else {
            // Monotonic linking
            int endpoint_index;
            std::tie(endpoint_index, start) =
                closestOpenPolyline({m_polylines.front(), m_polylines.back()}, temp_current_location);
            index = (endpoint_index == 0) ? 0 : (m_polylines.size() - 1);
        }

        // if false, indicates index is closest if you start at the end point, so reverse
        if (start == false) {
            m_polylines[index] = m_polylines[index].reverse();
        }

        new_polyline += m_polylines[index];
        temp_current_location = new_polyline.back();
        m_polylines.remove(index);
    }

    // add as many motions without a travel as possible into one polyline
    for (int i = 0, end = m_polylines.size(); i < end; ++i) {
        if (m_polylines.size() > 0) {
            const auto& [index, start] = closestOpenPolyline(m_polylines, temp_current_location);

            // if false, indicates index is closest if you start at the end point, so reverse
            if (start == false) {
                m_polylines[index] = m_polylines[index].reverse();
            }

            Point link_start = temp_current_location;
            Point link_end = m_polylines[index].front();

            // Link must not intersect border geometry, remaining polylines, previously created/optimized polylines, or
            // currently constructed polylines AND must be shorter than travel distance Otherwise, a travel must be used
            if (!(linkIntersects(link_start, link_end, empty_polylines, m_border_geometry) ||
                  linkIntersects(link_start, link_end, m_polylines, empty_polygon_list) ||
                  linkIntersects(link_start, link_end, polylines, empty_polygon_list) ||
                  linkIntersects(link_start, link_end, QVector<Polyline> {new_polyline}, empty_polygon_list)) &&
                link_start.distance(link_end) < m_min_travel_distance) {
                new_polyline += m_polylines[index];
                temp_current_location = new_polyline.back();
                m_polylines.remove(index);
                i = 0;
            }
        }
    }

    // Determine which end of infill path should be the start
    if (!new_polyline.empty() && m_point_override_location.distance(new_polyline.front()) >
                                     m_point_override_location.distance(new_polyline.back())) {
        new_polyline = new_polyline.reverse();
    }

    return new_polyline;
}

Polyline PolylineOrderOptimizer::linkNextInfillConcentric() {
    Polyline new_polyline;

    //! \note Future work: travels aren't always needed between concentric Polylines, only needed when link distance is
    //! longer than \note travel distance or when link/travel segment crosses Polylines or border geometry
    if (m_polylines.size() > 0)
        new_polyline = linkTo();

    return new_polyline;
}

Polyline PolylineOrderOptimizer::linkNextSkeletonPolyline() {
    Polyline new_polyline;
    if (!m_polylines.isEmpty()) {
        const auto& [index, start] = closestOpenPolyline(m_polylines, m_current_location);

        new_polyline = m_polylines[index];
        if (!start) {
            new_polyline = new_polyline.reverse();
        }
        m_polylines.remove(index);
    }

    Point queryPoint;
    if (m_point_optimization == PointOrderOptimization::kCustomPoint)
        queryPoint = m_point_override_location;
    else
        queryPoint = m_current_location;

    // If polyline is closed loop, apply point optimization strategies for a closed-loop path not a skeleton
    if (new_polyline.front() == new_polyline.back()) {
        // Remove last element because it is a duplicate of the first. It will be re-added after re-ordering the vector.
        new_polyline.removeLast();

        // Find index of the point that needs to be first.
        int pointIndex = PointOrderOptimizer::linkToPoint(
            queryPoint, new_polyline, m_layer_num, m_point_optimization, m_min_point_distance_enable,
            m_min_point_distance, m_consecutive_threshold, m_randomness_enable, m_randomness_radius);

        // Rotate the order of points to get the proper point at the start of the path
        for (int i = 0; i < pointIndex; ++i)
            new_polyline.move(0, new_polyline.size() - 1);

        // Re-add the first point to the end to close the loop.
        new_polyline.push_back(new_polyline.front());
    }
    // For open loop skeletons, check with point order optimizer to determine which end of the skeleton should be the
    // start point
    else if (PointOrderOptimizer::findSkeletonPointOrder(queryPoint, new_polyline, m_point_optimization,
                                                         m_min_point_distance_enable, m_min_point_distance))
        new_polyline = new_polyline.reverse();

    return new_polyline;
}

bool PolylineOrderOptimizer::linkIntersects(Point link_start, Point link_end, QVector<Polyline> infill_geometry,
                                            PolygonList border_geometry) {
    //! Check for possible intersections with infill geometry
    for (Polyline polyline : infill_geometry) {
        for (int i = 0, end = polyline.size() - 1; i < end; ++i) {
            Point startPt = polyline[i];
            Point endPt = polyline[(i + 1) % polyline.size()];

            if (link_start != startPt && link_start != endPt && link_end != startPt && link_end != endPt) {
                if (MathUtils::intersect(link_start, link_end, startPt, endPt)) {
                    return true;
                }
            }
        }
    }

    //! Check for possible intersections with border geometry
    for (Polygon poly : border_geometry) {
        for (int i = 0, end = poly.size() - 1; i < end; ++i) {
            if (MathUtils::intersect(link_start, link_end, poly[i], poly[i + 1])) {
                return true;
            }
        }

        //! Check last line of polygon
        if (MathUtils::intersect(link_start, link_end, poly.last(), poly.first())) {
            return true;
        }
    }

    return false;
}

QPair<int, bool> PolylineOrderOptimizer::closestOpenPolyline(QVector<Polyline> polylines, Point currentLocation) {
    Distance shortest = Distance(std::numeric_limits<float>::max());

    int index = 0;
    bool start;

    Point queryPoint = currentLocation;

    for (int i = 0, end = polylines.size(); i < end; ++i) {
        if (queryPoint.distance(polylines[i].front()) < shortest) {
            shortest = queryPoint.distance(polylines[i].front());
            index = i;
            start = true;
        }

        if (queryPoint.distance(polylines[i].back()) < shortest) {
            shortest = queryPoint.distance(polylines[i].back());
            index = i;
            start = false;
        }
    }
    return QPair<int, bool>(index, start);
}

Polyline PolylineOrderOptimizer::linkTo() {
    int polylineIndex;
    switch (m_optimization) {
        case PathOrderOptimization::kNextClosest:
            polylineIndex = findShortestOrLongestDistance();
            break;
        case PathOrderOptimization::kNextFarthest:
            polylineIndex = findShortestOrLongestDistance(false);
            break;
        case PathOrderOptimization::kRandom:
            polylineIndex = linkToRandom();
            break;
        case PathOrderOptimization::kOutsideIn:
            polylineIndex = findInteriorExterior();
            break;
        case PathOrderOptimization::kInsideOut:
            polylineIndex = findInteriorExterior(false);
            break;
        default:
            polylineIndex = findShortestOrLongestDistance();
            break;
    }

    Polyline nextPolyline = m_polylines[polylineIndex];
    m_polylines.removeAt(polylineIndex);

    Point queryPoint;
    if (m_point_optimization == PointOrderOptimization::kCustomPoint)
        queryPoint = m_point_override_location;
    else
        queryPoint = m_current_location;

    int pointIndex = PointOrderOptimizer::linkToPoint(
        queryPoint, nextPolyline, m_layer_num, m_point_optimization, m_min_point_distance_enable, m_min_point_distance,
        m_consecutive_threshold, m_randomness_enable, m_randomness_radius);

    for (int i = 0; i < pointIndex; ++i)
        nextPolyline.move(0, nextPolyline.size() - 1);

    return nextPolyline;
}

int PolylineOrderOptimizer::findShortestOrLongestDistance(bool shortest) {
    Point queryPoint;
    if (m_override_used)
        queryPoint = m_override_location;
    else
        queryPoint = m_current_location;

    int polylineIndex;
    Distance closest;
    if (shortest)
        closest = Distance(DBL_MAX);

    for (int i = 0, end = m_polylines.size(); i < end; ++i) {
        for (int j = 0, end2 = m_polylines[i].size() - 1; j < end2; ++j) {
            Distance closestSegment =
                MathUtils::distanceFromLineSegSqrd(queryPoint, m_polylines[i][j], m_polylines[i][j + 1]);
            if (shortest) {
                if (closestSegment < closest) {
                    closest = closestSegment;
                    polylineIndex = i;
                }
            }
            else {
                if (closestSegment > closest) {
                    closest = closestSegment;
                    polylineIndex = i;
                }
            }
        }
    }

    return polylineIndex;
}

int PolylineOrderOptimizer::linkToRandom() { return QRandomGenerator::global()->bounded(m_polylines.size()); }

int PolylineOrderOptimizer::findInteriorExterior(bool ExtToInt) {
    if (!m_has_computed_heirarchy && m_polylines.size() > 0) {
        QSharedPointer<TopologicalNode> root = computeTopologicalHeirarchy();
        // inOrderTraversal(root);
        levelOrder(root);
        if (!ExtToInt) {
            std::reverse(m_topo_order.begin(), m_topo_order.end());
            for (QVector<int>& level : m_topo_order) {
                std::reverse(level.begin(), level.end());
            }
        }

        m_has_computed_heirarchy = true;
    }

    int result = m_topo_order.first().first();
    m_topo_order.first().pop_front();
    if (m_topo_order.first().size() == 0)
        m_topo_order.pop_front();

    for (QVector<int>& level : m_topo_order)
        for (int& element : level)
            if (element > result)
                --element;

    return result;
}

QSharedPointer<PolylineOrderOptimizer::TopologicalNode> PolylineOrderOptimizer::computeTopologicalHeirarchy() {
    QVector<QSharedPointer<TopologicalNode>> all_nodes;
    all_nodes.reserve(m_polylines.size());

    for (int i = 0, end = m_polylines.size(); i < end; ++i)
        all_nodes.push_back(QSharedPointer<TopologicalNode>::create(TopologicalNode(i, m_polylines[i])));

    // assume first Polyline is outer contour
    QSharedPointer<TopologicalNode> root;
    root = all_nodes[0];
    all_nodes.removeFirst();

    for (QSharedPointer<TopologicalNode> node : all_nodes)
        insert(root, node);

    return root;
}

void PolylineOrderOptimizer::insert(QSharedPointer<TopologicalNode> root, QSharedPointer<TopologicalNode> current) {
    if (root->m_children.size() == 0) {
        root->m_children.push_back(current);
        return;
    }
    bool insideAny = false;
    for (QSharedPointer<TopologicalNode> child : root->m_children) {
        if (child->m_poly.inside(current->m_poly[0])) {
            insert(child, current);
            insideAny = true;
            break;
        }
    }

    if (!insideAny) {
        for (int j = root->m_children.size() - 1; j >= 0; --j) {
            if (current->m_poly.inside(root->m_children[j]->m_poly[0])) {
                current->m_children.push_back(root->m_children[j]);
                root->m_children.removeAt(j);
            }
        }
        root->m_children.push_back(current);
    }
}

void PolylineOrderOptimizer::levelOrder(QSharedPointer<TopologicalNode> root) {
    if (m_topo_order.size() == m_topo_level)
        m_topo_order.push_back({root->m_Polyline_index});
    else
        m_topo_order[m_topo_level].push_back(root->m_Polyline_index);

    ++m_topo_level;

    for (QSharedPointer<TopologicalNode> n : root->m_children)
        levelOrder(n);

    --m_topo_level;
}

Polyline PolylineOrderOptimizer::linkSpiralPolyline2D(bool last_spiral, Distance layerHeight,
                                                      PointOrderOptimization pointOrder) {
    int polylineIndex = findShortestOrLongestDistance();
    Polyline newPolyline = m_polylines[polylineIndex];
    m_polylines.removeAt(polylineIndex);
    int pointIndex = PointOrderOptimizer::linkToPoint(m_current_location, newPolyline, m_layer_num,
                                                      PointOrderOptimization::kNextClosest, false, 0, 0, false, 0);

    // Define which point to start layer one - all other layers must use next closest
    if (m_layer_num == 0) {
        if (pointOrder == PointOrderOptimization::kCustomPoint) {
            Point startOverride = m_point_override_location;
            pointIndex = PointOrderOptimizer::linkToPoint(startOverride, newPolyline, m_layer_num, pointOrder, false, 0,
                                                          0, false, 0);
        }
        else {
            pointIndex = PointOrderOptimizer::linkToPoint(m_current_location, newPolyline, m_layer_num, pointOrder,
                                                          false, 0, 0, false, 0);
        }
    }

    for (int i = 0; i < pointIndex; ++i)
        newPolyline.move(0, newPolyline.size() - 1);

    newPolyline.push_back(newPolyline.first());
    Distance polylineLength = newPolyline.length();

    Distance currentLength;
    Point temp_current_location = newPolyline[0];

    for (int start = 0, end = newPolyline.size(); start < end; ++start) {
        Point& pt = newPolyline[start];
        currentLength += pt.distance(temp_current_location);
        newPolyline[start].z(newPolyline[start].z() + ((currentLength / polylineLength) * layerHeight));

        temp_current_location.x(pt.x());
        temp_current_location.y(pt.y());
    }
    return newPolyline;
}
} // namespace ORNL
