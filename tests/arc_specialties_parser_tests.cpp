#include <QCoreApplication>
#include <QSharedPointer>
#include <QStringList>
#include <QVector3D>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>

#include "configs/settings_base.h"
#include "gcode/arc_specialties_axis_inference.h"
#include "gcode/gcode_meta.h"
#include "gcode/parsers/arc_specialties_parser.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "step/layer/cylindrical_layer.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace {
bool expect(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

bool parsesArcLine(const QString& line) {
    QStringList original_lines {line};
    QStringList upper_lines {line.toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        const QList<QList<ORNL::GcodeCommand>> commands = parser.parseLines();
        return commands.size() == 1 && commands.first().size() == 1 &&
               !commands.first().first().getParameters().contains('G');
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool parsedArcKeepsCpForVisualization(const QString& line, bool expect_feedrate_parameter = true) {
    QStringList original_lines {line};
    QStringList upper_lines {line.toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        const QList<QList<ORNL::GcodeCommand>> commands = parser.parseLines();
        return commands.size() == 1 && commands.first().size() == 1 &&
               commands.first().first().getOptionalParameters().contains('C') &&
               commands.first().first().getOptionalParameters().value('C') == 0.0 &&
               commands.first().first().getParameters().contains('F') == expect_feedrate_parameter;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool parsedLineKeepsCpForVisualization(const QString& line, bool expect_feedrate_parameter = true) {
    QStringList original_lines {line};
    QStringList upper_lines {line.toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        const QList<QList<ORNL::GcodeCommand>> commands = parser.parseLines();
        return commands.size() == 1 && commands.first().size() == 1 &&
               commands.first().first().getOptionalParameters().contains('C') &&
               commands.first().first().getOptionalParameters().value('C') == 90.0 &&
               commands.first().first().getParameters().contains('F') == expect_feedrate_parameter;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool parsedLineKeepsCpForVisualization() {
    return parsedLineKeepsCpForVisualization(
        "G01 X=0.0000 Y=1.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=90.0000 "
        "F600.0000 ;RADIAL");
}

bool parsedLineWithScheduleSpeedKeepsCpForVisualization() {
    return parsedLineKeepsCpForVisualization(
               "G01 X=0.0000 Y=1.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 "
               "CP=90.0000 FV.S.SPEED ;RADIAL",
               false) &&
           parsedLineKeepsCpForVisualization(
               "G01 X=0.0000 Y=1.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 "
               "CP=90.0000 F=V.S.SPEED ;RADIAL",
               false);
}

bool parsesArcSpecialtiesOptStopModeAssignment() {
    QStringList original_lines {"V.E.OptStopMode = 1"};
    QStringList upper_lines {original_lines.first().toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        parser.parseLines();
        return true;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return false;
    }
}

bool parsesNumberedArcSpecialtiesMotion() {
    return parsedLineKeepsCpForVisualization(
               "N20002 G01 X=0.0000 Y=1.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 "
               "AP=0.0000 CP=90.0000 F600.0000 ;RADIAL") &&
           parsedArcKeepsCpForVisualization(
               "N10005 G02 X=1.0000 Y=0.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 "
               "AP=0.0000 CP=0.0000 I=0.5000 J=0.0000 F600.0000 G81 ;PERIMETER");
}

bool rejectsDuplicateScheduleSpeedFeedrate() {
    QStringList original_lines {
        "G01 X=0.0000 Y=1.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=90.0000 "
        "F600.0000 FV.S.SPEED ;RADIAL"};
    QStringList upper_lines {original_lines.first().toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, original_lines, upper_lines);

    try {
        parser.parseLines();
    } catch (const std::exception&) { return true; }

    return false;
}

QString lineContaining(const QString& block, const QString& marker);

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

    const QString first_arc  = writer.writeArc(start, end, center, 180.0 * ORNL::degree, false, segment_settings);
    const QString second_arc = writer.writeArc(start, end, center, 180.0 * ORNL::degree, false, segment_settings);

    return first_arc.contains("F600.0000 G81 ;") && second_arc.contains("F600.0000 G81 ;") &&
           !first_arc.contains("G81 ;OPTIONAL STOP ROUTINE") && !second_arc.contains("G81 ;OPTIONAL STOP ROUTINE");
}

bool writesConfiguredG80WeldScheduleFile() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG80WeldScheduleFile, QString("D:\\Schedules\\custom_sch.nc"));

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString setup = writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    return setup.contains("#FILE NAME[ G80=\"D:\\Schedules\\custom_sch.nc\" ]\n") &&
           !setup.contains("#FILE NAME[ G80=\"\" ]\n");
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
    const QString radial_line = writer.writeLine(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                 ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm), segment_settings);

    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kHelical));
    segment_settings->setSetting(ORNL::PS::Helical::kHelicalPathHandedness,
                                 static_cast<int>(ORNL::HelicalPathHandedness::kRightHanded));
    segment_settings->setSetting(ORNL::PS::Helical::kHelicalToolStartAngleOffset, 0.0 * ORNL::degree);
    ORNL::ArcSpecialtiesWriter helical_writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString helical_line = helical_writer.writeLine(
        ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm), ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm), segment_settings);

    return radial_line.contains(";RADIAL\n") && helical_line.contains(";HELICAL\n") &&
           !radial_line.contains("AXIS_X") && !radial_line.contains("AXIS_Y") && !helical_line.contains("AXIS_X") &&
           !helical_line.contains("AXIS_Y");
}

