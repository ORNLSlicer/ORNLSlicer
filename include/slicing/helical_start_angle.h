#pragma once

#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL::HelicalStartAngle {
/*!
 * @brief Returns the signed helical start-angle offset for one generated radius pass.
 * @param configured_offset User-configured offset from top dead center.
 * @param radius_pass_index Zero-based generated helical radius pass index.
 * @param z_clip_rounding User-selected helical Z clip rounding.
 * @param path_order Resolved cylindrical path order.
 * @return Configured or direction-mirrored offset for this radius pass.
 */
Angle effectiveOffset(Angle configured_offset, int radius_pass_index, HelicalPathZClipRounding z_clip_rounding,
                      PathOrderOptimization path_order);

/*!
 * @brief Returns the fixed helical geometry start angle.
 * @return Top-dead-center start angle used by emitted helical X/Y coordinates.
 */
Angle geometricStartAngle();
}  // namespace ORNL::HelicalStartAngle
