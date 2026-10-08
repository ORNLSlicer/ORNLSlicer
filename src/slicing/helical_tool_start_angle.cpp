#include "slicing/helical_tool_start_angle.h"

#include <cmath>
#include <limits>

namespace ORNL::HelicalToolStartAngle {
namespace {
const Angle kTopDeadCenterStartAngle = 90.0 * degree;
}

std::optional<Angle> angleOffsetForRadius(Distance arc_length_offset, Distance radius) {
    if (!std::isfinite(arc_length_offset())) { return std::nullopt; }
    if (std::abs(arc_length_offset()) <= std::numeric_limits<double>::epsilon()) { return 0.0 * radian; }
    if (!std::isfinite(radius()) || radius() <= std::numeric_limits<double>::epsilon()) { return std::nullopt; }

    return Angle(arc_length_offset() / radius());
}

Angle effectiveOffset(Angle radius_angle_offset, bool starts_from_generated_end,
                      HelicalPathZClipRounding z_clip_rounding, PathOrderOptimization path_order) {
    const bool ordered_path_prints_from_generated_end =
        z_clip_rounding == HelicalPathZClipRounding::kCompleteRevolution &&
        path_order == PathOrderOptimization::kNextClosest && starts_from_generated_end;
    return ordered_path_prints_from_generated_end ? -radius_angle_offset : radius_angle_offset;
}

Angle geometricStartAngle() {
    return kTopDeadCenterStartAngle;
}
}  // namespace ORNL::HelicalToolStartAngle