QSharedPointer<ORNL::SettingsBase> helicalWriterSettings(bool support_arcs) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kCylindrical));
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kHelical));
    settings->setSetting(ORNL::PRS::MachineSetup::kSupportG3, support_arcs);
    settings->setSetting(ORNL::PRS::MachineSetup::kG2G3CenterPointInterpretation, 1);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisA, 0.0 * ORNL::degree);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisC, 0.0 * ORNL::degree);
    return settings;
}

QSharedPointer<ORNL::SettingsBase> helicalSegmentSettings(std::optional<ORNL::RegionType> region_type) {
    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create();
    segment_settings->setSetting(ORNL::SS::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    segment_settings->setSetting(QStringLiteral("radial_center_x"), 0.0 * ORNL::mm);
    segment_settings->setSetting(QStringLiteral("radial_center_y"), 0.0 * ORNL::mm);
    segment_settings->setSetting(ORNL::PS::Helical::kHelicalPathHandedness,
                                 static_cast<int>(ORNL::HelicalPathHandedness::kRightHanded));
    segment_settings->setSetting(ORNL::PS::Helical::kHelicalToolStartAngleOffset, 0.0 * ORNL::degree);
    if (region_type.has_value()) { segment_settings->setSetting(ORNL::SS::kRegionType, region_type.value()); }
    return segment_settings;
}

bool writesNumericSpeedWhenG80ScheduleFileIsEmpty() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG80WeldScheduleFile, QString());

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString block = writer.writeLine(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                           ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                                           helicalSegmentSettings(ORNL::RegionType::kPerimeter));
    const QString line  = lineContaining(block, ";HELICAL PERIMETER");

    return line.contains(" F600.0000 ;HELICAL PERIMETER") && !line.contains("FV.S.SPEED");
}

bool writesG80ScheduleSpeedVariableForLineAndArc() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(true);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG80WeldScheduleFile, QString("D:\\Schedules\\custom_sch.nc"));
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG2G3OptionalStop, true);

    const ORNL::Point start(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm);
    const ORNL::Point center(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    QSharedPointer<ORNL::SettingsBase> segment_settings = helicalSegmentSettings(ORNL::RegionType::kPerimeter);

    ORNL::ArcSpecialtiesWriter line_writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString line_block = line_writer.writeLine(start, end, segment_settings);
    const QString line       = lineContaining(line_block, ";HELICAL PERIMETER");

    ORNL::ArcSpecialtiesWriter arc_writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString arc_block = arc_writer.writeArc(start, end, center, 90.0 * ORNL::degree, false, segment_settings);
    const QString arc       = lineContaining(arc_block, ";HELICAL PERIMETER");

    const QString expected_arc =
        "G02 X=0.0000 Y=1.0000 Z=1.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=0.0000 "
        "I=-1.0000 J=0.0000 FV.S.SPEED G81 ;HELICAL PERIMETER";

    return line.contains(" FV.S.SPEED ;HELICAL PERIMETER") && !line.contains("F600.0000") && arc == expected_arc;
}

bool writesHelicalCpFromStartOffsetBaseline() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(true);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisA, 90.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 20.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    QSharedPointer<ORNL::SettingsBase> segment_settings = helicalSegmentSettings(ORNL::RegionType::kPerimeter);
    segment_settings->populate(settings);
    segment_settings->setSetting(ORNL::PS::Helical::kHelicalPathHandedness,
                                 static_cast<int>(ORNL::HelicalPathHandedness::kLeftHanded));
    segment_settings->setSetting(ORNL::PS::Helical::kHelicalToolStartAngleOffset, -12.0 * ORNL::degree);

    const ORNL::Distance radius = 100.0 * ORNL::mm;
    auto point_at_angle         = [radius](double angle_degrees, ORNL::Distance z) {
        const double angle_radians = angle_degrees * M_PI / 180.0;
        return ORNL::Point(radius * std::cos(angle_radians), radius * std::sin(angle_radians), z);
    };
    const ORNL::Point start = point_at_angle(90.0, 0.0 * ORNL::mm);
    const ORNL::Point end   = point_at_angle(78.0, 1.0 * ORNL::mm);
    const ORNL::Point center(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    QString block;
    block += writer.writeLayerChange(0);
    block += writer.writeBeforeLayer(0.0f, settings);
    block += writer.writeBeforeRegion(ORNL::RegionType::kPerimeter, 1);
    block += writer.writeTravel(start, start, ORNL::TravelLiftType::kLiftLowerOnly, segment_settings);
    block += writer.writeArc(start, end, center, 12.0 * ORNL::degree, false, segment_settings);

    const QString world_approach_line = lineContaining(block, ";WORLD APPROACH TRAVEL");
    const QString travel_line         = lineContaining(block, ";TRAVEL");
    const QString travel_lower_line   = lineContaining(block, ";TRAVEL LOWER");
    const QString arc_line            = lineContaining(block, ";HELICAL PERIMETER");

    return world_approach_line.contains("CP=-12.0000") && travel_line.contains("X=0.0000 Y=100.0000") &&
           travel_line.contains("CP=-12.0000") && travel_lower_line.contains("X=0.0000 Y=100.0000") &&
           travel_lower_line.contains("CP=-12.0000") && arc_line.contains("X=20.7912 Y=97.8148") &&
           arc_line.contains("CP=0.0000");
}

