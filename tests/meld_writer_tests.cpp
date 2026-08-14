#include <cstdlib>
#include <iostream>

#include <QSharedPointer>
#include <QString>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/meld_writer.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
const QString kMeldScalingRadius = "meld_scaling_radius";

bool expect(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

QSharedPointer<ORNL::SettingsBase> globalSettings(bool scaling_enabled) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::MachineSetup::kSyntax, static_cast<int>(ORNL::GcodeSyntax::kMeld));
    settings->setSetting(ORNL::PRS::MachineSetup::kMachineType, static_cast<int>(ORNL::MachineType::kFrictionStir));
    settings->setSetting(ORNL::PRS::MachineSpeed::kMeldDepositionRateScaling, scaling_enabled);
    settings->setSetting(ORNL::PRS::Dimensions::kZOffset, ORNL::Distance(0.0));
    settings->setSetting(ORNL::PRS::Dimensions::kWMax, ORNL::Distance(0.0));
    settings->setSetting(ORNL::PRS::GCode::kEnableStartupCode, false);
    settings->setSetting(ORNL::PRS::GCode::kEnableBoundingBox, false);
    settings->setSetting(ORNL::ES::FileOutput::kMeldDiscrete, true);
    settings->setSetting(ORNL::MS::Cooling::kForceMinLayerTime, false);
    settings->setSetting(ORNL::MS::Extruder::kInitialSpeed, 0);
    return settings;
}

QSharedPointer<ORNL::SettingsBase> segmentSettings(ORNL::Distance radius) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::SS::kSpeed, ORNL::Velocity(1.0));
    settings->setSetting(ORNL::SS::kExtruderSpeed, 100);
    settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);
    settings->setSetting(ORNL::SS::kPathModifiers, ORNL::PathModifiers::kNone);
    settings->setSetting(ORNL::SS::kWidth, ORNL::Distance(10.0));
    settings->setSetting(kMeldScalingRadius, radius);
    return settings;
}

QString firstLineForRadius(bool scaling_enabled, ORNL::Distance radius) {
    QSharedPointer<ORNL::SettingsBase> settings = globalSettings(scaling_enabled);
    ORNL::MeldWriter writer(ORNL::GcodeMetaList::MeldMeta, settings);
    writer.writeInitialSetup(ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), 1);
    return writer.writeLine(ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(1.0f, 0.0f, 0.0f), segmentSettings(radius));
}
} // namespace

int main() {
    bool passed = true;

    const QString scaled_line = firstLineForRadius(true, ORNL::Distance(2.0));
    passed &= expect(scaled_line.contains("M24 S40"), "Expected 100 * min(1, 2*2/10) to emit M24 S40.");

    const QString capped_line = firstLineForRadius(true, ORNL::Distance(8.0));
    passed &= expect(capped_line.contains("M24 S100"), "Expected scaling to cap at the commanded value.");

    const QString disabled_line = firstLineForRadius(false, ORNL::Distance(2.0));
    passed &= expect(disabled_line.contains("M24 S100"), "Expected disabled scaling to emit the commanded value.");

    QSharedPointer<ORNL::SettingsBase> settings = globalSettings(true);
    ORNL::MeldWriter spiralize_writer(ORNL::GcodeMetaList::MeldMeta, settings);
    spiralize_writer.writeInitialSetup(ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), 1);
    const QString first_spiralized_line = spiralize_writer.writeLine(
        ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(1.0f, 0.0f, 0.0f), segmentSettings(ORNL::Distance(2.0)));
    const QString next_spiralized_line = spiralize_writer.writeLine(
        ORNL::Point(1.0f, 0.0f, 0.0f), ORNL::Point(2.0f, 0.0f, 0.0f), segmentSettings(ORNL::Distance(1.0)));

    passed &= expect(first_spiralized_line.contains("M24 S40"), "Expected first active line to emit scaled M24 S40.");
    passed &= expect(next_spiralized_line.contains("M24 S20"),
                     "Expected active Meld actuator output to refresh with M24 when scaled deposition changes.");
    passed &= expect(!next_spiralized_line.contains("M25"),
                     "Expected active Meld actuator refresh to avoid cycling the actuator off.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
