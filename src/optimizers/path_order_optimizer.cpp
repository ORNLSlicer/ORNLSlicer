#include "optimizers/path_order_optimizer.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>

#include <QRandomGenerator>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segment_base.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "optimizers/point_order_optimizer.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
PathOrderOptimizer::PathOrderOptimizer(Point& start, uint layer_number, const QSharedPointer<SettingsBase>& sb)
    : m_current_location(start), m_layer_number(layer_number), m_sb(sb), m_override_used(false),
      m_point_override_used(false) {
    m_layer_num = layer_number;
}

Point& PathOrderOptimizer::getCurrentLocation() { return m_current_location; }

int PathOrderOptimizer::getCurrentPathCount() { return m_paths.size(); }

void PathOrderOptimizer::setPathsToEvaluate(QVector<Path> paths) {
    m_paths = paths;
    for (Path& path : m_paths)
        path.removeTravels();

    if (paths.size() > 0)
        m_current_region_type = paths.front().getSegments().front()->getSb()->setting<RegionType>(SS::kRegionType);
    else
        m_current_region_type = RegionType::kUnknown;

    m_has_computed_heirarchy = false;
    m_topo_level = 0;
}

void PathOrderOptimizer::setParameters(InfillPatterns infillPattern, PolygonList border_geometry) {
    m_pattern = infillPattern;
    m_border_geometry = border_geometry;
}

void PathOrderOptimizer::setParameters(PolygonList previousIslands) {}

void PathOrderOptimizer::setStartOverride(Point pt) {
    m_override_location = pt;
    m_override_used = true;
}

void PathOrderOptimizer::setStartPointOverride(Point pt) {
    m_point_override_location = pt;
    m_point_override_used = true;
}

Path PathOrderOptimizer::linkNextPath(QVector<Path> paths) {
    if (m_paths.size() > 0) {
        switch (m_current_region_type) {
            case RegionType::kInfill:
            case RegionType::kSkin:
                return linkNextInfillPath(paths);
                break;

            case RegionType::kSkeleton:
                return linkNextSkeletonPath();
                break;

            default:
                return linkTo();
                break;
        }
    }
    return Path();
}

Path PathOrderOptimizer::linkNextInfillPath(QVector<Path>& paths) {
    Point savedLocation = m_current_location;

    Path nextPath;
    switch (m_pattern) {
        case InfillPatterns::kLines:
            nextPath = linkNextInfillLines(paths);
            break;
        case InfillPatterns::kGrid:
            nextPath = linkNextInfillLines(paths);
            break;
        case InfillPatterns::kConcentric:
            nextPath = linkNextInfillConcentric();
            break;
        case InfillPatterns::kTriangles:
            nextPath = linkNextInfillLines(paths);
            break;
        case InfillPatterns::kHexagonsAndTriangles:
            nextPath = linkNextInfillLines(paths);
            break;
        case InfillPatterns::kHoneycomb:
            nextPath = linkNextInfillLines(paths);
            break;
        case InfillPatterns::kRadialHatch:
            nextPath = linkNextInfillConcentric();
            break;
        default:
            nextPath = linkNextInfillLines(paths);
            break;
    }

    Distance minDist;
    if (nextPath.back()->getSb()->setting<RegionType>(SS::kRegionType) == RegionType::kInfill)
        minDist = m_sb->setting<Distance>(PS::Infill::kMinPathLength);
    else if (nextPath.back()->getSb()->setting<RegionType>(SS::kRegionType) == RegionType::kSkin)
        minDist = m_sb->setting<Distance>(PS::Skin::kMinPathLength);

    if (nextPath.calculateLengthNoTravel() < minDist)
        nextPath.clear();

    // if there are no segments reset start location
    if (nextPath.size() > 0)
        m_current_location = nextPath.back()->end();
    else
        m_current_location = savedLocation;

    return nextPath;
}

