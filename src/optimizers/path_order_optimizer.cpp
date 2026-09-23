#include "optimizers/path_order_optimizer.h"

#include <QRandomGenerator>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

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
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "optimizers/point_order_optimizer.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
constexpr double kDistanceTolerance = 1.0e-6;
constexpr double kTwoPi             = 6.28318530717958647692;

Polyline pathStartPoints(const Path& path) {
    Polyline line;
    line.reserve(path.size());
    for (const QSharedPointer<SegmentBase>& segment : path) {
        if (!segment.isNull()) line.append(segment->start());
    }

    return line;
}

int normalizedIndex(int index, int size) {
    if (size <= 0) return 0;

    index %= size;
    if (index < 0) index += size;
    return index;
}

double chordRatioForPoint(const Point& start, const Point& end, const Point& point) {
    const double dx          = static_cast<double>(end.x() - start.x());
    const double dy          = static_cast<double>(end.y() - start.y());
    const double denominator = (dx * dx) + (dy * dy);
    if (denominator <= kDistanceTolerance) return 0.0;

    const double point_dx = static_cast<double>(point.x() - start.x());
    const double point_dy = static_cast<double>(point.y() - start.y());
    return std::clamp(((point_dx * dx) + (point_dy * dy)) / denominator, 0.0, 1.0);
}

Angle arcSweepAngle(const Point& center, const Point& start, const Point& end, bool counterclockwise) {
    const double start_angle = std::atan2(center.x() - start.x(), center.y() - start.y());
    const double end_angle   = std::atan2(center.x() - end.x(), center.y() - end.y());
    double sweep             = counterclockwise ? start_angle - end_angle : end_angle - start_angle;
    if (sweep <= 0.0) sweep += kTwoPi;

    return Angle(sweep);
}

Point pointOnArcAtChordRatio(const ArcSegment& arc, const Point& chord_point) {
    const Point start  = arc.start();
    const Point end    = arc.end();
    const Point center = arc.center();
    const double ratio = chordRatioForPoint(start, end, chord_point);
    const double radius =
        std::hypot(static_cast<double>(start.x() - center.x()), static_cast<double>(start.y() - center.y()));
    const double start_angle =
        std::atan2(static_cast<double>(start.y() - center.y()), static_cast<double>(start.x() - center.x()));
    const double signed_sweep = arc.counterclockwise() ? arc.angle()() : -arc.angle()();
    const double angle        = start_angle + (signed_sweep * ratio);

    return Point(center.x() + (radius * std::cos(angle)), center.y() + (radius * std::sin(angle)),
                 start.z() + ((end.z() - start.z()) * ratio));
}

void refreshArcSweep(const QSharedPointer<SegmentBase>& segment) {
    ArcSegment* arc = dynamic_cast<ArcSegment*>(segment.data());
    if (arc == nullptr) return;

    arc->setAngle(arcSweepAngle(arc->center(), arc->start(), arc->end(), arc->counterclockwise()));
}

bool splitSegment(Path& path, int insertion_index, const Point& split_point) {
    if (path.size() == 0) return false;

    insertion_index                              = normalizedIndex(insertion_index, path.size());
    const int segment_index                      = insertion_index == 0 ? path.size() - 1 : insertion_index - 1;
    QSharedPointer<SegmentBase> original_segment = path[segment_index];
    if (original_segment.isNull()) return false;

    const ArcSegment* original_arc = dynamic_cast<ArcSegment*>(original_segment.data());
    if (dynamic_cast<LineSegment*>(original_segment.data()) == nullptr && original_arc == nullptr) return false;

    const Point adjusted_split_point =
        original_arc == nullptr ? split_point : pointOnArcAtChordRatio(*original_arc, split_point);
    if (adjusted_split_point == original_segment->start() || adjusted_split_point == original_segment->end())
        return false;

    QSharedPointer<SegmentBase> first_segment  = original_segment->clone();
    QSharedPointer<SegmentBase> second_segment = original_segment->clone();
    first_segment->setEnd(adjusted_split_point);
    second_segment->setStart(adjusted_split_point);
    refreshArcSweep(first_segment);
    refreshArcSweep(second_segment);

    path.removeAt(segment_index);
    if (insertion_index == 0) {
        path.append(first_segment);
        path.prepend(second_segment);
    }
    else {
        path.insert(segment_index, first_segment);
        path.insert(insertion_index, second_segment);
    }

    return true;
}

