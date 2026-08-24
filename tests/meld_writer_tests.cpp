#include <QSharedPointer>
#include <QString>
#include <cstdlib>
#include <iostream>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/meld_writer.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
const QString kMeldScalingRadius = "meld_scaling_radius";

ORNL::Velocity depositionRate(double inches_per_minute) {
    return ORNL::Velocity(inches_per_minute * ORNL::in / ORNL::minute);
}

bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

QSharedPointer<ORNL::SettingsBase> globalSettings(bool scaling_enabled, bool force_feedrate_scaling = false) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::MachineSetup::kSyntax, static_cast<int>(ORNL::GcodeSyntax::kMeld));
    settings->setSetting(ORNL::PRS::MachineSetup::kMachineType, static_cast<int>(ORNL::MachineType::kFrictionStir));
    settings->setSetting(ORNL::PRS::MachineSpeed::kMeldDepositionRateScaling, scaling_enabled);
    settings->setSetting(ORNL::PRS::Dimensions::kZOffset, ORNL::Distance(0.0));
    settings->setSetting(ORNL::PRS::Dimensions::kWMax, ORNL::Distance(0.0));
    settings->setSetting(ORNL::PRS::GCode::kEnableStartupCode, false);
    settings->setSetting(ORNL::PRS::GCode::kEnableBoundingBox, false);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, ORNL::Velocity(1.0));
    settings->setSetting(ORNL::PRS::MachineSpeed::kMeldDiscrete, true);
    settings->setSetting(ORNL::PS::Travel::kSpeed, ORNL::Velocity(1.0));
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, ORNL::Distance(0.0));
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, ORNL::Distance(0.0));
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalX, 0.0f);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalY, 0.0f);
    settings->setSetting(ORNL::PS::Slicing::kSlicePlaneNormalZ, 1.0f);
    settings->setSetting(ORNL::MS::Cooling::kForceMinLayerTime, force_feedrate_scaling);
    settings->setSetting(ORNL::MS::Cooling::kForceMinLayerTimeMethod,
                         static_cast<int>(ORNL::ForceMinimumLayerTime::kSlow_Feedrate));
    settings->setSetting(ORNL::MS::Extruder::kInitialSpeed, 0);
    return settings;
}

QSharedPointer<ORNL::SettingsBase> segmentSettings(ORNL::Distance radius) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::SS::kSpeed, ORNL::Velocity(1.0));
    settings->setSetting(ORNL::SS::kExtruderSpeed, depositionRate(10.0));
    settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);
    settings->setSetting(ORNL::SS::kPathModifiers, ORNL::PathModifiers::kNone);
    settings->setSetting(ORNL::SS::kWidth, ORNL::Distance(10.0));
    settings->setSetting(kMeldScalingRadius, radius);
    return settings;
}

QString firstLineForRadius(bool scaling_enabled, ORNL::Distance radius, bool force_feedrate_scaling = false) {
    QSharedPointer<ORNL::SettingsBase> settings = globalSettings(scaling_enabled, force_feedrate_scaling);
    ORNL::MeldWriter writer(ORNL::GcodeMetaList::MeldMeta, settings);
    writer.writeInitialSetup(ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), 1);
    return writer.writeLine(ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(1.0f, 0.0f, 0.0f), segmentSettings(radius));
}
}  // namespace