Path PathOrderOptimizer::linkNextInfillLines(QVector<Path>& paths) {
    //! Gather settings for line segment links
    Distance bead_width = m_paths.front().front()->getSb()->setting<Distance>(SS::kWidth);
    Distance layer_height = m_paths.front().front()->getSb()->setting<Distance>(SS::kHeight);
    Velocity speed = m_paths.front().front()->getSb()->setting<Velocity>(SS::kSpeed);
    Acceleration acceleration = m_paths.front().front()->getSb()->setting<Acceleration>(SS::kAccel);
    AngularVelocity extruder_speed = m_paths.front().front()->getSb()->setting<AngularVelocity>(SS::kExtruderSpeed);

    Path new_path;
    QPair<int, bool> indexAndStart = closestOpenPath(m_paths);
    int index = indexAndStart.first;

    // if false, indicates index is closest if you start at the end point, so reverse
    if (indexAndStart.second == false)
        m_paths[index].reverseSegments();

    QVector<Path> empty_paths;
    PolygonList empty_polygon_list;

    QSharedPointer<TravelSegment> travel_segment =
        QSharedPointer<TravelSegment>::create(m_current_location, m_paths[index].front()->start());

    Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
    new_path.append(travel_segment);

    for (QSharedPointer<SegmentBase> seg : m_paths[index])
        new_path.append(seg);

    m_current_location = new_path.back()->end();
    m_paths.remove(index);

    const Distance min_travel_distance = m_sb->setting<Distance>(PS::Travel::kInfillMinLength);
    if (min_travel_distance > 0) {
        for (int i = 0, end = m_paths.size(); i < end; ++i) {
            if (m_paths.size() > 0) {
                indexAndStart = closestOpenPath(m_paths);
                index = indexAndStart.first;
                // if false, indicates index is closest if you start at the end point, so reverse
                if (indexAndStart.second == false)
                    m_paths[index].reverseSegments();

                Point link_start = m_current_location;
                Point link_end = m_paths[index].front()->start();
                Distance link_distance = link_start.distance(link_end);

                // If link intersects the border geometry, always travel. If link intersects the infill/skin paths,
                // check versus minimum travel distance to see if travel or link is needed.

                if (link_distance < min_travel_distance &&
                    !(linkIntersects(link_start, link_end, empty_paths, m_border_geometry) ||
                      linkIntersects(link_start, link_end, m_paths, empty_polygon_list) ||
                      linkIntersects(link_start, link_end, paths, empty_polygon_list) ||
                      linkIntersects(link_start, link_end, QVector<Path> {new_path}, empty_polygon_list))) {
                    QSharedPointer<LineSegment> line_segment =
                        QSharedPointer<LineSegment>::create(link_start, link_end);

                    line_segment->getSb()->setSetting(SS::kWidth, bead_width);
                    line_segment->getSb()->setSetting(SS::kHeight, layer_height);
                    line_segment->getSb()->setSetting(SS::kSpeed, speed);
                    line_segment->getSb()->setSetting(SS::kAccel, acceleration);
                    line_segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
                    line_segment->getSb()->setSetting(
                        SS::kMaterialNumber, m_paths[index].front()->getSb()->setting<int>(SS::kMaterialNumber));
                    line_segment->getSb()->setSetting(
                        SS::kRegionType, m_paths[index].front()->getSb()->setting<RegionType>(SS::kRegionType));

                    new_path.append(line_segment);

                    for (QSharedPointer<SegmentBase> seg : m_paths[index])
                        new_path.append(seg);

                    m_current_location = new_path.back()->end();
                    m_paths.remove(index);

                    i = 0;
                }
            }
        }
    }

    return new_path;
}

Path PathOrderOptimizer::linkNextInfillConcentric() {
    Path new_path;

    //! \note Future work: travels aren't always needed between concentric paths, only needed when link distance is
    //! longer than \note travel distance or when link/travel segment crosses paths or border geometry
    if (m_paths.size() > 0)
        new_path = linkTo();

    return new_path;
}

