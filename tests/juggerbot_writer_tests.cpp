#include <QCoreApplication>
#include <QSharedPointer>
#include <QString>
#include <cstdlib>
#include <iostream>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/juggerbot_writer.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
constexpr double kPi = 3.14159265358979323846;

bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

QSharedPointer<ORNL::SettingsBase> writerSettings(double perimeter_multiplier) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::Dimensions::kZOffset, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PRS::GCode::kEnableStartupCode, 0);
    settings->setSetting(ORNL::PRS::GCode::kEnableBoundingBox, 0);
    settings->setSetting(ORNL::PRS::GCode::kStartCode, QString());
    settings->setSetting(ORNL::PS::SpecialModes::kEnableWidthHeight, true);
    settings->setSetting(ORNL::PS::Perimeter::kExtrusionMultiplier, perimeter_multiplier);
    settings->setSetting(ORNL::MS::MultiMaterial::kEnable, 0);
    settings->setSetting(ORNL::MS::Extruder::kInitialSpeed, 0);
    settings->setSetting(ORNL::MS::Extruder::kOffDelay, 0.0 * ORNL::s);

    return settings;
}

QSharedPointer<ORNL::SettingsBase> segmentSettings(int extruder_speed,
                                                   ORNL::PathModifiers modifier = ORNL::PathModifiers::kNone) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::SS::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::SS::kExtruderSpeed, extruder_speed);
    settings->setSetting(ORNL::SS::kMaterialNumber, 0);
    settings->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);
    settings->setSetting(ORNL::SS::kPathModifiers, modifier);
    settings->setSetting(ORNL::SS::kWidth, 3.0 * ORNL::mm);
    settings->setSetting(ORNL::SS::kHeight, 1.0 * ORNL::mm);

    return settings;
}

bool appliesExtrusionMultiplierToWidthHeightArcBeadArea() {
    constexpr double multiplier                 = 1.5;
    QSharedPointer<ORNL::SettingsBase> settings = writerSettings(multiplier);
    ORNL::JuggerBotWriter writer(ORNL::GcodeMetaList::MarlinMeta, settings);
    writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    const ORNL::Point start(0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(1.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point center(0.5 * ORNL::mm, 0.0 * ORNL::mm);
    const QString arc = writer.writeArc(start, end, center, 180.0 * ORNL::degree, false, segmentSettings(120));

    constexpr double base_area = (3.0 - 1.0) * 1.0 + (kPi * 0.5 * 0.5);
    const QString expected     = "M3 S" + QString::number(base_area * multiplier) + " ;SET BEAD AREA";

    return arc.contains(expected);
}

bool turnsExtruderOffBeforeZeroRpmWidthHeightArc() {
    QSharedPointer<ORNL::SettingsBase> settings = writerSettings(1.0);
    ORNL::JuggerBotWriter writer(ORNL::GcodeMetaList::MarlinMeta, settings);
    writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    const ORNL::Point start(0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(1.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point center(0.5 * ORNL::mm, 0.0 * ORNL::mm);
    writer.writeArc(start, end, center, 180.0 * ORNL::degree, false, segmentSettings(120));

    const QString zero_rpm_arc = writer.writeArc(start, end, center, 180.0 * ORNL::degree, false,
                                                 segmentSettings(0, ORNL::PathModifiers::kSpiralLift));

    return zero_rpm_arc.startsWith("M5 ;TURN EXTRUDER OFF\nG2") && !zero_rpm_arc.contains("M3");
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    bool passed = true;
    passed &= expect(appliesExtrusionMultiplierToWidthHeightArcBeadArea(),
                     "JuggerBot writer did not apply extrusion multiplier to width/height arc bead area.");
    passed &= expect(turnsExtruderOffBeforeZeroRpmWidthHeightArc(),
                     "JuggerBot writer did not turn the extruder off before a zero-rpm width/height arc.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