void applyPointSelectionToPath(Path& path, const PointOrderOptimizer::PointOrderSelection& selection) {
    if (path.size() == 0) return;

    int rotation_index = normalizedIndex(selection.rotation_index, path.size());

    if (selection.insert_split_point) {
        int insertion_index                 = normalizedIndex(selection.insertion_index, path.size());
        const int segment_index             = insertion_index == 0 ? path.size() - 1 : insertion_index - 1;
        QSharedPointer<SegmentBase> segment = path[segment_index];

        if (!segment.isNull() && selection.split_point == segment->start()) { rotation_index = segment_index; }
        else if (!segment.isNull() && selection.split_point == segment->end()) { rotation_index = insertion_index; }
        else if (splitSegment(path, insertion_index, selection.split_point)) {
            rotation_index = insertion_index == 0 ? 0 : insertion_index;
        }
        else { rotation_index = insertion_index; }
    }

    rotation_index = normalizedIndex(rotation_index, path.size());
    for (int i = 0; i < rotation_index; ++i) path.move(0, path.size() - 1);
}
}  // namespace

PathOrderOptimizer::PathOrderOptimizer(Point& start, uint layer_number, const QSharedPointer<SettingsBase>& sb)
    : m_current_location(start),
      m_layer_number(layer_number),
      m_sb(sb),
      m_override_used(false),
      m_point_override_used(false) {
    m_layer_num = layer_number;
}

Point& PathOrderOptimizer::getCurrentLocation() {
    return m_current_location;
}

int PathOrderOptimizer::getCurrentPathCount() {
    return m_paths.size();
}

void PathOrderOptimizer::setPathsToEvaluate(QVector<Path> paths) {
    m_paths = paths;
    for (Path& path : m_paths) path.removeTravels();

    for (int i = m_paths.size() - 1; i >= 0; --i) {
        if (m_paths[i].size() == 0) m_paths.removeAt(i);
    }

    if (m_paths.size() > 0 && !m_paths.front().getSegments().front().isNull())
        m_current_region_type = m_paths.front().getSegments().front()->getSb()->setting<RegionType>(SS::kRegionType);
    else
        m_current_region_type = RegionType::kUnknown;

    m_has_computed_heirarchy = false;
    m_topo_level             = 0;
    m_topo_order.clear();
}

void PathOrderOptimizer::setParameters(InfillPatterns infillPattern, PolygonList border_geometry) {
    m_pattern         = infillPattern;
    m_border_geometry = border_geometry;
}

void PathOrderOptimizer::setParameters(PolygonList previousIslands) {}

void PathOrderOptimizer::setStartOverride(Point pt) {
    m_override_location = pt;
    m_override_used     = true;
}

void PathOrderOptimizer::setStartPointOverride(Point pt) {
    m_point_override_location = pt;
    m_point_override_used     = true;
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

    if (nextPath.size() == 0) {
        m_current_location = savedLocation;
        return nextPath;
    }

    Distance minDist;
    if (nextPath.back()->getSb()->setting<RegionType>(SS::kRegionType) == RegionType::kInfill)
        minDist = m_sb->setting<Distance>(PS::Infill::kMinPathLength);
    else if (nextPath.back()->getSb()->setting<RegionType>(SS::kRegionType) == RegionType::kSkin)
        minDist = m_sb->setting<Distance>(PS::Skin::kMinPathLength);

    if (nextPath.calculateLengthNoTravel() < minDist) nextPath.clear();

    // if there are no segments reset start location
    if (nextPath.size() > 0)
        m_current_location = nextPath.back()->end();
    else
        m_current_location = savedLocation;

    return nextPath;
}