Path PathOrderOptimizer::linkNextSkeletonPath() {
    Path new_path;
    if (!m_paths.isEmpty()) {
        QPair<int, bool> location = closestOpenPath(m_paths);
        int index = location.first;
        bool start = location.second;

        new_path.setCCW(m_paths[index].getCCW());

        if (start) {
            QSharedPointer<TravelSegment> travel_segment =
                QSharedPointer<TravelSegment>::create(m_current_location, m_paths[index].front()->start());
            Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
            travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
            new_path.append(travel_segment);

            for (QSharedPointer<SegmentBase> seg : m_paths[index])
                new_path.append(seg);
        }
        else // End
        {
            QSharedPointer<TravelSegment> travel_segment =
                QSharedPointer<TravelSegment>::create(m_current_location, m_paths[index].back()->end());
            Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
            travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
            new_path.append(travel_segment);

            QList<QSharedPointer<SegmentBase>> segments = m_paths[index].getSegments();
            while (!segments.isEmpty()) {
                QSharedPointer<SegmentBase> seg = segments.back();
                seg->reverse();
                new_path.append(seg);
                segments.removeLast();
            }
        }
        m_current_location = new_path.back()->end();
        m_paths.remove(index);
    }
    return new_path;
}

Path PathOrderOptimizer::linkNextRadialPath(const Point& center) {
    Path new_path;
    if (m_paths.isEmpty()) {
        return new_path;
    }

    QPair<int, bool> location = radialOpenPath(center);
    int index = location.first;
    bool start = location.second;

    new_path.setCCW(m_paths[index].getCCW());

    QSharedPointer<TravelSegment> travel_segment = QSharedPointer<TravelSegment>::create(
        m_current_location, start ? m_paths[index].front()->start() : m_paths[index].back()->end());
    Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
    new_path.append(travel_segment);

    if (start) {
        for (QSharedPointer<SegmentBase> seg : m_paths[index]) {
            new_path.append(seg);
        }
    }
    else {
        QList<QSharedPointer<SegmentBase>> segments = m_paths[index].getSegments();
        while (!segments.isEmpty()) {
            QSharedPointer<SegmentBase> seg = segments.back();
            seg->reverse();
            new_path.append(seg);
            segments.removeLast();
        }
    }

    m_current_location = new_path.back()->end();
    m_paths.remove(index);
    return new_path;
}

QPair<int, bool> PathOrderOptimizer::radialOpenPath(const Point& center) {
    PathOrderOptimization path_order =
        static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder));
    Point query_point = radialPathQueryPoint(path_order);

    int selected_index = 0;
    switch (path_order) {
        case PathOrderOptimization::kNextFarthest: {
            double farthest = -1.0;
            for (int i = 0, end = m_paths.size(); i < end; ++i) {
                const double distance = nearestOpenEndpointDistance(query_point, m_paths[i])();
                if (distance > farthest) {
                    farthest = distance;
                    selected_index = i;
                }
            }
            break;
        }
        case PathOrderOptimization::kRandom:
            selected_index = QRandomGenerator::global()->bounded(m_paths.size());
            break;
        case PathOrderOptimization::kOutsideIn:
        case PathOrderOptimization::kInsideOut: {
            const double query_radius = std::hypot(query_point.x() - center.x(), query_point.y() - center.y());
            const bool reverse_sweep = path_order == PathOrderOptimization::kInsideOut;
            double best_sweep = std::numeric_limits<double>::max();

            for (int i = 0, end = m_paths.size(); i < end; ++i) {
                const double path_angle = radialArcMidpointAngle(m_paths[i], center);
                double sweep = path_angle;
                if (query_radius > std::numeric_limits<double>::epsilon()) {
                    const double query_angle = std::atan2(query_point.y() - center.y(), query_point.x() - center.x());
                    sweep = reverse_sweep ? positiveAngularDelta(query_angle - path_angle)
                                          : positiveAngularDelta(path_angle - query_angle);
                }
                else if (reverse_sweep) {
                    sweep = positiveAngularDelta(-path_angle);
                }
                else {
                    sweep = positiveAngularDelta(path_angle);
                }

                if (sweep < best_sweep) {
                    best_sweep = sweep;
                    selected_index = i;
                }
            }
            break;
        }
        case PathOrderOptimization::kNextClosest:
        case PathOrderOptimization::kCustomPoint:
        default: {
            double closest = std::numeric_limits<double>::max();
            for (int i = 0, end = m_paths.size(); i < end; ++i) {
                const double distance = nearestOpenEndpointDistance(query_point, m_paths[i])();
                if (distance < closest) {
                    closest = distance;
                    selected_index = i;
                }
            }
            break;
        }
    }

    PointOrderOptimization point_order =
        static_cast<PointOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPointOrder));
    Point point_query = radialPointQueryPoint(point_order);
    Polyline endpoints {m_paths[selected_index].front()->start(), m_paths[selected_index].back()->end()};
    const bool reverse = PointOrderOptimizer::findSkeletonPointOrder(
        point_query, endpoints, point_order, m_sb->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
        m_sb->setting<Distance>(PS::Optimizations::kMinDistanceThreshold));

    return QPair<int, bool>(selected_index, !reverse);
}

