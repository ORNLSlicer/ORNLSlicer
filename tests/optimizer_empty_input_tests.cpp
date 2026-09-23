#include <QList>
#include <QSharedPointer>
#include <QVector>
#include <cmath>
#include <cstdlib>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "optimizers/island_order_optimizer.h"
#include "optimizers/path_order_optimizer.h"
#include "optimizers/polyline_order_optimizer.h"
#define private public
#include "step/layer/cylindrical_layer.h"
#undef private
#include "step/layer/island/island_base.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {

bool closeTo(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= 1.0e-4;
}

bool pointClose(const ORNL::Point& lhs, const ORNL::Point& rhs) {
    return closeTo(lhs.x(), rhs.x()) && closeTo(lhs.y(), rhs.y()) && closeTo(lhs.z(), rhs.z());
}

QSharedPointer<ORNL::LineSegment> lineSegment(const ORNL::Point& start, const ORNL::Point& end,
                                              ORNL::RegionType region_type = ORNL::RegionType::kPerimeter,
                                              bool region_start            = false) {
    QSharedPointer<ORNL::LineSegment> segment = QSharedPointer<ORNL::LineSegment>::create(start, end);
    segment->getSb()->setSetting(ORNL::SS::kRegionType, region_type);
    segment->getSb()->setSetting(ORNL::SS::kIsRegionStartSegment, region_start);
    return segment;
}

QSharedPointer<ORNL::ArcSegment> arcSegment(const ORNL::Point& start, const ORNL::Point& end, const ORNL::Point& center,
                                            bool counterclockwise) {
    QSharedPointer<ORNL::ArcSegment> segment =
        QSharedPointer<ORNL::ArcSegment>::create(start, end, center, counterclockwise);
    segment->getSb()->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);
    segment->getSb()->setSetting(ORNL::SS::kIsRegionStartSegment, false);
    return segment;
}

ORNL::Path linePath(float start_x, float end_x) {
    ORNL::Path path;
    path.append(lineSegment(ORNL::Point(start_x, 0.0f, 0.0f), ORNL::Point(end_x, 0.0f, 0.0f)));
    return path;
}

ORNL::Path pathFromPoints(const QVector<ORNL::Point>& points) {
    ORNL::Path path;
    for (int i = 0, end = points.size() - 1; i < end; ++i) { path.append(lineSegment(points[i], points[i + 1])); }

    return path;
}

QSharedPointer<ORNL::SettingsBase> cylindricalSettings(ORNL::PathOrderOptimization cylindrical_order,
                                                       ORNL::PathOrderOptimization planar_order) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Optimizations::kCylindricalPathOrder, static_cast<int>(cylindrical_order));
    settings->setSetting(ORNL::PS::Optimizations::kPathOrder, static_cast<int>(planar_order));
    settings->setSetting(ORNL::PS::Optimizations::kPointOrder,
                         static_cast<int>(ORNL::PointOrderOptimization::kNextClosest));
    settings->setSetting(ORNL::PS::Optimizations::kMinDistanceEnabled, false);
    settings->setSetting(ORNL::PS::Optimizations::kLocalRandomnessEnable, false);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 100.0 * ORNL::mm / ORNL::s);
    return settings;
}

bool isTravelSegment(const QSharedPointer<ORNL::SegmentBase>& segment) {
    return dynamic_cast<ORNL::TravelSegment*>(segment.data()) != nullptr;
}

ORNL::Path mixedRegionPath(const QVector<ORNL::RegionType>& regions) {
    ORNL::Path path;
    for (int i = 0, end = regions.size(); i < end; ++i) {
        path.append(lineSegment(ORNL::Point(10.0f + i, 0.0f, 0.0f), ORNL::Point(11.0f + i, 0.0f, 0.0f), regions[i]));
    }

    return path;
}

