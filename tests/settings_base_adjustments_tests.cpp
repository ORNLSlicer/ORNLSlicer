#include <cstdlib>
#include <iostream>

#include "configs/settings_base.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

void enableFrictionStirIncompatibleSettings(ORNL::SettingsBase& settings) {
    settings.setSetting(ORNL::PRS::MachineSetup::kMachineType,
                        static_cast<int>(ORNL::MachineType::kFrictionStir));
    settings.setSetting(ORNL::PS::SpecialModes::kEnableWidthHeight, true);
    settings.setSetting(ORNL::PS::Support::kEnable, true);
    settings.setSetting(ORNL::ES::Ramping::kTrajectoryAngleEnabled, true);
    settings.setSetting(ORNL::MS::PlatformAdhesion::kRaftEnable, true);
    settings.setSetting(ORNL::MS::PlatformAdhesion::kBrimEnable, true);
    settings.setSetting(ORNL::MS::PlatformAdhesion::kSkirtEnable, true);
    settings.setSetting(ORNL::MS::SpiralLift::kPerimeterEnable, true);
    settings.setSetting(ORNL::MS::SpiralLift::kInsetEnable, true);
    settings.setSetting(ORNL::MS::SpiralLift::kSkinEnable, true);
    settings.setSetting(ORNL::MS::SpiralLift::kInfillEnable, true);
    settings.setSetting(ORNL::MS::SpiralLift::kLayerEnable, true);
    settings.setSetting(ORNL::MS::SpiralLift::kDisableFeedrateScaling, true);
    settings.setSetting(ORNL::MS::Purge::kEnablePurgeDwell, true);
}

bool incompatibleSettingsDisabled(const ORNL::SettingsBase& settings) {
    bool passed = true;
    passed &= expect(!settings.setting<bool>(ORNL::PS::SpecialModes::kEnableWidthHeight),
                     "Expected width/height mode to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::PS::Support::kEnable),
                     "Expected support to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::ES::Ramping::kTrajectoryAngleEnabled),
                     "Expected trajectory auto speed ramping to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::PlatformAdhesion::kRaftEnable),
                     "Expected raft to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::PlatformAdhesion::kBrimEnable),
                     "Expected brim to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::PlatformAdhesion::kSkirtEnable),
                     "Expected skirt to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::SpiralLift::kPerimeterEnable),
                     "Expected perimeter spiral lift to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::SpiralLift::kInsetEnable),
                     "Expected inset spiral lift to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::SpiralLift::kSkinEnable),
                     "Expected skin spiral lift to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::SpiralLift::kInfillEnable),
                     "Expected infill spiral lift to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::SpiralLift::kLayerEnable),
                     "Expected end-of-layer spiral lift to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::SpiralLift::kDisableFeedrateScaling),
                     "Expected spiral lift feedrate-scaling override to be disabled for Friction Stir.");
    passed &= expect(!settings.setting<bool>(ORNL::MS::Purge::kEnablePurgeDwell),
                     "Expected purge during dwell to be disabled for Friction Stir.");
    return passed;
}
} // namespace

int main() {
    ORNL::SettingsBase settings;
    enableFrictionStirIncompatibleSettings(settings);
    settings.makeGlobalAdjustments();

    return incompatibleSettingsDisabled(settings) ? EXIT_SUCCESS : EXIT_FAILURE;
}