bool writesLayerScopedBlockNumbersWhenEnabled() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesEmitBlockNumbers, true);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 20.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    QSharedPointer<ORNL::SettingsBase> segment_settings = helicalSegmentSettings(ORNL::RegionType::kInfill);
    segment_settings->populate(settings);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    QString first_layer;
    first_layer += writer.writeLayerChange(0);
    first_layer += writer.writeBeforeLayer(0.0f, settings);
    first_layer += writer.writeBeforeRegion(ORNL::RegionType::kInfill, 1);
    first_layer += writer.writeTravel(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                      ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                      ORNL::TravelLiftType::kLiftLowerOnly, segment_settings);
    first_layer += writer.writeLine(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                    ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm), segment_settings);
    first_layer += writer.writeAfterLayer();

    QString second_layer;
    second_layer += writer.writeLayerChange(1);
    second_layer += writer.writeBeforeLayer(1.0f, settings);
    second_layer += writer.writeBeforeRegion(ORNL::RegionType::kInfill, 1);
    second_layer += writer.writeTravel(ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                                       ORNL::Point(1.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                                       ORNL::TravelLiftType::kLiftLowerOnly, segment_settings);
    second_layer += writer.writeLine(ORNL::Point(1.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                                     ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 2.0 * ORNL::mm), segment_settings);

    const int first_opt_stop    = first_layer.indexOf("V.E.OptStopMode = 1\n");
    const int first_schedule    = first_layer.indexOf("G80 [0] ;Infill Schedule\n");
    const int second_opt_stop   = second_layer.indexOf("V.E.OptStopMode = 1\n");
    const int second_welder_off = second_layer.indexOf(";WIRE ARC WELDER OFF");
    const int second_schedule   = second_layer.indexOf("G80 [0] ;Infill Schedule\n");

    return first_layer.contains(";BEGINNING LAYER: 1\nV.E.OptStopMode = 1\n") &&
           second_layer.contains(";BEGINNING LAYER: 2\nV.E.OptStopMode = 1\n") &&
           lineContaining(first_layer, ";BEGINNING LAYER: 1").startsWith(";") &&
           !lineContaining(first_layer, ";WORLD APPROACH TRAVEL").startsWith("N") &&
           lineContaining(first_layer, ";OPTIONAL STOP ROUTINE").startsWith("N10000 G81") &&
           lineContaining(first_layer, ";TRAVEL LOWER").startsWith("N10001 G01") && first_opt_stop >= 0 &&
           first_schedule > first_opt_stop &&
           lineContaining(first_layer, "G80 [0] ;Infill Schedule").startsWith("N10002 G80") &&
           lineContaining(first_layer, ";WIRE ARC WELDER ON").startsWith("N10003 G82") &&
           lineContaining(first_layer, ";BLENDING ON").startsWith("N10004 G261") &&
           lineContaining(first_layer, ";HELICAL INFILL").startsWith("N10005 G01") &&
           lineContaining(second_layer, ";WIRE ARC WELDER OFF").startsWith("G83 [0]") &&
           second_welder_off > second_opt_stop &&
           lineContaining(second_layer, ";OPTIONAL STOP ROUTINE").startsWith("N20000 G81") &&
           lineContaining(second_layer, ";TRAVEL LOWER").startsWith("N20001 G01") && second_opt_stop >= 0 &&
           second_schedule > second_opt_stop &&
           lineContaining(second_layer, "G80 [0] ;Infill Schedule").startsWith("N20002 G80") &&
           lineContaining(second_layer, ";HELICAL INFILL").startsWith("N20005 G01");
}

