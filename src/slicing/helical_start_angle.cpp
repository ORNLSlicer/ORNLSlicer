#include "slicing/helical_start_angle.h"

namespace ORNL::HelicalStartAngle {
namespace {
const Angle kTopDeadCenterStartAngle = 90.0 * degree;
}

Angle effectiveOffset(Angle configured_offset, int radius_pass_index, HelicalPathZClipRounding z_clip_rounding,
                      PathOrderOptimization path_order) {
    const bool next_radius_prints_from_generated_end =
        z_clip_rounding == HelicalPathZClipRounding::kCompleteRevolution &&
        path_order == PathOrderOptimization::kNextClosest && radius_pass_index % 2 != 0;
    return next_radius_prints_from_generated_end ? -configured_offset : configured_offset;
}

Angle geometricStartAngle() {
    return kTopDeadCenterStartAngle;
}
}  // namespace ORNL::HelicalStartAngle