Point PathOrderOptimizer::radialPathQueryPoint(PathOrderOptimization optimization) const {
    if (m_override_used) {
        return m_override_location;
    }

    if (optimization == PathOrderOptimization::kCustomPoint) {
        return Point(m_sb->setting<double>(PS::Optimizations::kCustomPathXLocation),
                     m_sb->setting<double>(PS::Optimizations::kCustomPathYLocation), m_current_location.z());
    }

    return m_current_location;
}

Point PathOrderOptimizer::radialPointQueryPoint(PointOrderOptimization optimization) const {
    if (usesCustomPointLocation(optimization)) {
        if (m_point_override_used) {
            return m_point_override_location;
        }

        return Point(m_sb->setting<double>(PS::Optimizations::kCustomPointXLocation),
                     m_sb->setting<double>(PS::Optimizations::kCustomPointYLocation), m_current_location.z());
    }

    return m_current_location;
}

Distance PathOrderOptimizer::nearestOpenEndpointDistance(const Point& query, const Path& path) const {
    return std::min(query.distance(path.front()->start()), query.distance(path.back()->end()));
}

double PathOrderOptimizer::radialArcMidpointAngle(const Path& path, const Point& center) const {
    const double start_angle =
        std::atan2(path.front()->start().y() - center.y(), path.front()->start().x() - center.x());
    const double end_angle = std::atan2(path.back()->end().y() - center.y(), path.back()->end().x() - center.x());
    const double pi = std::acos(-1.0);
    double delta = end_angle - start_angle;
    while (delta > pi) {
        delta -= 2.0 * pi;
    }
    while (delta < -pi) {
        delta += 2.0 * pi;
    }

    return start_angle + delta / 2.0;
}

double PathOrderOptimizer::positiveAngularDelta(double delta) const {
    const double two_pi = 2.0 * std::acos(-1.0);
    while (delta < 0.0) {
        delta += two_pi;
    }
    while (delta >= two_pi) {
        delta -= two_pi;
    }

    return delta;
}

bool PathOrderOptimizer::linkIntersects(Point link_start, Point link_end, QVector<Path> infill_geometry,
                                        PolygonList border_geometry) {
    //! Check for possible intersections with infill geometry
    for (Path path : infill_geometry)
        for (QSharedPointer<SegmentBase> seg : path)
            if (link_start != seg->end() && link_end != seg->start())
                if (seg.dynamicCast<TravelSegment>().isNull() &&
                    MathUtils::intersect(link_start, link_end, seg->start(), seg->end()))
                    return true;

    //! Check for possible intersections with border geometry
    for (Polygon poly : border_geometry) {
        for (int i = 0, end = poly.size() - 1; i < end; ++i)
            if (MathUtils::intersect(link_start, link_end, poly[i], poly[i + 1]))
                return true;

        //! Check last line of polygon
        if (MathUtils::intersect(link_start, link_end, poly.last(), poly.first()))
            return true;
    }

    return false;
}