Path PathOrderOptimizer::linkNextInfillLines(QVector<Path>& paths) {
    if (m_paths.isEmpty()) return Path();

    //! Gather settings for line segment links
    Distance bead_width            = m_paths.front().front()->getSb()->setting<Distance>(SS::kWidth);
    Distance layer_height          = m_paths.front().front()->getSb()->setting<Distance>(SS::kHeight);
    Velocity speed                 = m_paths.front().front()->getSb()->setting<Velocity>(SS::kSpeed);
    Acceleration acceleration      = m_paths.front().front()->getSb()->setting<Acceleration>(SS::kAccel);
    AngularVelocity extruder_speed = m_paths.front().front()->getSb()->setting<AngularVelocity>(SS::kExtruderSpeed);

    Path new_path;
    QPair<int, bool> indexAndStart = closestOpenPath(m_paths);
    int index                      = indexAndStart.first;
    if (index < 0 || index >= m_paths.size()) return Path();

    // if false, indicates index is closest if you start at the end point, so reverse
    if (indexAndStart.second == false) m_paths[index].reverseSegments();

    QVector<Path> empty_paths;
    PolygonList empty_polygon_list;

    QSharedPointer<TravelSegment> travel_segment =
        QSharedPointer<TravelSegment>::create(m_current_location, m_paths[index].front()->start());

    Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
    new_path.append(travel_segment);

    for (QSharedPointer<SegmentBase> seg : m_paths[index]) new_path.append(seg);

    m_current_location = new_path.back()->end();
    m_paths.remove(index);

    const Distance min_travel_distance = m_sb->setting<Distance>(PS::Travel::kInfillMinLength);
    if (min_travel_distance > 0) {
        for (int i = 0, end = m_paths.size(); i < end; ++i) {
            if (m_paths.size() > 0) {
                indexAndStart = closestOpenPath(m_paths);
                index         = indexAndStart.first;
                if (index < 0 || index >= m_paths.size()) break;

                // if false, indicates index is closest if you start at the end point, so reverse
                if (indexAndStart.second == false) m_paths[index].reverseSegments();

                Point link_start       = m_current_location;
                Point link_end         = m_paths[index].front()->start();
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

                    for (QSharedPointer<SegmentBase> seg : m_paths[index]) new_path.append(seg);

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
    if (m_paths.size() > 0) new_path = linkTo();

    return new_path;
}

Path PathOrderOptimizer::linkNextSkeletonPath() {
    Path new_path;
    if (!m_paths.isEmpty()) {
        QPair<int, bool> location = closestOpenPath(m_paths);
        int index                 = location.first;
        bool start                = location.second;
        if (index < 0 || index >= m_paths.size()) return new_path;

        new_path.setCCW(m_paths[index].getCCW());

        if (start) {
            QSharedPointer<TravelSegment> travel_segment =
                QSharedPointer<TravelSegment>::create(m_current_location, m_paths[index].front()->start());
            Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
            travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
            new_path.append(travel_segment);

            for (QSharedPointer<SegmentBase> seg : m_paths[index]) new_path.append(seg);
        }
        else  // End
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

Path PathOrderOptimizer::linkNextRadialPath() {
    Path new_path;
    if (m_paths.isEmpty()) { return new_path; }

    RadialPathSelection location = radialPathSelection();
    int index                    = location.path_index;
    if (index < 0 || index >= m_paths.size()) return new_path;

    new_path = m_paths[index];
    new_path.setCCW(m_paths[index].getCCW());

    if (location.rotate_to_segment) {
        PointOrderOptimizer::PointOrderSelection point_selection;
        point_selection.rotation_index     = location.segment_index;
        point_selection.insert_split_point = location.insert_split_point;
        point_selection.split_point        = location.split_point;
        point_selection.insertion_index    = location.insertion_index;
        addTravel(point_selection, new_path);
        m_current_location = new_path.back()->end();
        m_paths.remove(index);
        return new_path;
    }

    QSharedPointer<TravelSegment> travel_segment = QSharedPointer<TravelSegment>::create(
        m_current_location, location.start_from_front ? m_paths[index].front()->start() : m_paths[index].back()->end());
    Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
    new_path.clear();
    new_path.setCCW(m_paths[index].getCCW());
    new_path.append(travel_segment);

    if (location.start_from_front) {
        for (QSharedPointer<SegmentBase> seg : m_paths[index]) { new_path.append(seg); }
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

Path PathOrderOptimizer::linkNextHelicalPath(bool* starts_from_generated_end) {
    Path new_path;
    if (starts_from_generated_end != nullptr) { *starts_from_generated_end = false; }
    if (m_paths.isEmpty()) { return new_path; }

    OpenPathSelection location = helicalOpenPath();
    const int index            = location.path_index;
    if (index < 0 || index >= m_paths.size()) return new_path;
    if (starts_from_generated_end != nullptr) { *starts_from_generated_end = !location.start_from_front; }

    new_path.setCCW(m_paths[index].getCCW());

    QSharedPointer<TravelSegment> travel_segment = QSharedPointer<TravelSegment>::create(
        m_current_location, location.start_from_front ? m_paths[index].front()->start() : m_paths[index].back()->end());
    Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);
    new_path.append(travel_segment);

    if (location.start_from_front) {
        for (QSharedPointer<SegmentBase> seg : m_paths[index]) { new_path.append(seg); }
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

PathOrderOptimizer::RadialPathSelection PathOrderOptimizer::radialPathSelection() {
    RadialPathSelection selection;
    if (m_paths.isEmpty()) return selection;

    PathOrderOptimization path_order = cylindricalPathOrderOptimization();
    Point query_point                = radialPathQueryPoint(path_order);
    const bool find_farthest         = path_order == PathOrderOptimization::kNextFarthest;
    const PointOrderOptimization point_order =
        static_cast<PointOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPointOrder));

    double selected_distance = 0.0;
    for (int i = 0, end = m_paths.size(); i < end; ++i) {
        if (m_paths[i].isClosed()) {
            for (int j = 0, segment_count = m_paths[i].size(); j < segment_count; ++j) {
                QSharedPointer<SegmentBase> segment = m_paths[i][j];
                if (segment.isNull()) { continue; }

                const double distance = query_point.distance(segment->start())();
                if (selection.path_index < 0 || (find_farthest && distance > selected_distance) ||
                    (!find_farthest && distance < selected_distance)) {
                    selected_distance           = distance;
                    selection.path_index        = i;
                    selection.segment_index     = j;
                    selection.start_from_front  = true;
                    selection.rotate_to_segment = true;
                }
            }
        }
        else {
            const double distance = nearestOpenEndpointDistance(query_point, m_paths[i])();
            if (selection.path_index < 0 || (find_farthest && distance > selected_distance) ||
                (!find_farthest && distance < selected_distance)) {
                selected_distance           = distance;
                selection.path_index        = i;
                selection.segment_index     = 0;
                selection.start_from_front  = true;
                selection.rotate_to_segment = false;
            }
        }
    }

    if (selection.path_index < 0) { return selection; }

    if (selection.rotate_to_segment) {
        if (point_order == PointOrderOptimization::kConsecutive) {
            const Polyline line = pathStartPoints(m_paths[selection.path_index]);
            std::optional<Point> consecutive_reference;
            if (m_layer_num > 1) consecutive_reference = m_current_location;

            const auto point_selection = PointOrderOptimizer::linkToPoint(
                m_current_location, line, m_layer_num, point_order,
                m_sb->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                m_sb->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                m_sb->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                m_sb->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                m_sb->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius), false, consecutive_reference);

            selection.segment_index      = point_selection.rotation_index;
            selection.insert_split_point = point_selection.insert_split_point;
            selection.split_point        = point_selection.split_point;
            selection.insertion_index    = point_selection.insertion_index;
        }

        return selection;
    }

    Point point_query = radialPointQueryPoint(point_order);
    Polyline endpoints {m_paths[selection.path_index].front()->start(), m_paths[selection.path_index].back()->end()};
    const bool reverse = PointOrderOptimizer::findSkeletonPointOrder(
        point_query, endpoints, point_order, m_sb->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
        m_sb->setting<Distance>(PS::Optimizations::kMinDistanceThreshold));

    selection.start_from_front = !reverse;
    return selection;
}

PathOrderOptimizer::OpenPathSelection PathOrderOptimizer::helicalOpenPath() const {
    OpenPathSelection selection;
    if (m_paths.isEmpty()) return selection;

    const PathOrderOptimization path_order = cylindricalPathOrderOptimization();
    const Point query_point                = m_current_location;
    const bool find_farthest               = path_order == PathOrderOptimization::kNextFarthest;

    double selected_distance = 0.0;
    for (int i = 0, end = m_paths.size(); i < end; ++i) {
        const double start_distance = query_point.distance(m_paths[i].front()->start())();
        if (selection.path_index < 0 || (find_farthest && start_distance > selected_distance) ||
            (!find_farthest && start_distance < selected_distance)) {
            selected_distance          = start_distance;
            selection.path_index       = i;
            selection.start_from_front = true;
        }

        const double end_distance = query_point.distance(m_paths[i].back()->end())();
        if ((find_farthest && end_distance > selected_distance) ||
            (!find_farthest && end_distance < selected_distance)) {
            selected_distance          = end_distance;
            selection.path_index       = i;
            selection.start_from_front = false;
        }
    }

    return selection;
}

PathOrderOptimization PathOrderOptimizer::cylindricalPathOrderOptimization() const {
    const int path_order = m_sb->setting<int>(PS::Optimizations::kCylindricalPathOrder);
    if (path_order == static_cast<int>(PathOrderOptimization::kNextFarthest)) {
        return PathOrderOptimization::kNextFarthest;
    }

    return PathOrderOptimization::kNextClosest;
}

Point PathOrderOptimizer::radialPathQueryPoint(PathOrderOptimization optimization) const {
    if (m_override_used) { return m_override_location; }

    if (optimization == PathOrderOptimization::kCustomPoint) {
        return Point(m_sb->setting<double>(PS::Optimizations::kCustomPathXLocation),
                     m_sb->setting<double>(PS::Optimizations::kCustomPathYLocation), m_current_location.z());
    }

    return m_current_location;
}

Point PathOrderOptimizer::radialPointQueryPoint(PointOrderOptimization optimization) const {
    if (usesCustomPointLocation(optimization)) {
        if (m_point_override_used) { return m_point_override_location; }

        return Point(m_sb->setting<double>(PS::Optimizations::kCustomPointXLocation),
                     m_sb->setting<double>(PS::Optimizations::kCustomPointYLocation), m_current_location.z());
    }

    return m_current_location;
}

Distance PathOrderOptimizer::nearestOpenEndpointDistance(const Point& query, const Path& path) const {
    return std::min(query.distance(path.front()->start()), query.distance(path.back()->end()));
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
            if (MathUtils::intersect(link_start, link_end, poly[i], poly[i + 1])) return true;

        //! Check last line of polygon
        if (MathUtils::intersect(link_start, link_end, poly.last(), poly.first())) return true;
    }

    return false;
}

QPair<int, bool> PathOrderOptimizer::closestOpenPath(QVector<Path> paths) {
    if (paths.isEmpty()) return QPair<int, bool>(-1, true);

    Distance shortest = Distance(std::numeric_limits<float>::max());

    int index       = 0;
    bool start      = true;
    bool found_path = false;

    Point queryPoint;
    if (m_override_used)
        queryPoint = m_override_location;
    else
        queryPoint = m_current_location;

    for (int i = 0, end = paths.size(); i < end; ++i) {
        if (paths[i].size() == 0) continue;

        if (queryPoint.distance(paths[i].front()->start()) < shortest) {
            shortest   = queryPoint.distance(paths[i].front()->start());
            index      = i;
            start      = true;
            found_path = true;
        }

        if (queryPoint.distance(paths[i].back()->end()) < shortest) {
            shortest   = queryPoint.distance(paths[i].back()->end());
            index      = i;
            start      = false;
            found_path = true;
        }
    }
    if (!found_path) return QPair<int, bool>(-1, true);

    return QPair<int, bool>(index, start);
}

void PathOrderOptimizer::addTravel(int index, Path& path) {
    PointOrderOptimizer::PointOrderSelection selection;
    selection.rotation_index = index;
    addTravel(selection, path);
}

void PathOrderOptimizer::addTravel(const PointOrderOptimizer::PointOrderSelection& selection, Path& path) {
    if (path.size() == 0) return;

    applyPointSelectionToPath(path, selection);

    QSharedPointer<TravelSegment> travel_segment =
        QSharedPointer<TravelSegment>::create(m_current_location, path.front()->start());
    Velocity velocity = m_sb->setting<Velocity>(PS::Travel::kSpeed);
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);

    m_current_location = path.front()->start();
    path.prepend(travel_segment);
}

Path PathOrderOptimizer::linkTo() {
    if (m_paths.isEmpty()) return Path();

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
    if (pathIndex < 0 || pathIndex >= m_paths.size()) return Path();

    Path nextPath = m_paths[pathIndex];
    m_paths.removeAt(pathIndex);
    if (nextPath.size() == 0) return Path();

    Point queryPoint;
    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPointOrder));

    if (usesCustomPointLocation(pointOrderOptimization))
        queryPoint = m_point_override_location;
    else
        queryPoint = m_current_location;

    Polyline line;
    for (QSharedPointer<SegmentBase> seg : nextPath) line.append(seg->start());

    auto pointSelection =
        PointOrderOptimizer::linkToPoint(queryPoint, line, m_layer_num, pointOrderOptimization,
                                         m_sb->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                                         m_sb->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                                         m_sb->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                                         m_sb->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                                         m_sb->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius));

    addTravel(pointSelection, nextPath);

    m_current_location = nextPath.back()->end();
    return nextPath;
}

