#include <cstdlib>
#include <iostream>
#include <string>

#include <QList>
#include <QSharedPointer>
#include <QVector>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polyline.h"
#include "optimizers/island_order_optimizer.h"
#include "optimizers/path_order_optimizer.h"
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/island/island_base.h"
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

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