bool writesHelicalOptStopModeFromPostOrderingRotationDirection() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    settings->setSetting(ORNL::PS::Optimizations::kCylindricalPathOrder,
                         static_cast<int>(ORNL::PathOrderOptimization::kNextClosest));
    settings->setSetting(ORNL::PS::Travel::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    QSharedPointer<ORNL::ArcSpecialtiesWriter> writer =
        QSharedPointer<ORNL::ArcSpecialtiesWriter>::create(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);

    auto write_ordered_layer = [&settings, &writer](uint layer_number, const ORNL::Point& current_location) {
        const ORNL::Point generated_start(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
        const ORNL::Point generated_end(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm);

        QSharedPointer<ORNL::SettingsBase> segment_settings = helicalSegmentSettings(ORNL::RegionType::kPerimeter);
        segment_settings->populate(settings);

        ORNL::Path path;
        QSharedPointer<ORNL::LineSegment> segment =
            QSharedPointer<ORNL::LineSegment>::create(generated_start, generated_end);
        segment->setSb(segment_settings);
        path.append(segment);

        ORNL::CylindricalLayer layer(layer_number + 1, settings, ORNL::CylindricalPathPattern::kHelical);
        layer.addPath(path);
        ORNL::Point optimized_current_location = current_location;
        layer.calculateModifiers(optimized_current_location);

        QString block;
        block += writer->writeLayerChange(layer_number);
        block += writer->writeBeforeLayer(layer.getMinZ(), layer.getSb());
        block += layer.writeGCode(writer);
        block += writer->writeAfterLayer();
        return block;
    };

    const QString positive_layer = write_ordered_layer(0, ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm));
    const QString negative_layer = write_ordered_layer(1, ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm));

    const int positive_layer_marker = positive_layer.indexOf(";BEGINNING LAYER: 1");
    const int positive_opt_stop     = positive_layer.indexOf("V.E.OptStopMode = 1\n");
    const int positive_schedule     = positive_layer.indexOf("G80 [1] ;Perimeter Schedule\n");
    const int positive_print        = positive_layer.indexOf(";HELICAL PERIMETER\n");
    const int negative_layer_marker = negative_layer.indexOf(";BEGINNING LAYER: 2");
    const int negative_opt_stop     = negative_layer.indexOf("V.E.OptStopMode = 2\n");
    const int negative_schedule     = negative_layer.indexOf("G80 [1] ;Perimeter Schedule\n");
    const int negative_print        = negative_layer.indexOf(";HELICAL PERIMETER\n");

    return positive_layer.contains(";BEGINNING LAYER: 1\nV.E.OptStopMode = 1\n") &&
           negative_layer.contains(";BEGINNING LAYER: 2\nV.E.OptStopMode = 2\n") && positive_layer_marker >= 0 &&
           positive_opt_stop > positive_layer_marker && positive_schedule > positive_opt_stop &&
           positive_print > positive_schedule && negative_layer_marker >= 0 &&
           negative_opt_stop > negative_layer_marker && negative_schedule > negative_opt_stop &&
           negative_print > negative_schedule && !positive_layer.contains("V.E.OptStopMode = 2") &&
           !negative_layer.contains("V.E.OptStopMode = 1");
}

void setHelicalToolFrameSettings(const QSharedPointer<ORNL::SettingsBase>& settings) {
    settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterToolFrameXRotation, 11.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterToolFrameYRotation, 12.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterToolFrameZRotation, 13.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetToolFrameXRotation, 21.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetToolFrameYRotation, 22.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetToolFrameZRotation, 23.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalInfillToolFrameXRotation, 31.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalInfillToolFrameYRotation, 32.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalInfillToolFrameZRotation, 33.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalTravelToolFrameXRotation, 41.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalTravelToolFrameYRotation, 42.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Helical::kHelicalTravelToolFrameZRotation, 43.0 * ORNL::degree);
}

bool writesHelicalRegionLineComments() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);

    const ORNL::Point start(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm);
    const QString block = writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kPerimeter)) %
                          writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kInset)) %
                          writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kInfill));

    return block.contains(";HELICAL PERIMETER\n") && block.contains(";HELICAL INSET\n") &&
           block.contains(";HELICAL INFILL\n");
}

bool writesHelicalRegionArcComments() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(true);
    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);

    const ORNL::Point start(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm);
    const ORNL::Point center(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const QString block = writer.writeArc(start, end, center, 90.0 * ORNL::degree, true,
                                          helicalSegmentSettings(ORNL::RegionType::kPerimeter)) %
                          writer.writeArc(start, end, center, 90.0 * ORNL::degree, true,
                                          helicalSegmentSettings(ORNL::RegionType::kInset)) %
                          writer.writeArc(start, end, center, 90.0 * ORNL::degree, true,
                                          helicalSegmentSettings(ORNL::RegionType::kInfill));

    return block.contains("G03") && block.contains(";HELICAL PERIMETER\n") && block.contains(";HELICAL INSET\n") &&
           block.contains(";HELICAL INFILL\n");
}