int PathOrderOptimizer::findShortestOrLongestDistance(bool shortest) {
    if (m_paths.isEmpty()) return -1;

    Point queryPoint;
    if (m_override_used)
        queryPoint = m_override_location;
    else
        queryPoint = m_current_location;

    int pathIndex = -1;
    Distance selected_distance;

    for (int i = 0, end = m_paths.size(); i < end; ++i) {
        for (int j = 0, end2 = m_paths[i].size(); j < end2; ++j) {
            QSharedPointer<SegmentBase> seg = m_paths[i][j];
            if (seg.isNull()) continue;

            Distance closestSegment = MathUtils::distanceFromLineSegSqrd(queryPoint, seg->start(), seg->end());
            if (pathIndex < 0 || (shortest && closestSegment < selected_distance) ||
                (!shortest && closestSegment > selected_distance)) {
                selected_distance = closestSegment;
                pathIndex         = i;
            }
        }
    }

    return pathIndex;
}

int PathOrderOptimizer::linkToRandom() {
    if (m_paths.isEmpty()) return -1;

    return QRandomGenerator::global()->bounded(m_paths.size());
}

int PathOrderOptimizer::findInteriorExterior(bool ExtToInt) {
    if (m_paths.isEmpty()) return -1;

    if (!m_has_computed_heirarchy && m_paths.size() > 0) {
        QSharedPointer<TopologicalNode> root = computeTopologicalHeirarchy();
        // inOrderTraversal(root);
        levelOrder(root);
        if (!ExtToInt) std::reverse(m_topo_order.begin(), m_topo_order.end());

        m_has_computed_heirarchy = true;
    }

    if (m_topo_order.isEmpty() || m_topo_order.first().isEmpty()) return findShortestOrLongestDistance();

    int result = m_topo_order.first().first();
    m_topo_order.first().pop_front();
    if (m_topo_order.first().size() == 0) m_topo_order.pop_front();

    for (QVector<int>& level : m_topo_order)
        for (int& element : level)
            if (element > result) --element;

    return result;
}

