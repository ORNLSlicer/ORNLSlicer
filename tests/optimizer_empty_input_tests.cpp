#include <QList>
#include <QSharedPointer>
#include <QVector>
#include <cstdlib>
#include <iostream>
#include <string>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segments/line.h"
#include "optimizers/island_order_optimizer.h"
#include "optimizers/path_order_optimizer.h"
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/radial_layer.h"
#include "step/layer/island/island_base.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

QSharedPointer<ORNL::LineSegment> lineSegment(const ORNL::Point& start, const ORNL::Point& end) {
    QSharedPointer<ORNL::LineSegment> segment = QSharedPointer<ORNL::LineSegment>::create(start, end);
    segment->getSb()->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);
    return segment;
}

ORNL::Path linePath(float start_x, float end_x) {
    ORNL::Path path;
    path.append(lineSegment(ORNL::Point(start_x, 0.0f, 0.0f), ORNL::Point(end_x, 0.0f, 0.0f)));
    return path;
}

ORNL::Path pathFromPoints(const QVector<ORNL::Point>& points) {
    ORNL::Path path;
    for (int i = 0, end = points.size() - 1; i < end; ++i) {
        path.append(lineSegment(points[i], points[i + 1]));
    }

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
    return settings;
}
} // namespace

int main() {
    bool passed = true;

    ORNL::Point start(0.0f, 0.0f, 0.0f);

    ORNL::IslandBaseOrderOptimizer island_optimizer(start, QList<QSharedPointer<ORNL::IslandBase>>(), -1);
    passed &= expect(island_optimizer.computeNextIndex() == -1, "Expected empty island optimizer to return -1.");

    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    ORNL::PathOrderOptimizer path_optimizer(start, 0, settings);
    QVector<ORNL::Path> paths;
    paths.append(ORNL::Path());
    path_optimizer.setPathsToEvaluate(paths);
    passed &= expect(path_optimizer.getCurrentPathCount() == 0, "Expected empty paths to be filtered.");
    passed &= expect(path_optimizer.linkNextPath().size() == 0, "Expected empty path optimizer result.");

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
    passed &= expect(farthest_path_optimizer.linkNextPath().size() > 0,
                     "Expected farthest path optimizer to consume a zero-distance path.");
    passed &= expect(farthest_path_optimizer.getCurrentPathCount() == 0,
                     "Expected farthest path optimizer to make progress.");

    ORNL::PolylineOrderOptimizer polyline_optimizer(start, 0);
    ORNL::Polyline one_point_polyline;
    one_point_polyline.append(ORNL::Point(1.0f, 0.0f, 0.0f));
    QVector<ORNL::Polyline> polylines;
    polylines.append(ORNL::Polyline());
    polylines.append(one_point_polyline);
    polyline_optimizer.setGeometryToEvaluate(polylines, ORNL::RegionType::kSkeleton,
                                             ORNL::PathOrderOptimization::kNextClosest);
    passed &=
        expect(polyline_optimizer.getCurrentPolylineCount() == 0, "Expected degenerate polylines to be filtered.");
    passed &= expect(polyline_optimizer.linkNextPolyline().isEmpty(), "Expected empty polyline optimizer result.");

    ORNL::Polyline zero_distance_polyline;
    zero_distance_polyline.append(start);
    zero_distance_polyline.append(start);
    ORNL::PolylineOrderOptimizer farthest_polyline_optimizer(start, 0);
    farthest_polyline_optimizer.setPointParameters(ORNL::PointOrderOptimization::kNextClosest, false, ORNL::Distance(),
                                                   ORNL::Distance(), false, ORNL::Distance(), false);
    farthest_polyline_optimizer.setGeometryToEvaluate({zero_distance_polyline}, ORNL::RegionType::kInset,
                                                      ORNL::PathOrderOptimization::kNextFarthest);
    passed &= expect(!farthest_polyline_optimizer.linkNextPolyline().isEmpty(),
                     "Expected farthest polyline optimizer to consume a zero-distance polyline.");
    passed &= expect(farthest_polyline_optimizer.getCurrentPolylineCount() == 0,
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
    passed &= expect(!monotonic_result.isEmpty() && monotonic_result.front().x() == 1.0f,
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
    passed &= expect(!ordered_result.isEmpty() && ordered_result.front().x() == 10.0f,
                     "Expected ordered line linking to honor next farthest path order.");

    ORNL::Point radial_closest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer radial_closest_optimizer(
        radial_closest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    radial_closest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path radial_closest_result = radial_closest_optimizer.linkNextRadialPath();
    passed &= expect(radial_closest_result.size() > 1 && radial_closest_result[1]->start().x() == 1.0f,
                     "Expected radial linking to honor cylindrical next closest path order.");

    ORNL::Point radial_farthest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer radial_farthest_optimizer(
        radial_farthest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextFarthest, ORNL::PathOrderOptimization::kNextClosest));
    radial_farthest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path radial_farthest_result = radial_farthest_optimizer.linkNextRadialPath();
    passed &= expect(radial_farthest_result.size() > 1 && radial_farthest_result[1]->start().x() == 10.0f,
                     "Expected radial linking to honor cylindrical next farthest path order.");

    ORNL::Path closed_radial_path =
        pathFromPoints({ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f),
                        ORNL::Point(10.0f, 10.0f, 0.0f), ORNL::Point(0.0f, 10.0f, 0.0f),
                        ORNL::Point(0.0f, 0.0f, 0.0f)});

    ORNL::Point closed_radial_closest_start(9.8f, 10.0f, 0.0f);
    ORNL::PathOrderOptimizer closed_radial_closest_optimizer(
        closed_radial_closest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    closed_radial_closest_optimizer.setPathsToEvaluate({closed_radial_path});
    ORNL::Path closed_radial_closest_result = closed_radial_closest_optimizer.linkNextRadialPath();
    passed &= expect(closed_radial_closest_result.size() > 1 &&
                         closed_radial_closest_result[1]->start() == ORNL::Point(10.0f, 10.0f, 0.0f),
                     "Expected closed radial closest linking to rotate to the nearest segment start.");
    passed &= expect(closed_radial_closest_result.size() > 1 &&
                         closed_radial_closest_result[1]->end() == ORNL::Point(0.0f, 10.0f, 0.0f),
                     "Expected closed radial closest linking to preserve segment direction after rotation.");

    ORNL::Point closed_radial_farthest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer closed_radial_farthest_optimizer(
        closed_radial_farthest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextFarthest, ORNL::PathOrderOptimization::kNextClosest));
    closed_radial_farthest_optimizer.setPathsToEvaluate({closed_radial_path});
    ORNL::Path closed_radial_farthest_result = closed_radial_farthest_optimizer.linkNextRadialPath();
    passed &= expect(closed_radial_farthest_result.size() > 1 &&
                         closed_radial_farthest_result[1]->start() == ORNL::Point(10.0f, 10.0f, 0.0f),
                     "Expected closed radial farthest linking to rotate to the farthest segment start.");

    ORNL::Path open_radial_path =
        pathFromPoints({ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f),
                        ORNL::Point(10.0f, 10.0f, 0.0f), ORNL::Point(0.0f, 10.0f, 0.0f)});
    ORNL::Point open_radial_start(9.8f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer open_radial_optimizer(
        open_radial_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    open_radial_optimizer.setPathsToEvaluate({open_radial_path});
    ORNL::Path open_radial_result = open_radial_optimizer.linkNextRadialPath();
    passed &= expect(open_radial_result.size() > 1 &&
                         open_radial_result[1]->start() == ORNL::Point(0.0f, 0.0f, 0.0f),
                     "Expected open radial closest linking to remain endpoint-only.");

    QSharedPointer<ORNL::SettingsBase> radial_layer_settings =
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest);
    ORNL::RadialLayer radial_layer(0, radial_layer_settings);
    radial_layer.addPath(pathFromPoints({ORNL::Point(100.0f, 0.0f, 0.0f), ORNL::Point(110.0f, 0.0f, 0.0f),
                                         ORNL::Point(110.0f, 10.0f, 0.0f), ORNL::Point(100.0f, 10.0f, 0.0f),
                                         ORNL::Point(100.0f, 0.0f, 0.0f)}));
    radial_layer.addPath(pathFromPoints({ORNL::Point(0.0f, 0.0f, 1.0f), ORNL::Point(10.0f, 0.0f, 1.0f),
                                         ORNL::Point(10.0f, 10.0f, 1.0f), ORNL::Point(0.0f, 10.0f, 1.0f),
                                         ORNL::Point(0.0f, 0.0f, 1.0f)}));
    ORNL::Point radial_layer_current_location(9.8f, 10.0f, 0.0f);
    radial_layer.calculateModifiers(radial_layer_current_location);
    passed &= expect(radial_layer_current_location == ORNL::Point(100.0f, 10.0f, 0.0f),
                     "Expected radial layer ordering to choose across all paths instead of same-Z groups.");

    ORNL::Point helical_closest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer helical_closest_optimizer(
        helical_closest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    helical_closest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path helical_closest_result = helical_closest_optimizer.linkNextHelicalPath();
    passed &= expect(helical_closest_result.size() > 1 && helical_closest_result[1]->start().x() == 1.0f,
                     "Expected helical linking to honor cylindrical next closest path order.");
    passed &= expect(helical_closest_result.size() > 1 && helical_closest_result.back()->end().x() == 2.0f,
                     "Expected helical closest linking to keep forward direction when the start endpoint is selected.");

    ORNL::Point helical_closest_reverse_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer helical_closest_reverse_optimizer(
        helical_closest_reverse_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextClosest, ORNL::PathOrderOptimization::kNextFarthest));
    helical_closest_reverse_optimizer.setPathsToEvaluate({linePath(10.0f, 1.0f), linePath(20.0f, 21.0f)});
    ORNL::Path helical_closest_reverse_result = helical_closest_reverse_optimizer.linkNextHelicalPath();
    passed &= expect(helical_closest_reverse_result.size() > 1 &&
                         helical_closest_reverse_result[1]->start().x() == 1.0f,
                     "Expected helical closest linking to enter from the nearest end endpoint.");
    passed &= expect(helical_closest_reverse_result.size() > 1 &&
                         helical_closest_reverse_result.back()->end().x() == 10.0f,
                     "Expected helical closest linking to reverse the fragment when entering from the end endpoint.");

    ORNL::Point helical_farthest_start(0.0f, 0.0f, 0.0f);
    ORNL::PathOrderOptimizer helical_farthest_optimizer(
        helical_farthest_start, 0,
        cylindricalSettings(ORNL::PathOrderOptimization::kNextFarthest, ORNL::PathOrderOptimization::kNextClosest));
    helical_farthest_optimizer.setPathsToEvaluate({linePath(1.0f, 2.0f), linePath(10.0f, 11.0f)});
    ORNL::Path helical_farthest_result = helical_farthest_optimizer.linkNextHelicalPath();
    passed &= expect(helical_farthest_result.size() > 1 && helical_farthest_result[1]->start().x() == 11.0f,
                     "Expected helical farthest linking to enter from the farthest end endpoint.");
    passed &= expect(helical_farthest_result.size() > 1 && helical_farthest_result.back()->end().x() == 10.0f,
                     "Expected helical farthest linking to reverse the fragment when entering from the end endpoint.");

    passed &= expect(ORNL::optionalPathOrderOptimization(0, ORNL::PathOrderOptimization::kNextFarthest) ==
                         ORNL::PathOrderOptimization::kNextFarthest,
                     "Expected optional path order 0 to use the fallback order.");
    passed &= expect(ORNL::optionalPathOrderOptimization(2, ORNL::PathOrderOptimization::kNextClosest) ==
                         ORNL::PathOrderOptimization::kNextFarthest,
                     "Expected optional path order 2 to map to next farthest.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
