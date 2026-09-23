#include <cstdlib>
#include <optional>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "optimizers/point_order_optimizer.h"
#include "test_utils.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
constexpr float kTolerance = 1.0e-5f;

ORNL::Polyline squareLoop() {
    return ORNL::Polyline {ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(10.0f, 0.0f, 0.0f),
                           ORNL::Point(10.0f, 10.0f, 0.0f), ORNL::Point(0.0f, 10.0f, 0.0f)};
}
}  // namespace

int main() {
    bool passed                   = true;
    const ORNL::Polyline polyline = squareLoop();

    passed &= expect(ORNL::PS::Optimizations::kConsecutiveDistanceThreshold == "consecutive_distance_threshold",
                     "Expected consecutive threshold setting key to match project/resource setting name.");

    const auto physical_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(5.0), false, ORNL::Distance(0.0), false,
        std::optional<ORNL::Point>(ORNL::Point(0.0f, 0.0f, 20.0f)));

    passed &= ORNL::Testing::expect(physical_selection.insert_split_point,
                                    "Expected consecutive physical selection to split at the requested XY distance.");
    passed &= ORNL::Testing::expect(physical_selection.insertion_index == 1,
                                    "Expected consecutive split to be inserted before the second square vertex.");
    passed &= ORNL::Testing::expect(ORNL::Testing::near2DPoint(physical_selection.split_point, 5.0, 0.0, kTolerance),
                                    "Expected consecutive split point at (5, 0).");

    const auto randomized_split_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(5.0), true, ORNL::Distance(4.0), false,
        std::optional<ORNL::Point>(ORNL::Point(0.0f, 0.0f, 20.0f)));

    passed &= ORNL::Testing::expect(randomized_split_selection.insert_split_point,
                                    "Expected local randomness with no candidates to preserve the split selection.");
    passed &=
        ORNL::Testing::expect(ORNL::Testing::near2DPoint(randomized_split_selection.split_point, 5.0, 0.0, kTolerance),
                              "Expected preserved randomized split point at (5, 0).");

    const auto middle_reference_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(5.0), false, ORNL::Distance(0.0), false,
        std::optional<ORNL::Point>(ORNL::Point(2.0f, 0.0f, 20.0f)));

    passed &= ORNL::Testing::expect(middle_reference_selection.insert_split_point,
                                    "Expected consecutive physical selection to split from a mid-edge prior start.");
    passed &=
        ORNL::Testing::expect(ORNL::Testing::near2DPoint(middle_reference_selection.split_point, 7.0, 0.0, kTolerance),
                              "Expected mid-edge consecutive split point at (7, 0).");

    const auto corner_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(15.0), false, ORNL::Distance(0.0), false,
        std::optional<ORNL::Point>(ORNL::Point(0.0f, 0.0f, 20.0f)));

    passed &= expect(corner_selection.insert_split_point,
                     "Expected consecutive physical selection to split after walking around a corner.");
    passed &= expect(corner_selection.insertion_index == 2,
                     "Expected around-corner consecutive split to be inserted before the third square vertex.");
    passed &= expect(closeTo(corner_selection.split_point.x(), 10.0) && closeTo(corner_selection.split_point.y(), 5.0),
                     "Expected around-corner consecutive split point at (10, 5).");

    const auto wrapped_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(39.0), false, ORNL::Distance(0.0), false,
        std::optional<ORNL::Point>(ORNL::Point(2.0f, 0.0f, 20.0f)));

    passed &= expect(wrapped_selection.insert_split_point,
                     "Expected consecutive physical selection to traverse the final partial edge.");
    passed &= expect(wrapped_selection.insertion_index == 1,
                     "Expected wrapped consecutive split to remain on the first square edge.");
    passed &= expect(closeTo(wrapped_selection.split_point.x(), 1.0) && closeTo(wrapped_selection.split_point.y(), 0.0),
                     "Expected wrapped consecutive split point at (1, 0).");

    const auto legacy_selection = ORNL::PointOrderOptimizer::linkToPoint(
        ORNL::Point(999.0f, 999.0f, 0.0f), polyline, 4, ORNL::PointOrderOptimization::kConsecutive, false,
        ORNL::Distance(0.0), ORNL::Distance(5.0), false, ORNL::Distance(0.0), false);

    passed &=
        ORNL::Testing::expect(!legacy_selection.insert_split_point,
                              "Expected legacy consecutive selection without a prior start to use an existing vertex.");
    passed &= ORNL::Testing::expect(legacy_selection.rotation_index == 3,
                                    "Expected legacy consecutive fallback to preserve layer-index selection.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