QSharedPointer<PathOrderOptimizer::TopologicalNode> PathOrderOptimizer::computeTopologicalHeirarchy() {
    QVector<QSharedPointer<TopologicalNode>> all_nodes;
    all_nodes.reserve(m_paths.size());

    for (int i = 0, end = m_paths.size(); i < end; ++i) {
        if (m_paths[i].size() == 0) continue;

        Polygon poly;
        poly.reserve(m_paths[i].size());
        for (QSharedPointer<SegmentBase> seg : m_paths[i].getSegments()) poly.push_back(Point(seg->start()));

        if (!poly.isEmpty()) all_nodes.push_back(QSharedPointer<TopologicalNode>::create(TopologicalNode(i, poly)));
    }
    if (all_nodes.isEmpty()) return QSharedPointer<TopologicalNode>();

    // assume first path is outer contour
    QSharedPointer<TopologicalNode> root;
    root = all_nodes[0];
    all_nodes.removeFirst();

    for (QSharedPointer<TopologicalNode> node : all_nodes) insert(root, node);

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
    if (root.isNull()) return;

    if (m_topo_order.size() == m_topo_level)
        m_topo_order.push_back({root->m_path_index});
    else
        m_topo_order[m_topo_level].push_back(root->m_path_index);

    ++m_topo_level;

    for (QSharedPointer<TopologicalNode> n : root->m_children) levelOrder(n);

    --m_topo_level;
}

