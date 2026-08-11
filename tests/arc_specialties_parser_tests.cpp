#include <cstdlib>
#include <exception>
#include <iostream>

#include <QCoreApplication>
#include <QSharedPointer>
#include <QStringList>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/parsers/arc_specialties_parser.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace {
bool expect(bool condition, const char* message) {
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

bool parsesArcLine(const QString& line) {
    QStringList original_lines{line};
    QStringList upper_lines{line.toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        const QList<QList<ORNL::GcodeCommand>> commands = parser.parseLines();
        return commands.size() == 1 && commands.first().size() == 1 &&
               !commands.first().first().getParameters().contains('G');
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool parsedArcKeepsCpForVisualization(const QString& line) {
    QStringList original_lines{line};
    QStringList upper_lines{line.toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        const QList<QList<ORNL::GcodeCommand>> commands = parser.parseLines();
        return commands.size() == 1 && commands.first().size() == 1 &&
               commands.first().first().getOptionalParameters().contains('C') &&
               commands.first().first().getOptionalParameters().value('C') == 0.0;
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool parsedLineKeepsCpForVisualization() {
    QStringList original_lines{
        "G01 X=0.0000 Y=1.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=90.0000 "
        "F600.0000 ;RADIAL"};
    QStringList upper_lines{original_lines.first().toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        const QList<QList<ORNL::GcodeCommand>> commands = parser.parseLines();
        return commands.size() == 1 && commands.first().size() == 1 &&
               commands.first().first().getOptionalParameters().contains('C') &&
               commands.first().first().getOptionalParameters().value('C') == 90.0;
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool writesInlineArcOptionalStop() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::MachineSetup::kSupportG3, true);
    settings->setSetting(ORNL::PRS::MachineSetup::kG2G3CenterPointInterpretation, 1);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG2G3OptionalStop, true);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create();
    segment_settings->setSetting(ORNL::SS::kSpeed, 600.0 * ORNL::mm / ORNL::minute);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const ORNL::Point start(0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(1.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point center(0.5 * ORNL::mm, 0.0 * ORNL::mm);

    const QString first_arc = writer.writeArc(start, end, center, 180.0 * ORNL::degree, false, segment_settings);
    const QString second_arc = writer.writeArc(start, end, center, 180.0 * ORNL::degree, false, segment_settings);

    return first_arc.contains("F600.0000 G81 ;") && second_arc.contains("F600.0000 G81 ;") &&
           !first_arc.contains("G81 ;OPTIONAL STOP ROUTINE") &&
           !second_arc.contains("G81 ;OPTIONAL STOP ROUTINE");
}

bool writesCompactCylindricalPrintComments() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kCylindrical));
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kRadial));
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisA, 0.0 * ORNL::degree);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisC, 0.0 * ORNL::degree);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create();
    segment_settings->setSetting(ORNL::SS::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    segment_settings->setSetting(QStringLiteral("radial_center_x"), 0.0 * ORNL::mm);
    segment_settings->setSetting(QStringLiteral("radial_center_y"), 0.0 * ORNL::mm);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString radial_line =
        writer.writeLine(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm),
                         ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm), segment_settings);

    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kHelical));
    segment_settings->setSetting(ORNL::PS::Slicing::kHelicalPathHandedness,
                                 static_cast<int>(ORNL::HelicalPathHandedness::kRightHanded));
    segment_settings->setSetting(ORNL::PS::Slicing::kHelicalPathStartAngle, 0.0 * ORNL::degree);
    ORNL::ArcSpecialtiesWriter helical_writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString helical_line =
        helical_writer.writeLine(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm),
                                 ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm), segment_settings);

    return radial_line.contains(";RADIAL\n") && helical_line.contains(";HELICAL\n") &&
           !radial_line.contains("AXIS_X") && !radial_line.contains("AXIS_Y") &&
           !helical_line.contains("AXIS_X") && !helical_line.contains("AXIS_Y");
}

QString lineContaining(const QString& block, const QString& marker) {
    for (const QString& line : block.split('\n', Qt::SkipEmptyParts)) {
        if (line.contains(marker)) {
            return line;
        }
    }

    return QString();
}

bool writesFirstTravelWithWorkObjectToolFrame() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Travel::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString travel_block = writer.writeTravel(ORNL::Point(0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                    ORNL::Point(1.0 * ORNL::mm, 2.0 * ORNL::mm, 3.0 * ORNL::mm),
                                                    ORNL::TravelLiftType::kNoLift, settings);

    const QString world_approach_line = lineContaining(travel_block, ";WORLD APPROACH TRAVEL");
    const QString first_travel_line = lineContaining(travel_block, ";TRAVEL");

    return world_approach_line.contains("ZR=-90.0000") && first_travel_line.contains("ZR=-135.0000");
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QString clockwise_arc =
        "G02 X=1.0000 Y=0.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=0.0000 "
        "I=0.5000 J=0.0000 F600.0000 G81 ;PERIMETER";
    const QString counter_clockwise_arc =
        "G03 X=1.0000 Y=0.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=0.0000 "
        "I=0.5000 J=0.0000 F600.0000 G81 ;PERIMETER";

    bool passed = true;
    passed &= expect(parsesArcLine(clockwise_arc), "Arc Specialties G02 did not ignore inline G81.");
    passed &= expect(parsesArcLine(counter_clockwise_arc), "Arc Specialties G03 did not ignore inline G81.");
    passed &= expect(parsedArcKeepsCpForVisualization(clockwise_arc),
                     "Arc Specialties parser did not retain CP for visualization.");
    passed &= expect(parsedLineKeepsCpForVisualization(),
                     "Arc Specialties parser did not retain linear CP for visualization.");
    passed &= expect(writesInlineArcOptionalStop(), "Arc Specialties writer did not emit inline G81 on G02/G03.");
    passed &= expect(writesCompactCylindricalPrintComments(),
                     "Arc Specialties writer did not emit compact cylindrical comments.");
    passed &= expect(writesFirstTravelWithWorkObjectToolFrame(),
                     "Arc Specialties first travel did not use ZR=-135.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