QPair<int, bool> PathOrderOptimizer::closestOpenPath(QVector<Path> paths) {
    Distance shortest = Distance(std::numeric_limits<float>::max());

    int index = 0;
    bool start;

    Point queryPoint;
    if (m_override_used)
        queryPoint = m_override_location;
    else
        queryPoint = m_current_location;

    for (int i = 0, end = paths.size(); i < end; ++i) {
        if (queryPoint.distance(paths[i].front()->start()) < shortest) {
            shortest = queryPoint.distance(paths[i].front()->start());
            index = i;
            start = true;
        }

        if (queryPoint.distance(paths[i].back()->end()) < shortest) {
            shortest = queryPoint.distance(paths[i].back()->end());
            index = i;
            start = false;
        }
    }
    return QPair<int, bool>(index, start);
}

void PathOrderOptimizer::addTravel(int index, Path& path) {
    QSharedPointer<TravelSegment> travel_segment =
        QSharedPointer<TravelSegment>::create(m_current_location, path[index]->start());
    Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);

    m_current_location = path[index]->start();
    for (int i = 0; i < index; ++i) {
        path.move(0, path.size() - 1);
    }
    path.prepend(travel_segment);
}

Path PathOrderOptimizer::linkTo() {
    int pathIndex;
    PathOrderOptimization orderOptimization =
        static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder));
    switch (orderOptimization) {
        case PathOrderOptimization::kNextClosest:
            pathIndex = findShortestOrLongestDistance();
            break;
        case PathOrderOptimization::kNextFarthest:
            pathIndex = findShortestOrLongestDistance(false);
            break;
        case PathOrderOptimization::kRandom:
            pathIndex = linkToRandom();
            break;
        case PathOrderOptimization::kOutsideIn:
            pathIndex = findInteriorExterior();
            break;
        case PathOrderOptimization::kInsideOut:
            pathIndex = findInteriorExterior(false);
            break;
        default:
            pathIndex = findShortestOrLongestDistance();
            break;
    }

    Path nextPath = m_paths[pathIndex];
    m_paths.removeAt(pathIndex);

    Point queryPoint;
    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPointOrder));

    if (usesCustomPointLocation(pointOrderOptimization))
        queryPoint = m_point_override_location;
    else
        queryPoint = m_current_location;

    Polyline line;
    for (QSharedPointer<SegmentBase> seg : nextPath)
        line.append(seg->start());

    int pointIndex =
        PointOrderOptimizer::linkToPoint(queryPoint, line, m_layer_num, pointOrderOptimization,
                                         m_sb->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                                         m_sb->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                                         m_sb->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                                         m_sb->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                                         m_sb->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius))
            .rotation_index;

    addTravel(pointIndex, nextPath);

    m_current_location = nextPath.back()->end();
    return nextPath;
}

int PathOrderOptimizer::findShortestOrLongestDistance(bool shortest) {
    Point queryPoint;
    if (m_override_used)
        queryPoint = m_override_location;
    else
        queryPoint = m_current_location;

    int pathIndex;
    Distance closest;
    if (shortest)
        closest = Distance(DBL_MAX);

    for (int i = 0, end = m_paths.size(); i < end; ++i) {
        for (int j = 0, end2 = m_paths[i].size(); j < end2; ++j) {
            QSharedPointer<SegmentBase> seg = m_paths[i][j];
            Distance closestSegment = MathUtils::distanceFromLineSegSqrd(queryPoint, seg->start(), seg->end());
            if (shortest) {
                if (closestSegment < closest) {
                    closest = closestSegment;
                    pathIndex = i;
                }
            }
            else {
                if (closestSegment > closest) {
                    closest = closestSegment;
                    pathIndex = i;
                }
            }
        }
    }

    return pathIndex;
}

int PathOrderOptimizer::linkToRandom() { return QRandomGenerator::global()->bounded(m_paths.size()); }