bool writesGenericHelicalCommentForMissingOrUnknownRegion() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);

    const ORNL::Point start(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm);
    const QString missing_region = writer.writeLine(start, end, helicalSegmentSettings(std::nullopt));
    const QString unknown_region = writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kUnknown));

    return missing_region.contains(";HELICAL\n") && unknown_region.contains(";HELICAL\n") &&
           !missing_region.contains(";HELICAL PERIMETER") && !unknown_region.contains(";HELICAL INFILL");
}

bool helicalLayerFinalizesOnlyAtPhysicalPathEnd() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    settings->setSetting(ORNL::PS::SpecialModes::kEnableSpiralize, false);
    settings->setSetting(ORNL::PS::GCode::kPerimeterEnd, QStringLiteral("PERIMETER_END_SENTINEL"));
    settings->setSetting(ORNL::PS::GCode::kInsetEnd, QStringLiteral("INSET_END_SENTINEL"));
    settings->setSetting(ORNL::PS::GCode::kInfillEnd, QStringLiteral("INFILL_END_SENTINEL"));
    settings->setSetting(ORNL::PS::Travel::kSpeed, 482.6 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    ORNL::Path path;
    QSharedPointer<ORNL::TravelSegment> travel = QSharedPointer<ORNL::TravelSegment>::create(
        ORNL::Point(1.0 * ORNL::mm, -1.0 * ORNL::mm, 0.0 * ORNL::mm),
        ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm), ORNL::TravelLiftType::kNoLift);
    QSharedPointer<ORNL::SettingsBase> travel_settings = helicalSegmentSettings(ORNL::RegionType::kPerimeter);
    travel_settings->populate(settings);
    travel->setSb(travel_settings);
    path.append(travel);

    const auto append_segment = [&path](const ORNL::Point& start, const ORNL::Point& end,
                                        ORNL::RegionType region_type) {
        QSharedPointer<ORNL::LineSegment> segment = QSharedPointer<ORNL::LineSegment>::create(start, end);
        segment->setSb(helicalSegmentSettings(region_type));
        path.append(segment);
    };

    append_segment(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                   ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm), ORNL::RegionType::kPerimeter);
    append_segment(ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                   ORNL::Point(-1.0 * ORNL::mm, 0.0 * ORNL::mm, 2.0 * ORNL::mm), ORNL::RegionType::kInset);
    append_segment(ORNL::Point(-1.0 * ORNL::mm, 0.0 * ORNL::mm, 2.0 * ORNL::mm),
                   ORNL::Point(0.0 * ORNL::mm, -1.0 * ORNL::mm, 3.0 * ORNL::mm), ORNL::RegionType::kInfill);

    ORNL::CylindricalLayer layer(1, settings, ORNL::CylindricalPathPattern::kHelical);
    layer.addPath(path);

    QSharedPointer<ORNL::ArcSpecialtiesWriter> writer =
        QSharedPointer<ORNL::ArcSpecialtiesWriter>::create(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString block          = layer.writeGCode(writer);
    const int perimeter_schedule = block.indexOf("G80 [1] ;Perimeter Schedule\n");
    const int inset_schedule     = block.indexOf("G80 [2] ;Inset Schedule\n");
    const int infill_schedule    = block.indexOf("G80 [0] ;Infill Schedule\n");
    const int beginning_bead     = block.indexOf(";BEGINNING BEAD:");
    const int first_print        = block.indexOf(";HELICAL PERIMETER\n");

    return block.contains(";HELICAL PERIMETER\n") && block.contains(";HELICAL INSET\n") &&
           block.contains(";HELICAL INFILL\n") && beginning_bead >= 0 && perimeter_schedule > beginning_bead &&
           first_print > perimeter_schedule && inset_schedule > perimeter_schedule &&
           infill_schedule > inset_schedule && block.count("G80 [1] ;Perimeter Schedule\n") == 1 &&
           block.count("G80 [2] ;Inset Schedule\n") == 1 && block.count("G80 [0] ;Infill Schedule\n") == 1 &&
           !block.contains("PERIMETER_END_SENTINEL") && !block.contains("INSET_END_SENTINEL") &&
           block.count("INFILL_END_SENTINEL") == 1;
}

bool writesHelicalRegionToolFrameRotations() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    setHelicalToolFrameSettings(settings);
    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);

    const ORNL::Point start(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Point end(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm);
    const QString block = writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kPerimeter)) %
                          writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kInset)) %
                          writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kInfill)) %
                          writer.writeLine(start, end, helicalSegmentSettings(ORNL::RegionType::kUnknown));

    const QString perimeter_line = lineContaining(block, ";HELICAL PERIMETER");
    const QString inset_line     = lineContaining(block, ";HELICAL INSET");
    const QString infill_line    = lineContaining(block, ";HELICAL INFILL");
    QString fallback_line;
    for (const QString& line : block.split('\n', Qt::SkipEmptyParts)) {
        if (line.contains(";HELICAL") && !line.contains("PERIMETER") && !line.contains("INSET") &&
            !line.contains("INFILL")) {
            fallback_line = line;
            break;
        }
    }

    return perimeter_line.contains("XR=11.0000 YR=12.0000 ZR=13.0000") &&
           inset_line.contains("XR=21.0000 YR=22.0000 ZR=23.0000") &&
           infill_line.contains("XR=31.0000 YR=32.0000 ZR=33.0000") &&
           fallback_line.contains("XR=180.0000 YR=0.0000 ZR=-135.0000");
}

