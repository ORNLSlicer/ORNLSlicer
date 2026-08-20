#include <cstdlib>
#include <iostream>
#include <string>

#include <QList>
#include <QSharedPointer>
#include <QVector>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segments/line.h"
#include "optimizers/island_order_optimizer.h"
#include "optimizers/path_order_optimizer.h"
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/island/island_base.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
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
    passed &= expect(polyline_optimizer.getCurrentPolylineCount() == 0, "Expected degenerate polylines to be filtered.");
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

    passed &= expect(ORNL::optionalPathOrderOptimization(0, ORNL::PathOrderOptimization::kNextFarthest) ==
                         ORNL::PathOrderOptimization::kNextFarthest,
                     "Expected optional path order 0 to use the fallback order.");
    passed &= expect(ORNL::optionalPathOrderOptimization(2, ORNL::PathOrderOptimization::kNextClosest) ==
                         ORNL::PathOrderOptimization::kNextFarthest,
                     "Expected optional path order 2 to map to next farthest.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
