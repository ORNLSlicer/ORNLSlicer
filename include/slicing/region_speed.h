#pragma once

#include "configs/settings_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
//! \brief Resolves the print speed used by generated segments and ARC weld schedules.
inline Velocity regionPrintSpeed(const SettingsBase& settings, RegionType region) {
    if (static_cast<SlicingMode>(settings.setting<int>(PS::Slicing::kSlicingMode)) == SlicingMode::kCylindrical &&
        settings.setting<CylindricalPathPattern>(PS::Slicing::kCylindricalPathPattern) ==
            CylindricalPathPattern::kRadial) {
        const Velocity speed = settings.setting<Velocity>(PS::Layer::kSpeed);
        // Match the ARC writer's existing fallback for nonpositive radial speeds.
        return speed > 0 ? speed : 10.0 * mm / s;
    }

    switch (region) {
        case RegionType::kPerimeter:
            return settings.setting<Velocity>(PS::Perimeter::kSpeed);
        case RegionType::kInset:
            return settings.setting<Velocity>(PS::Inset::kSpeed);
        case RegionType::kInfill:
            return settings.setting<Velocity>(PS::Infill::kSpeed);
        case RegionType::kSkeleton:
            return settings.setting<Velocity>(PS::Skeleton::kSpeed);
        default:
            return settings.setting<Velocity>(PS::Layer::kSpeed);
    }
}
}  // namespace ORNL