bool writesHelicalTravelToolFrameRotation() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    setHelicalToolFrameSettings(settings);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    QSharedPointer<ORNL::SettingsBase> segment_settings = helicalSegmentSettings(ORNL::RegionType::kPerimeter);
    segment_settings->populate(settings);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString travel_block = writer.writeTravel(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                    ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                                                    ORNL::TravelLiftType::kNoLift, segment_settings);

    const QString world_approach_line = lineContaining(travel_block, ";WORLD APPROACH TRAVEL");
    const QString first_travel_line   = lineContaining(travel_block, ";TRAVEL");

    return world_approach_line.contains("XR=180.0000 YR=0.0000 ZR=-90.0000") &&
           first_travel_line.contains("XR=41.0000 YR=42.0000 ZR=43.0000");
}

bool writesHelicalToolFrameHeader() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    setHelicalToolFrameSettings(settings);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Layer::kBeadWidth, 4.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Slicing::kCylinderInnerRadius, 5.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Slicing::kCylinderHeight, 10.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Slicing::kCylinderAxisSource,
                         static_cast<int>(ORNL::CylinderAxisSource::kPartCentroid));
    settings->setSetting(ORNL::PS::Slicing::kArcsPerRevolution, 8);
    settings->setSetting(ORNL::PS::Optimizations::kCylindricalPathOrder,
                         static_cast<int>(ORNL::PathOrderOptimization::kNextClosest));
    settings->setSetting(ORNL::PS::Helical::kHelicalPathZClipRounding,
                         static_cast<int>(ORNL::HelicalPathZClipRounding::kExactIntersection));
    settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterRevolutions, 1);
    settings->setSetting(ORNL::PS::Helical::kHelicalInsetRevolutions, 1);
    settings->setSetting(ORNL::PS::Helical::kHelicalInfillRevolutionsRounding,
                         static_cast<int>(ORNL::HelicalInfillRevolutionsRounding::kCeil));
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString header = writer.writeSettingsHeader(ORNL::GcodeSyntax::kArcSpecialties);

    return header.contains(";Helical Perimeter Tool Frame Rotation: XR=11.0000deg YR=12.0000deg ZR=13.0000deg") &&
           header.contains(";Helical Inset Tool Frame Rotation: XR=21.0000deg YR=22.0000deg ZR=23.0000deg") &&
           header.contains(";Helical Infill Tool Frame Rotation: XR=31.0000deg YR=32.0000deg ZR=33.0000deg") &&
           header.contains(";Helical Travel Tool Frame Rotation: XR=41.0000deg YR=42.0000deg ZR=43.0000deg");
}

QString lineContaining(const QString& block, const QString& marker) {
    for (const QString& line : block.split('\n', Qt::SkipEmptyParts)) {
        if (line.contains(marker)) { return line; }
    }

    return QString();
}

int occurrenceCount(const QString& block, const QString& marker) {
    return block.count(marker);
}

bool near(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-6;
}

bool infersLeftHandedHelicalAxisFromReversedCpDelta() {
    ORNL::Point center;
    const bool inferred = ORNL::ArcSpecialtiesAxisInference::cylindricalAxisFromCpDelta(
        QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 1.0, 0.0), 0.0, 270.0, true, std::nullopt, center);

    return inferred && near(center.x(), 0.0) && near(center.y(), 0.0);
}

bool rightHandedCpDeltaDoesNotMirrorAxis() {
    ORNL::Point center;
    const bool inferred = ORNL::ArcSpecialtiesAxisInference::cylindricalAxisFromCpDelta(
        QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 1.0, 0.0), 0.0, 270.0, false, std::nullopt, center);

    return inferred && !near(center.x(), 0.0) && !near(center.y(), 0.0);
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
    const QString first_travel_line   = lineContaining(travel_block, ";TRAVEL");

    return world_approach_line.contains("ZR=-90.0000") && first_travel_line.contains("ZR=-135.0000");
}

QString worldApproachLineForSafeZ(ORNL::Distance build_maximum_z, ORNL::Distance cylinder_height) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kCylindrical));
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kRadial));
    settings->setSetting(ORNL::PS::Slicing::kCylinderHeight, cylinder_height);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisA, 0.0 * ORNL::degree);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisC, 0.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create(*settings);
    segment_settings->setSetting(QStringLiteral("radial_center_x"), 0.0 * ORNL::mm);
    segment_settings->setSetting(QStringLiteral("radial_center_y"), 0.0 * ORNL::mm);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    writer.setBuildMaximumZ(build_maximum_z);
    const QString travel_block = writer.writeTravel(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                    ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                    ORNL::TravelLiftType::kNoLift, segment_settings);

    return lineContaining(travel_block, ";WORLD APPROACH TRAVEL");
}

