#include "slicing/helical_tool_start_angle.h"

namespace ORNL::HelicalToolStartAngle {
namespace {
const Angle kTopDeadCenterStartAngle = 90.0 * degree;
}

Angle effectiveOffset(Angle configured_tool_offset, bool starts_from_generated_end,
                      HelicalPathZClipRounding z_clip_rounding, PathOrderOptimization path_order) {
    const bool ordered_path_prints_from_generated_end =
        z_clip_rounding == HelicalPathZClipRounding::kCompleteRevolution &&
        path_order == PathOrderOptimization::kNextClosest && starts_from_generated_end;
    return ordered_path_prints_from_generated_end ? -configured_tool_offset : configured_tool_offset;
}

Angle geometricStartAngle() {
    return kTopDeadCenterStartAngle;
}
}  // namespace ORNL::HelicalToolStartAngle