int PathOrderOptimizer::findInteriorExterior(bool ExtToInt) {
    if (!m_has_computed_heirarchy && m_paths.size() > 0) {
        QSharedPointer<TopologicalNode> root = computeTopologicalHeirarchy();
        // inOrderTraversal(root);
        levelOrder(root);
        if (!ExtToInt)
            std::reverse(m_topo_order.begin(), m_topo_order.end());

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

QSharedPointer<PathOrderOptimizer::TopologicalNode> PathOrderOptimizer::computeTopologicalHeirarchy() {
    QVector<QSharedPointer<TopologicalNode>> all_nodes;
    all_nodes.reserve(m_paths.size());

    for (int i = 0, end = m_paths.size(); i < end; ++i) {
        Polygon poly;
        poly.reserve(m_paths[i].size());
        for (QSharedPointer<SegmentBase> seg : m_paths[i].getSegments())
            poly.push_back(Point(seg->start()));

        all_nodes.push_back(QSharedPointer<TopologicalNode>::create(TopologicalNode(i, poly)));
    }

    // assume first path is outer contour
    QSharedPointer<TopologicalNode> root;
    root = all_nodes[0];
    all_nodes.removeFirst();

    for (QSharedPointer<TopologicalNode> node : all_nodes)
        insert(root, node);

    return root;
}

void PathOrderOptimizer::insert(QSharedPointer<TopologicalNode> root, QSharedPointer<TopologicalNode> current) {
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

void PathOrderOptimizer::levelOrder(QSharedPointer<TopologicalNode> root) {
    if (m_topo_order.size() == m_topo_level)
        m_topo_order.push_back({root->m_path_index});
    else
        m_topo_order[m_topo_level].push_back(root->m_path_index);

    ++m_topo_level;

    for (QSharedPointer<TopologicalNode> n : root->m_children)
        levelOrder(n);

    --m_topo_level;
}

Path PathOrderOptimizer::linkSpiralPath2D(bool last_spiral) {
    int pathIndex = findShortestOrLongestDistance();
    Path newPath = m_paths[pathIndex];
    m_paths.removeAt(pathIndex);

    QSharedPointer<SettingsBase> temp_sb = QSharedPointer<SettingsBase>::create(SettingsBase());
    temp_sb->setSetting(PS::Optimizations::kPointOrder, PointOrderOptimization::kNextClosest);
    temp_sb->setSetting(PS::Optimizations::kMinDistanceEnabled, false);
    temp_sb->setSetting<Distance>(PS::Optimizations::kMinDistanceThreshold, Distance());

    Polyline line;
    for (QSharedPointer<SegmentBase> seg : newPath)
        line.append(seg->start());

    int pointIndex = PointOrderOptimizer::linkToPoint(m_current_location, line, m_layer_num,
                                                      PointOrderOptimization::kNextClosest, false, 0, 0, false, 0)
                         .rotation_index;

    for (int i = 0; i < pointIndex; ++i)
        newPath.move(0, newPath.size() - 1);

    Distance layer_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);

    Distance pathLength = newPath.calculateLength();
    Distance currentLength;

    int start = 0;
    if (!last_spiral) {
        addTravel(0, newPath);
        start = 1;
    }

    for (int end = newPath.size(); start < end; ++start) {
        QSharedPointer<SegmentBase> seg = newPath[start];
        Point startPt = seg->start();
        Point endPt = seg->end();

        currentLength += startPt.distance(m_current_location);
        startPt.z(startPt.z() + ((currentLength / pathLength) * layer_height)());
        seg->setStart(startPt);

        m_current_location.x(startPt.x());
        m_current_location.y(startPt.y());

        currentLength += endPt.distance(m_current_location);
        endPt.z(endPt.z() + ((currentLength / pathLength) * layer_height)());
        seg->setEnd(endPt);

        m_current_location.x(endPt.x());
        m_current_location.y(endPt.y());
    }
    return newPath;
}

} // namespace ORNL
