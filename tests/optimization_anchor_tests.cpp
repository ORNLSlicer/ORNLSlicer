#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include <QSharedPointer>
#include <QVector3D>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "optimizers/optimization_anchor.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

bool closeTo(double lhs, double rhs) { return std::abs(lhs - rhs) <= 1.0e-5; }

QSharedPointer<ORNL::SettingsBase> settingsWithSeamAttractor(ORNL::PointOrderOptimization point_order) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Optimizations::kIslandOrder,
                         static_cast<int>(ORNL::IslandOrderOptimization::kNextClosest));
    settings->setSetting(ORNL::PS::Optimizations::kPathOrder,
                         static_cast<int>(ORNL::PathOrderOptimization::kNextClosest));
    settings->setSetting(ORNL::PS::Optimizations::kPointOrder, static_cast<int>(point_order));
    settings->setSetting(ORNL::PS::Optimizations::kCustomIslandXLocation, 2.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomIslandYLocation, 3.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomIslandZLocation, 0.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomPathXLocation, 2.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomPathYLocation, 3.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomPathZLocation, 0.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomPointXLocation, 2.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomPointYLocation, 3.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomPointZLocation, 0.0);
    settings->setSetting(ORNL::PS::Optimizations::kSeamAttractorVectorX, 1.0);
    settings->setSetting(ORNL::PS::Optimizations::kSeamAttractorVectorY, 0.0);
    settings->setSetting(ORNL::PS::Optimizations::kSeamAttractorVectorZ, 1.0);

    return settings;
}
} // namespace

int main() {
    bool passed = true;
    const ORNL::Plane slicing_plane(ORNL::Point(0.0f, 0.0f, 10.0f), QVector3D(0.0f, 0.0f, 1.0f));
    const ORNL::Point optimization_shift(0.0f, 0.0f, 0.0f);

    const ORNL::Point next_closest_anchor = ORNL::OptimizationAnchor::customPointOrderPoint(
        settingsWithSeamAttractor(ORNL::PointOrderOptimization::kNextClosest), slicing_plane, optimization_shift);
    passed &= expect(closeTo(next_closest_anchor.x(), 2.0) && closeTo(next_closest_anchor.y(), 3.0) &&
                         closeTo(next_closest_anchor.z(), 10.0),
                     "Expected seam attractor vector to be ignored outside Custom Location point order.");

    QSharedPointer<ORNL::SettingsBase> custom_island_settings =
        settingsWithSeamAttractor(ORNL::PointOrderOptimization::kNextClosest);
    custom_island_settings->setSetting(ORNL::PS::Optimizations::kIslandOrder,
                                       static_cast<int>(ORNL::IslandOrderOptimization::kCustomPoint));
    const ORNL::Point custom_island_anchor = ORNL::OptimizationAnchor::customIslandOrderPoint(
        custom_island_settings, slicing_plane, optimization_shift);
    passed &= expect(closeTo(custom_island_anchor.x(), 12.0) && closeTo(custom_island_anchor.y(), 3.0) &&
                         closeTo(custom_island_anchor.z(), 10.0),
                     "Expected seam attractor vector to project the anchor for Custom Island Location order.");

    QSharedPointer<ORNL::SettingsBase> custom_path_settings =
        settingsWithSeamAttractor(ORNL::PointOrderOptimization::kNextClosest);
    custom_path_settings->setSetting(ORNL::PS::Optimizations::kPathOrder,
                                     static_cast<int>(ORNL::PathOrderOptimization::kCustomPoint));
    const ORNL::Point custom_path_anchor =
        ORNL::OptimizationAnchor::customPathOrderPoint(custom_path_settings, slicing_plane, optimization_shift);
    passed &= expect(closeTo(custom_path_anchor.x(), 12.0) && closeTo(custom_path_anchor.y(), 3.0) &&
                         closeTo(custom_path_anchor.z(), 10.0),
                     "Expected seam attractor vector to project the anchor for Custom Path Location order.");

    const ORNL::Point custom_farthest_anchor = ORNL::OptimizationAnchor::customPointOrderPoint(
        settingsWithSeamAttractor(ORNL::PointOrderOptimization::kCustomFarthestPoint), slicing_plane,
        optimization_shift);
    passed &= expect(closeTo(custom_farthest_anchor.x(), 12.0) && closeTo(custom_farthest_anchor.y(), 3.0) &&
                         closeTo(custom_farthest_anchor.z(), 10.0),
                     "Expected seam attractor vector to project the anchor for Custom Farthest Location point order.");

    const ORNL::Point custom_anchor = ORNL::OptimizationAnchor::customPointOrderPoint(
        settingsWithSeamAttractor(ORNL::PointOrderOptimization::kCustomPoint), slicing_plane, optimization_shift);
    passed &= expect(closeTo(custom_anchor.x(), 12.0) && closeTo(custom_anchor.y(), 3.0) &&
                         closeTo(custom_anchor.z(), 10.0),
                     "Expected seam attractor vector to project the anchor for Custom Location point order.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