Path PathOrderOptimizer::linkSpiralPath2D(bool last_spiral) {
    int pathIndex = findShortestOrLongestDistance();
    if (pathIndex < 0 || pathIndex >= m_paths.size()) return Path();

    Path newPath = m_paths[pathIndex];
    m_paths.removeAt(pathIndex);

    QSharedPointer<SettingsBase> temp_sb = QSharedPointer<SettingsBase>::create(SettingsBase());
    temp_sb->setSetting(PS::Optimizations::kPointOrder, PointOrderOptimization::kNextClosest);
    temp_sb->setSetting(PS::Optimizations::kMinDistanceEnabled, false);
    temp_sb->setSetting<Distance>(PS::Optimizations::kMinDistanceThreshold, Distance());

    Polyline line;
    for (QSharedPointer<SegmentBase> seg : newPath) line.append(seg->start());

    int pointIndex = PointOrderOptimizer::linkToPoint(m_current_location, line, m_layer_num,
                                                      PointOrderOptimization::kNextClosest, false, 0, 0, false, 0)
                         .rotation_index;

    for (int i = 0; i < pointIndex; ++i) newPath.move(0, newPath.size() - 1);

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
        Point startPt                   = seg->start();
        Point endPt                     = seg->end();

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

}  // namespace ORNL