bool helicalLayerReversalPreservesRegionsAndTransitions() {
    QSharedPointer<ORNL::SettingsBase> settings =
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest);
    ORNL::CylindricalLayer layer(0, settings, ORNL::CylindricalPathPattern::kHelical);
    layer.addPath(
        mixedRegionPath({ORNL::RegionType::kPerimeter, ORNL::RegionType::kInfill, ORNL::RegionType::kPerimeter}));

    ORNL::Point current_location(13.2f, 0.0f, 0.0f);
    layer.calculateModifiers(current_location);

    if (layer.m_paths.size() != 1) { return false; }

    const ORNL::Path& result = layer.m_paths.first();
    if (result.size() != 4 || !isTravelSegment(result[0]) || isTravelSegment(result[1]) || isTravelSegment(result[2]) ||
        isTravelSegment(result[3])) {
        return false;
    }

    return result[1]->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType) == ORNL::RegionType::kPerimeter &&
           result[2]->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType) == ORNL::RegionType::kInfill &&
           result[3]->getSb()->setting<ORNL::RegionType>(ORNL::SS::kRegionType) == ORNL::RegionType::kPerimeter &&
           result[1]->getSb()->setting<bool>(ORNL::SS::kIsRegionStartSegment) &&
           result[2]->getSb()->setting<bool>(ORNL::SS::kIsRegionStartSegment) &&
           result[3]->getSb()->setting<bool>(ORNL::SS::kIsRegionStartSegment) &&
           result[1]->end() == result[2]->start() && result[2]->end() == result[3]->start();
}

bool helicalLayerRecomputesSameRegionStartFlags() {
    QSharedPointer<ORNL::SettingsBase> settings =
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest);
    ORNL::CylindricalLayer layer(0, settings, ORNL::CylindricalPathPattern::kHelical);
    layer.addPath(mixedRegionPath({ORNL::RegionType::kPerimeter, ORNL::RegionType::kPerimeter}));

    ORNL::Point current_location(12.2f, 0.0f, 0.0f);
    layer.calculateModifiers(current_location);

    if (layer.m_paths.size() != 1) { return false; }

    const ORNL::Path& result = layer.m_paths.first();
    if (result.size() != 3 || !isTravelSegment(result[0])) { return false; }

    return result[1]->getSb()->setting<bool>(ORNL::SS::kIsRegionStartSegment) &&
           !result[2]->getSb()->setting<bool>(ORNL::SS::kIsRegionStartSegment);
}
}  // namespace