bool writesStartupWorldApproachAbovePartOrCylinderHeight() {
    return worldApproachLineForSafeZ(20.0 * ORNL::mm, 50.0 * ORNL::mm).contains("Z=150.0000") &&
           worldApproachLineForSafeZ(70.0 * ORNL::mm, 50.0 * ORNL::mm).contains("Z=170.0000");
}

bool writesStartupWorldApproachAboveGeneratedHelicalMaxZ() {
    return worldApproachLineForSafeZ(62.0 * ORNL::mm, 50.0 * ORNL::mm).contains("Z=162.0000");
}

bool writesHelicalZClipRoundingHeader() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kCylindrical));
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kHelical));
    settings->setSetting(ORNL::PS::Helical::kHelicalPathZClipRounding,
                         static_cast<int>(ORNL::HelicalPathZClipRounding::kCompleteRevolution));
    settings->setSetting(ORNL::PS::Helical::kHelicalPathHandedness,
                         static_cast<int>(ORNL::HelicalPathHandedness::kRightHanded));
    settings->setSetting(ORNL::PS::Helical::kHelicalToolStartAngleOffset, 0.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, 1.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Layer::kBeadWidth, 4.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString header = writer.writeSettingsHeader(ORNL::GcodeSyntax::kArcSpecialties);
    const QString removed_boundary_policy_header =
        QStringLiteral(";Helical Path") % QStringLiteral(" Boundary Policy:");
    return !header.contains(removed_boundary_policy_header) && !header.contains(";Helical Rise Per Revolution:") &&
           header.contains(";Helical Region Pitch Fallback: 4.0000mm when a region stepover is 0") &&
           header.contains(";Helical Z Clip Rounding: Complete Revolution");
}