int main() {
    bool passed = true;

    const QString scaled_line = firstLineForRadius(true, ORNL::Distance(2.0));
    passed &=
        expect(scaled_line.contains("M24 S4000"), "Expected 10 in/min * 1000 * min(1, 2*2/10) to emit M24 S4000.");

    const QString capped_line = firstLineForRadius(true, ORNL::Distance(8.0));
    passed &= expect(capped_line.contains("M24 S10000"), "Expected scaling to cap at the commanded 10 in/min value.");

    const QString disabled_line = firstLineForRadius(false, ORNL::Distance(2.0));
    passed &=
        expect(disabled_line.contains("M24 S10000"), "Expected disabled scaling to emit M24 S10000 for 10 in/min.");

    const QString forced_feedrate_line = firstLineForRadius(false, ORNL::Distance(2.0), true);
    passed &= expect(forced_feedrate_line.count("M24 S10000") == 1,
                     "Expected force-feedrate output to avoid a duplicate M24 S10000 update.");
    passed &= expect(!forced_feedrate_line.contains("UPDATE ACTUATOR"),
                     "Expected no redundant actuator update immediately after turning the actuator on.");

    QSharedPointer<ORNL::SettingsBase> travel_settings = globalSettings(false);
    travel_settings->setSetting(ORNL::PS::Travel::kLiftHeight, ORNL::Distance(1.0));
    ORNL::MeldWriter travel_writer(ORNL::GcodeMetaList::MeldMeta, travel_settings);
    travel_writer.writeInitialSetup(ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), 1);
    const QString first_travel =
        travel_writer.writeTravel(ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(2.0f, 3.0f, 0.0f),
                                  ORNL::TravelLiftType::kBoth, segmentSettings(ORNL::Distance(2.0)));
    travel_settings->setSetting(ORNL::PS::Travel::kLiftHeight, ORNL::Distance(0.0));
    const QString second_travel =
        travel_writer.writeTravel(ORNL::Point(2.0f, 3.0f, 0.0f), ORNL::Point(4.0f, 5.0f, 0.0f),
                                  ORNL::TravelLiftType::kNoLift, segmentSettings(ORNL::Distance(2.0)));
    const QString first_line_after_travel = travel_writer.writeLine(
        ORNL::Point(4.0f, 5.0f, 0.0f), ORNL::Point(5.0f, 5.0f, 0.0f), segmentSettings(ORNL::Distance(8.0)));
    const QString first_travel_line = first_travel.section('\n', 0, 0);
    const QString first_lower_line  = first_travel.section('\n', 1, 1);
    const QString travel_sequence   = first_travel + second_travel + first_line_after_travel;

    passed &= expect(first_travel_line.startsWith("G0 X"), "Expected first Meld travel to use G0.");
    passed &= expect(!first_travel_line.contains(" F"), "Expected first Meld travel to omit feedrate.");
    passed &= expect(!first_travel_line.contains(" Z") && !first_travel_line.contains(" W"),
                     "Expected first Meld travel to be XY-only.");
    passed &= expect(first_travel.indexOf("L003") > first_travel.indexOf("TRAVEL"),
                     "Expected L003 after the first Meld travel.");
    passed &= expect(first_lower_line.startsWith("G1 F") && first_lower_line.contains(" Z0.0000"),
                     "Expected first Meld travel lower to force output of unchanged Z.");
    passed &= expect(first_travel.indexOf("L003") > first_travel.indexOf("TRAVEL LOWER Z"),
                     "Expected L003 after the first Meld travel lower.");
    passed &= expect(second_travel.startsWith("G1 F"), "Expected subsequent Meld travel to keep G1 feed moves.");
    passed &= expect(!second_travel.contains("L003"), "Expected L003 only after the first Meld travel.");
    passed &= expect(travel_sequence.indexOf("L003") < travel_sequence.indexOf("M24 S10000"),
                     "Expected L003 before turning the actuator on.");

    QSharedPointer<ORNL::SettingsBase> settings = globalSettings(true);
    ORNL::MeldWriter spiralize_writer(ORNL::GcodeMetaList::MeldMeta, settings);
    spiralize_writer.writeInitialSetup(ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), ORNL::Distance(), 1);
    const QString first_spiralized_line = spiralize_writer.writeLine(
        ORNL::Point(0.0f, 0.0f, 0.0f), ORNL::Point(1.0f, 0.0f, 0.0f), segmentSettings(ORNL::Distance(2.0)));
    const QString next_spiralized_line = spiralize_writer.writeLine(
        ORNL::Point(1.0f, 0.0f, 0.0f), ORNL::Point(2.0f, 0.0f, 0.0f), segmentSettings(ORNL::Distance(1.0)));

    passed &=
        expect(first_spiralized_line.contains("M24 S4000"), "Expected first active line to emit scaled M24 S4000.");
    passed &= expect(next_spiralized_line.contains("M24 S2000"),
                     "Expected active Meld actuator output to refresh with M24 when scaled deposition changes.");
    passed &= expect(!next_spiralized_line.contains("M25"),
                     "Expected active Meld actuator refresh to avoid cycling the actuator off.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
