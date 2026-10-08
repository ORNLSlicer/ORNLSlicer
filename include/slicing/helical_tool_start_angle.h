#pragma once

#include <optional>

#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL::HelicalToolStartAngle {
/*!
 * @brief Converts a physical arc-length offset to the angular offset for one helical path radius.
 * @param arc_length_offset Signed physical offset along the helical path circumference.
 * @param radius Actual XY radius of the generated helical path centerline.
 * @return The signed angular offset, zero for a zero arc-length request, or no value when a nonzero request cannot be
 * represented at the supplied radius.
 */
std::optional<Angle> angleOffsetForRadius(Distance arc_length_offset, Distance radius);

/*!
 * @brief Returns the signed helical tool start-angle offset for one generated radius pass.
 * @param radius_angle_offset Arc-length offset converted for the generated path radius.
 * @param starts_from_generated_end Whether the ordered path prints from the generated path end.
 * @param z_clip_rounding User-selected helical Z clip rounding.
 * @param path_order Resolved cylindrical path order.
 * @return Configured or direction-mirrored offset for this ordered path.
 */
Angle effectiveOffset(Angle radius_angle_offset, bool starts_from_generated_end,
                      HelicalPathZClipRounding z_clip_rounding, PathOrderOptimization path_order);

/*!
 * @brief Returns the fixed helical geometry start angle.
 * @return Top-dead-center start angle used by emitted helical X/Y coordinates.
 */
Angle geometricStartAngle();
}  // namespace ORNL::HelicalToolStartAngle