bool writesCylindricalTravelWithConfiguredArcDensity() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Slicing::kSlicingMode, static_cast<int>(ORNL::SlicingMode::kCylindrical));
    settings->setSetting(ORNL::PS::Slicing::kCylindricalPathPattern,
                         static_cast<int>(ORNL::CylindricalPathPattern::kRadial));
    settings->setSetting(ORNL::PS::Slicing::kArcsPerRevolution, 8);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisA, 0.0 * ORNL::degree);
    settings->setSetting(ORNL::PRS::MachineSetup::kAxisC, 0.0 * ORNL::degree);
    settings->setSetting(ORNL::PS::Travel::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kMaxXYSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::MachineSpeed::kZSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PS::Travel::kLiftHeight, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelLength, 0.0 * ORNL::mm);
    settings->setSetting(ORNL::PS::Travel::kMinTravelForLift, 0.0 * ORNL::mm);

    QSharedPointer<ORNL::SettingsBase> segment_settings = QSharedPointer<ORNL::SettingsBase>::create(*settings);
    segment_settings->setSetting(ORNL::SS::kWidth, 1.0 * ORNL::mm);
    segment_settings->setSetting(QStringLiteral("radial_center_x"), 0.0 * ORNL::mm);
    segment_settings->setSetting(QStringLiteral("radial_center_y"), 0.0 * ORNL::mm);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    writer.writeTravel(ORNL::Point(100.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                       ORNL::Point(100.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm), ORNL::TravelLiftType::kNoLift,
                       segment_settings);

    const QString travel_block = writer.writeTravel(ORNL::Point(100.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                    ORNL::Point(-100.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                    ORNL::TravelLiftType::kNoLift, segment_settings);

    return occurrenceCount(travel_block, ";TRAVEL ARC") == 4;
}
}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QString clockwise_arc =
        "G02 X=1.0000 Y=0.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=0.0000 "
        "I=0.5000 J=0.0000 F600.0000 G81 ;PERIMETER";
    const QString counter_clockwise_arc =
        "G03 X=1.0000 Y=0.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=0.0000 "
        "I=0.5000 J=0.0000 F600.0000 G81 ;PERIMETER";
    const QString clockwise_schedule_speed_arc =
        "G02 X=1.0000 Y=0.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=0.0000 "
        "I=0.5000 J=0.0000 FV.S.SPEED G81 ;PERIMETER";
    const QString counter_clockwise_schedule_speed_arc =
        "G03 X=1.0000 Y=0.0000 Z=0.0000 XR=180.0000 YR=0.0000 ZR=-135.0000 AP=0.0000 CP=0.0000 "
        "I=0.5000 J=0.0000 FV.S.SPEED G81 ;PERIMETER";

    bool passed = true;
    passed &= expect(parsesArcLine(clockwise_arc), "Arc Specialties G02 did not ignore inline G81.");
    passed &= expect(parsesArcLine(counter_clockwise_arc), "Arc Specialties G03 did not ignore inline G81.");
    passed &= expect(parsesArcLine(clockwise_schedule_speed_arc),
                     "Arc Specialties G02 did not accept the G80 schedule speed variable.");
    passed &= expect(parsesArcLine(counter_clockwise_schedule_speed_arc),
                     "Arc Specialties G03 did not accept the G80 schedule speed variable.");
    passed &= expect(parsedArcKeepsCpForVisualization(clockwise_arc),
                     "Arc Specialties parser did not retain CP for visualization.");
    passed &= expect(parsedArcKeepsCpForVisualization(clockwise_schedule_speed_arc, false),
                     "Arc Specialties parser did not retain CP with the G80 schedule speed variable.");
    passed &= expect(parsedLineKeepsCpForVisualization(),
                     "Arc Specialties parser did not retain linear CP for visualization.");
    passed &= expect(parsedLineWithScheduleSpeedKeepsCpForVisualization(),
                     "Arc Specialties parser did not retain linear CP with the G80 schedule speed variable.");
    passed &= expect(parsesArcSpecialtiesOptStopModeAssignment(),
                     "Arc Specialties parser did not accept the OptStopMode assignment.");
    passed &=
        expect(parsesNumberedArcSpecialtiesMotion(), "Arc Specialties parser did not accept Beckhoff block numbers.");
    passed &= expect(rejectsDuplicateScheduleSpeedFeedrate(),
                     "Arc Specialties parser did not reject duplicate schedule speed feedrates.");
    passed &= expect(infersLeftHandedHelicalAxisFromReversedCpDelta(),
                     "Arc Specialties loader did not reverse left-handed helical CP delta.");
    passed &= expect(rightHandedCpDeltaDoesNotMirrorAxis(),
                     "Arc Specialties loader unexpectedly reversed right-handed CP delta.");
    passed &= expect(writesInlineArcOptionalStop(), "Arc Specialties writer did not emit inline G81 on G02/G03.");
    passed &= expect(writesConfiguredG80WeldScheduleFile(),
                     "Arc Specialties writer did not emit the configured G80 weld schedule file.");
    passed &= expect(writesNumericSpeedWhenG80ScheduleFileIsEmpty(),
                     "Arc Specialties writer did not emit numeric speed without a G80 weld schedule file.");
    passed &= expect(writesG80ScheduleSpeedVariableForLineAndArc(),
                     "Arc Specialties writer did not emit the G80 schedule speed variable for print motion.");
    passed &= expect(writesHelicalCpFromStartOffsetBaseline(),
                     "Arc Specialties writer did not preserve the helical start-offset CP baseline.");
    passed &= expect(writesLayerScopedBlockNumbersWhenEnabled(),
                     "Arc Specialties writer did not emit layer-scoped block numbers.");
    passed &= expect(writesHelicalOptStopModeFromPostOrderingRotationDirection(),
                     "Arc Specialties writer did not emit helical OptStopMode from rotation direction.");
    passed &= expect(writesCompactCylindricalPrintComments(),
                     "Arc Specialties writer did not emit compact cylindrical comments.");
    passed &=
        expect(writesHelicalRegionLineComments(), "Arc Specialties writer did not emit helical line region comments.");
    passed &=
        expect(writesHelicalRegionArcComments(), "Arc Specialties writer did not emit helical arc region comments.");
    passed &= expect(writesGenericHelicalCommentForMissingOrUnknownRegion(),
                     "Arc Specialties writer did not fall back to generic helical comments.");
    passed &= expect(helicalLayerFinalizesOnlyAtPhysicalPathEnd(),
                     "Helical layer emitted region end G-code at internal region transitions.");
    passed &= expect(writesHelicalRegionToolFrameRotations(),
                     "Arc Specialties writer did not emit helical region tool-frame rotations.");
    passed &= expect(writesHelicalTravelToolFrameRotation(),
                     "Arc Specialties writer did not emit the helical travel tool-frame rotation.");
    passed &= expect(writesHelicalToolFrameHeader(),
                     "Arc Specialties writer did not report helical tool-frame rotations in the header.");
    passed &= expect(writesFirstTravelWithWorkObjectToolFrame(), "Arc Specialties first travel did not use ZR=-135.");
    passed &= expect(writesStartupWorldApproachAbovePartOrCylinderHeight(),
                     "Arc Specialties startup world approach did not use part/cylinder safe Z.");
    passed &= expect(writesStartupWorldApproachAboveGeneratedHelicalMaxZ(),
                     "Arc Specialties startup world approach did not use generated helical safe Z.");
    passed &= expect(writesHelicalZClipRoundingHeader(),
                     "Arc Specialties writer did not emit the helical Z clip rounding header.");
    passed &= expect(writesCylindricalTravelWithConfiguredArcDensity(),
                     "Arc Specialties cylindrical travel did not honor Arcs per Revolution.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
