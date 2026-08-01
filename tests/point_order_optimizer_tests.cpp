#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "optimizers/point_order_optimizer.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

bool closeTo(double lhs, double rhs) { return std::abs(lhs - rhs) <= 1.0e-5; }

ORNL::Polyline squareLoop() {
    return ORNL::Polyline {ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f),
                           ORNL::Point(10.0f, 10.0f, 0.0f), ORNL::Point(0.0f, 10.0f, 0.0f)};
}
} // namespace

int main() {
    bool passed = true;
    const ORNL::Polyline polyline = squareLoop();

    const auto physical_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(5.0), false, ORNL::Distance(0.0), false,
        std::optional<ORNL::Point>(ORNL::Point(0.0f, 0.0f, 20.0f)));

    passed &= expect(physical_selection.insert_split_point,
                     "Expected consecutive physical selection to split at the requested XY distance.");
    passed &= expect(physical_selection.insertion_index == 1,
                     "Expected consecutive split to be inserted before the second square vertex.");
    passed &= expect(closeTo(physical_selection.split_point.x(), 5.0) &&
                         closeTo(physical_selection.split_point.y(), 0.0),
                     "Expected consecutive split point at (5, 0).");

    const auto middle_reference_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(5.0), false, ORNL::Distance(0.0), false,
        std::optional<ORNL::Point>(ORNL::Point(2.0f, 0.0f, 20.0f)));

    passed &= expect(middle_reference_selection.insert_split_point,
                     "Expected consecutive physical selection to split from a mid-edge prior start.");
    passed &= expect(closeTo(middle_reference_selection.split_point.x(), 7.0) &&
                         closeTo(middle_reference_selection.split_point.y(), 0.0),
                     "Expected mid-edge consecutive split point at (7, 0).");

    const auto legacy_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(5.0), false, ORNL::Distance(0.0), false);

    passed &= expect(!legacy_selection.insert_split_point,
                     "Expected legacy consecutive selection without a prior start to use an existing vertex.");
    passed &= expect(legacy_selection.rotation_index == 3,
                     "Expected legacy consecutive fallback to preserve layer-index selection.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
