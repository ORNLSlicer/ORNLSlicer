#pragma once

#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL::HelicalToolStartAngle {
/*!
 * @brief Returns the signed helical tool start-angle offset for one generated radius pass.
 * @param configured_tool_offset User-configured tool/CP offset at top dead center.
 * @param radius_pass_index Zero-based generated helical radius pass index.
 * @param z_clip_rounding User-selected helical Z clip rounding.
 * @param path_order Resolved cylindrical path order.
 * @return Configured or direction-mirrored offset for this radius pass.
 */
Angle effectiveOffset(Angle configured_tool_offset, int radius_pass_index, HelicalPathZClipRounding z_clip_rounding,
                      PathOrderOptimization path_order);

/*!
 * @brief Returns the fixed helical geometry start angle.
 * @return Top-dead-center start angle used by emitted helical X/Y coordinates.
 */
Angle geometricStartAngle();
}  // namespace ORNL::HelicalToolStartAngle