int main() {
    bool passed = true;

    ORNL::Point start(0.0f, 0.0f, 0.0f);

    ORNL::IslandBaseOrderOptimizer island_optimizer(start, QList<QSharedPointer<ORNL::IslandBase>>(), -1);
    passed &= ORNL::Testing::expect(island_optimizer.computeNextIndex() == -1,
                                    "Expected empty island optimizer to return -1.");

    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    ORNL::PathOrderOptimizer path_optimizer(start, 0, settings);
    QVector<ORNL::Path> paths;
    paths.append(ORNL::Path());
    path_optimizer.setPathsToEvaluate(paths);
    passed &= ORNL::Testing::expect(path_optimizer.getCurrentPathCount() == 0, "Expected empty paths to be filtered.");
    passed &= ORNL::Testing::expect(path_optimizer.linkNextPath().size() == 0, "Expected empty path optimizer result.");

    settings->setSetting(ORNL::PS::Optimizations::kPathOrder,
                         static_cast<int>(ORNL::PathOrderOptimization::kNextFarthest));
    settings->setSetting(ORNL::PS::Optimizations::kPointOrder,
                         static_cast<int>(ORNL::PointOrderOptimization::kNextClosest));
    settings->setSetting(ORNL::PS::Optimizations::kMinDistanceEnabled, false);
    settings->setSetting(ORNL::PS::Optimizations::kLocalRandomnessEnable, false);

    ORNL::Path zero_distance_path;
    zero_distance_path.append(QSharedPointer<ORNL::LineSegment>::create(start, start));
    ORNL::PathOrderOptimizer farthest_path_optimizer(start, 0, settings);
    farthest_path_optimizer.setPathsToEvaluate({zero_distance_path});
    passed &= ORNL::Testing::expect(farthest_path_optimizer.linkNextPath().size() > 0,
                                    "Expected farthest path optimizer to consume a zero-distance path.");
    passed &= ORNL::Testing::expect(farthest_path_optimizer.getCurrentPathCount() == 0,
                                    "Expected farthest path optimizer to make progress.");

    ORNL::PolylineOrderOptimizer polyline_optimizer(start, 0);
    ORNL::Polyline one_point_polyline;
    one_point_polyline.append(ORNL::Point(1.0f, 0.0f, 0.0f));
    QVector<ORNL::Polyline> polylines;
    polylines.append(ORNL::Polyline());
    polylines.append(one_point_polyline);
    polyline_optimizer.setGeometryToEvaluate(polylines, ORNL::RegionType::kSkeleton,
                                             ORNL::PathOrderOptimization::kNextClosest);
    passed &= ORNL::Testing::expect(polyline_optimizer.getCurrentPolylineCount() == 0,
                                    "Expected degenerate polylines to be filtered.");
    passed &= ORNL::Testing::expect(polyline_optimizer.linkNextPolyline().isEmpty(),
                                    "Expected empty polyline optimizer result.");

    ORNL::Polyline zero_distance_polyline;
    zero_distance_polyline.append(start);
    zero_distance_polyline.append(start);
    ORNL::PolylineOrderOptimizer farthest_polyline_optimizer(start, 0);
    farthest_polyline_optimizer.setPointParameters(ORNL::PointOrderOptimization::kNextClosest, false, ORNL::Distance(),
                                                   ORNL::Distance(), false, ORNL::Distance(), false);
    farthest_polyline_optimizer.setGeometryToEvaluate({zero_distance_polyline}, ORNL::RegionType::kInset,
                                                      ORNL::PathOrderOptimization::kNextFarthest);
    passed &= ORNL::Testing::expect(!farthest_polyline_optimizer.linkNextPolyline().isEmpty(),
                                    "Expected farthest polyline optimizer to consume a zero-distance polyline.");
    passed &= ORNL::Testing::expect(farthest_polyline_optimizer.getCurrentPolylineCount() == 0,
                                    "Expected farthest polyline optimizer to make progress.");

    ORNL::Polyline near_line;
    near_line.append(ORNL::Point(1.0f, 0.0f, 0.0f));
    near_line.append(ORNL::Point(1.0f, 1.0f, 0.0f));

    ORNL::Polyline far_line;
    far_line.append(ORNL::Point(10.0f, 0.0f, 0.0f));
    far_line.append(ORNL::Point(10.0f, 1.0f, 0.0f));

    ORNL::PolylineOrderOptimizer monotonic_open_polyline_optimizer(start, 0);
    monotonic_open_polyline_optimizer.setPointParameters(ORNL::PointOrderOptimization::kNextClosest, false,
                                                         ORNL::Distance(), ORNL::Distance(), false, ORNL::Distance(),
                                                         false);
    monotonic_open_polyline_optimizer.setInfillParameters(ORNL::InfillPatterns::kLines, ORNL::PolygonList(),
                                                          ORNL::Distance(), ORNL::Distance(), false);
    monotonic_open_polyline_optimizer.setGeometryToEvaluate({near_line, far_line}, ORNL::RegionType::kSkin,
                                                            ORNL::PathOrderOptimization::kNextFarthest);
    ORNL::Polyline monotonic_result = monotonic_open_polyline_optimizer.linkNextPolyline();
    passed &= ORNL::Testing::expect(!monotonic_result.isEmpty() && monotonic_result.front().x() == 1.0f,
                                    "Expected default line linking to keep monotonic front/back selection.");

    ORNL::PolylineOrderOptimizer ordered_open_polyline_optimizer(start, 0);
    ordered_open_polyline_optimizer.setPointParameters(ORNL::PointOrderOptimization::kNextClosest, false,
                                                       ORNL::Distance(), ORNL::Distance(), false, ORNL::Distance(),
                                                       false);
    ordered_open_polyline_optimizer.setInfillParameters(ORNL::InfillPatterns::kLines, ORNL::PolygonList(),
                                                        ORNL::Distance(), ORNL::Distance(), true);
    ordered_open_polyline_optimizer.setGeometryToEvaluate({near_line, far_line}, ORNL::RegionType::kSkin,
                                                          ORNL::PathOrderOptimization::kNextFarthest);
    ORNL::Polyline ordered_result = ordered_open_polyline_optimizer.linkNextPolyline();
    passed &= ORNL::Testing::expect(!ordered_result.isEmpty() && ordered_result.front().x() == 10.0f,
                                    "Expected ordered line linking to honor next farthest path order.");

    ORNL::Point radial_closest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer radial_closest_optimizer(
        radial_closest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    radial_closest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path radial_closest_result = radial_closest_optimizer.linkNextRadialPath();
    passed &= ORNL::Testing::expect(radial_closest_result.size() > 1 && radial_closest_result[1]->start().x() == 1.0f,
                                    "Expected radial linking to honor cylindrical next closest path order.");

    ORNL::Point radial_farthest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer radial_farthest_optimizer(
        radial_farthest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextFarthest, ORNL::PathOrderOptimization::kNextClosest));
    radial_farthest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path radial_farthest_result = radial_farthest_optimizer.linkNextRadialPath();
    passed &=
        ORNL::Testing::expect(radial_farthest_result.size() > 1 && radial_farthest_result[1]->start().x() == 10.0f,
                              "Expected radial linking to honor cylindrical next farthest path order.");

    ORNL::Path closed_radial_path =
        pathFromPoints({ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 10.0f, 0.0f),
                        ORNL::Point(0.0f, 10.0f, 0.0f), ORNL::Point(0.0f, 0.0f, 0.0f)});

    ORNL::Point closed_radial_closest_start(9.8f, 10.0f, 0.0f);
    ORNL::PathOrderOptimizer closed_radial_closest_optimizer(
        closed_radial_closest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    closed_radial_closest_optimizer.setPathsToEvaluate({closed_radial_path});
    ORNL::Path closed_radial_closest_result = closed_radial_closest_optimizer.linkNextRadialPath();
    passed &= ORNL::Testing::expect(closed_radial_closest_result.size() > 1 &&
                                        closed_radial_closest_result[1]->start() == ORNL::Point(10.0f, 10.0f, 0.0f),
                                    "Expected closed radial closest linking to rotate to the nearest segment start.");
    passed &=
        ORNL::Testing::expect(closed_radial_closest_result.size() > 1 &&
                                  closed_radial_closest_result[1]->end() == ORNL::Point(0.0f, 10.0f, 0.0f),
                              "Expected closed radial closest linking to preserve segment direction after rotation.");

    ORNL::Point closed_radial_farthest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer closed_radial_farthest_optimizer(
        closed_radial_farthest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextFarthest, ORNL::PathOrderOptimization::kNextClosest));
    closed_radial_farthest_optimizer.setPathsToEvaluate({closed_radial_path});
    ORNL::Path closed_radial_farthest_result = closed_radial_farthest_optimizer.linkNextRadialPath();
    passed &= ORNL::Testing::expect(closed_radial_farthest_result.size() > 1 &&
                                        closed_radial_farthest_result[1]->start() == ORNL::Point(10.0f, 10.0f, 0.0f),
                                    "Expected closed radial farthest linking to rotate to the farthest segment start.");

    QSharedPointer<ORNL::SettingsBase> closed_radial_consecutive_settings =
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest);
    closed_radial_consecutive_settings->setSetting(ORNL::PS::Optimizations::kPointOrder,
                                                   static_cast<int>(ORNL::PointOrderOptimization::kConsecutive));
    closed_radial_consecutive_settings->setSetting<ORNL::Distance>(
        ORNL::PS::Optimizations::kConsecutiveDistanceThreshold, ORNL::Distance(5.0));
    ORNL::Point closed_radial_consecutive_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer closed_radial_consecutive_optimizer(closed_radial_consecutive_start, 2,
                                                                 closed_radial_consecutive_settings);
    closed_radial_consecutive_optimizer.setPathsToEvaluate({closed_radial_path});
    ORNL::Path closed_radial_consecutive_result = closed_radial_consecutive_optimizer.linkNextRadialPath();
    passed &= expect(closed_radial_consecutive_result.size() == 6,
                     "Expected closed radial consecutive linking to split the selected print segment.");
    passed &= expect(closed_radial_consecutive_result[1]->start() == ORNL::Point(5.0f, 0.0f, 0.0f),
                     "Expected closed radial consecutive linking to start at the threshold split point.");
    passed &= expect(closed_radial_consecutive_result.back()->end() == ORNL::Point(5.0f, 0.0f, 0.0f),
                     "Expected closed radial consecutive linking to end at the threshold split point.");
    passed &=
        expect(closed_radial_consecutive_result.size() > 1 &&
                   closed_radial_consecutive_result[1]->getSb() != closed_radial_consecutive_result.back()->getSb(),
               "Expected split path segments to own independent settings.");

    ORNL::Point multi_path_consecutive_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer multi_path_consecutive_optimizer(multi_path_consecutive_start, 2,
                                                              closed_radial_consecutive_settings);
    multi_path_consecutive_optimizer.setPathsToEvaluate({closed_radial_path, closed_radial_path});
    const ORNL::Path first_consecutive_path  = multi_path_consecutive_optimizer.linkNextRadialPath();
    const ORNL::Path second_consecutive_path = multi_path_consecutive_optimizer.linkNextRadialPath();
    passed &=
        expect(first_consecutive_path.size() > 1 && first_consecutive_path[1]->start() == ORNL::Point(5.0f, 0.0f, 0.0f),
               "Expected the first radial path to use the previous-layer seam reference.");
    passed &= expect(
        second_consecutive_path.size() > 1 && second_consecutive_path[1]->start() == ORNL::Point(5.0f, 0.0f, 0.0f),
        "Expected every radial path in a layer to retain the same previous-layer seam reference.");

    ORNL::Path closed_radial_arc_path;
    const ORNL::Point arc_center(0.0f, 0.0f, 0.0f);
    closed_radial_arc_path.append(
        arcSegment(ORNL::Point(10.0f, 0.0f, 0.0f), ORNL::Point(0.0f, 10.0f, 0.0f), arc_center, true));
    closed_radial_arc_path.append(
        arcSegment(ORNL::Point(0.0f, 10.0f, 0.0f), ORNL::Point(-10.0f, 0.0f, 0.0f), arc_center, true));
    closed_radial_arc_path.append(
        arcSegment(ORNL::Point(-10.0f, 0.0f, 0.0f), ORNL::Point(0.0f, -10.0f, 0.0f), arc_center, true));
    closed_radial_arc_path.append(
        arcSegment(ORNL::Point(0.0f, -10.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f), arc_center, true));

    QSharedPointer<ORNL::SettingsBase> closed_radial_arc_settings =
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest);
    closed_radial_arc_settings->setSetting(ORNL::PS::Optimizations::kPointOrder,
                                           static_cast<int>(ORNL::PointOrderOptimization::kConsecutive));
    closed_radial_arc_settings->setSetting<ORNL::Distance>(ORNL::PS::Optimizations::kConsecutiveDistanceThreshold,
                                                           ORNL::Distance(5.0));
    ORNL::Point closed_radial_arc_start(10.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer closed_radial_arc_optimizer(closed_radial_arc_start, 2, closed_radial_arc_settings);
    closed_radial_arc_optimizer.setPathsToEvaluate({closed_radial_arc_path});
    ORNL::Path closed_radial_arc_result = closed_radial_arc_optimizer.linkNextRadialPath();

    const double quarter_sweep  = std::acos(-1.0) / 2.0;
    const double expected_angle = 5.0 / 10.0;
    const ORNL::Point expected_arc_split(10.0 * std::cos(expected_angle), 10.0 * std::sin(expected_angle), 0.0);
    const ORNL::ArcSegment* split_start_arc = closed_radial_arc_result.size() > 1
                                                  ? dynamic_cast<ORNL::ArcSegment*>(closed_radial_arc_result[1].data())
                                                  : nullptr;
    const ORNL::ArcSegment* split_end_arc =
        closed_radial_arc_result.size() > 0 ? dynamic_cast<ORNL::ArcSegment*>(closed_radial_arc_result.back().data())
                                            : nullptr;

    passed &= expect(closed_radial_arc_result.size() == 6,
                     "Expected closed radial consecutive linking to split an arc print segment.");
    passed &= expect(split_start_arc != nullptr && split_end_arc != nullptr,
                     "Expected closed radial consecutive arc linking to preserve arc segment types.");
    passed &= expect(
        closed_radial_arc_result.size() > 1 && pointClose(closed_radial_arc_result[1]->start(), expected_arc_split),
        "Expected closed radial consecutive arc linking to start on the selected arc.");
    passed &= expect(
        closed_radial_arc_result.size() > 0 && pointClose(closed_radial_arc_result.back()->end(), expected_arc_split),
        "Expected closed radial consecutive arc linking to end on the selected arc.");
    passed &= expect(split_start_arc != nullptr && split_end_arc != nullptr &&
                         closeTo(split_start_arc->angle()() + split_end_arc->angle()(), quarter_sweep),
                     "Expected closed radial consecutive arc linking to refresh split arc sweep angles.");

    ORNL::Path full_circle_arc_path;
    full_circle_arc_path.append(
        arcSegment(ORNL::Point(10.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f), arc_center, true));
    ORNL::Point full_circle_arc_start(10.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer full_circle_arc_optimizer(full_circle_arc_start, 2, closed_radial_arc_settings);
    full_circle_arc_optimizer.setPathsToEvaluate({full_circle_arc_path});
    const ORNL::Path full_circle_arc_result = full_circle_arc_optimizer.linkNextRadialPath();
    const ORNL::Point expected_full_circle_split(10.0 * std::cos(expected_angle), 10.0 * std::sin(expected_angle), 0.0);
    const ORNL::ArcSegment* full_circle_start_arc =
        full_circle_arc_result.size() > 1 ? dynamic_cast<ORNL::ArcSegment*>(full_circle_arc_result[1].data()) : nullptr;
    const ORNL::ArcSegment* full_circle_end_arc =
        full_circle_arc_result.size() > 0 ? dynamic_cast<ORNL::ArcSegment*>(full_circle_arc_result.back().data())
                                          : nullptr;
    passed &= expect(full_circle_arc_result.size() == 3,
                     "Expected a single full-circle arc to split at the consecutive threshold.");
    passed &= expect(
        full_circle_arc_result.size() > 1 && pointClose(full_circle_arc_result[1]->start(), expected_full_circle_split),
        "Expected the default one-arc revolution to rotate by physical arc distance.");
    passed &=
        expect(full_circle_start_arc != nullptr && full_circle_end_arc != nullptr &&
                   closeTo(full_circle_start_arc->angle()() + full_circle_end_arc->angle()(), 2.0 * std::acos(-1.0)),
               "Expected a split full-circle arc to preserve one complete revolution.");

    ORNL::Path open_radial_path = pathFromPoints({ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f),
                                                  ORNL::Point(10.0f, 10.0f, 0.0f), ORNL::Point(0.0f, 10.0f, 0.0f)});
    ORNL::Point open_radial_start(9.8f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer open_radial_optimizer(
        open_radial_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    open_radial_optimizer.setPathsToEvaluate({open_radial_path});
    ORNL::Path open_radial_result = open_radial_optimizer.linkNextRadialPath();
    passed &= ORNL::Testing::expect(
        open_radial_result.size() > 1 && open_radial_result[1]->start() == ORNL::Point(0.0f, 0.0f, 0.0f),
        "Expected open radial closest linking to remain endpoint-only.");

    QSharedPointer<ORNL::SettingsBase> radial_layer_settings =
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest);
    ORNL::CylindricalLayer radial_layer(0, radial_layer_settings, ORNL::CylindricalPathPattern::kRadial);
    radial_layer.addPath(pathFromPoints({ORNL::Point(100.0f, 0.0f, 0.0f), ORNL::Point(110.0f, 0.0f, 0.0f),
                                         ORNL::Point(110.0f, 10.0f, 0.0f), ORNL::Point(100.0f, 10.0f, 0.0f),
                                         ORNL::Point(100.0f, 0.0f, 0.0f)}));
    radial_layer.addPath(
        pathFromPoints({ORNL::Point(0.0f, 0.0f, 1.0f), ORNL::Point(10.0f, 0.0f, 1.0f), ORNL::Point(10.0f, 10.0f, 1.0f),
                        ORNL::Point(0.0f, 10.0f, 1.0f), ORNL::Point(0.0f, 0.0f, 1.0f)}));
    ORNL::Point radial_layer_current_location(9.8f, 10.0f, 0.0f);
    radial_layer.calculateModifiers(radial_layer_current_location);
    passed &=
        ORNL::Testing::expect(radial_layer_current_location == ORNL::Point(100.0f, 10.0f, 0.0f),
                              "Expected radial layer ordering to choose across all paths instead of same-Z groups.");
    passed &= ORNL::Testing::expect(helicalLayerReversalPreservesRegionsAndTransitions(),
                                    "Expected helical layer reversal to preserve print regions and transition starts.");
    passed &= ORNL::Testing::expect(helicalLayerRecomputesSameRegionStartFlags(),
                                    "Expected helical layer reversal to recompute same-region start flags.");

    ORNL::Point helical_closest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer helical_closest_optimizer(
        helical_closest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    helical_closest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path helical_closest_result = helical_closest_optimizer.linkNextHelicalPath();
    passed &= ORNL::Testing::expect(helical_closest_result.size() > 1 && helical_closest_result[1]->start().x() == 1.0f,
                                    "Expected helical linking to honor cylindrical next closest path order.");
    passed &= ORNL::Testing::expect(
        helical_closest_result.size() > 1 && helical_closest_result.back()->end().x() == 2.0f,
        "Expected helical closest linking to keep forward direction when the start endpoint is selected.");

    ORNL::Point helical_closest_reverse_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer helical_closest_reverse_optimizer(
        helical_closest_reverse_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    helical_closest_reverse_optimizer.setPathsToEvaluate({linePath(10.0f, 1.0f), linePath(20.0f, 21.0f)});
    ORNL::Path helical_closest_reverse_result = helical_closest_reverse_optimizer.linkNextHelicalPath();
    passed &= ORNL::Testing::expect(
        helical_closest_reverse_result.size() > 1 && helical_closest_reverse_result[1]->start().x() == 1.0f,
        "Expected helical closest linking to enter from the nearest end endpoint.");
    passed &= ORNL::Testing::expect(
        helical_closest_reverse_result.size() > 1 && helical_closest_reverse_result.back()->end().x() == 10.0f,
        "Expected helical closest linking to reverse the fragment when entering from the end endpoint.");

    ORNL::Point helical_farthest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer helical_farthest_optimizer(
        helical_farthest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextFarthest, ORNL::PathOrderOptimization::kNextClosest));
    helical_farthest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path helical_farthest_result = helical_farthest_optimizer.linkNextHelicalPath();
    passed &=
        ORNL::Testing::expect(helical_farthest_result.size() > 1 && helical_farthest_result[1]->start().x() == 11.0f,
                              "Expected helical farthest linking to enter from the farthest end endpoint.");
    passed &= ORNL::Testing::expect(
        helical_farthest_result.size() > 1 && helical_farthest_result.back()->end().x() == 10.0f,
        "Expected helical farthest linking to reverse the fragment when entering from the end endpoint.");

    passed &=
        ORNL::Testing::expect(ORNL::optionalPathOrderOptimization(0, ORNL::PathOrderOptimization::kNextFarthest) ==
                                  ORNL::PathOrderOptimization::kNextFarthest,
                              "Expected optional path order 0 to use the fallback order.");
    passed &= ORNL::Testing::expect(ORNL::optionalPathOrderOptimization(2, ORNL::PathOrderOptimization::kNextClosest) ==
                                        ORNL::PathOrderOptimization::kNextFarthest,
                                    "Expected optional path order 2 to map to next farthest.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
