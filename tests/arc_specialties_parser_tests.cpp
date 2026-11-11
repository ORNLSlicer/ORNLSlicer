#include <QCoreApplication>
#include <QFile>
#include <QMatrix4x4>
#include <QRegularExpression>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>
#include <QVector3D>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>

#include "configs/settings_base.h"
#include "gcode/arc_specialties_axis_inference.h"
#include "gcode/gcode_meta.h"
#include "gcode/parsers/arc_specialties_parser.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "slicing/helical_tool_start_angle.h"
#include "step/layer/cylindrical_layer.h"
#include "test_utils.h"
#include "threading/gcode_loader.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace {
constexpr float kTolerance = 1.0e-6f;

QList<QList<ORNL::GcodeCommand>> orientationCommands(QStringList lines) {
    QStringList upper_lines;
    for (const QString& line : lines) { upper_lines.append(line.toUpper()); }
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, lines, upper_lines);
    return parser.parseLines();
}

bool toolFrameMatches(const std::optional<QQuaternion>& frame, double xr, double yr, double zr) {
    if (!frame) { return false; }
    const double sx = std::sin(xr * M_PI / 180.0), cx = std::cos(xr * M_PI / 180.0);
    const double sy = std::sin(yr * M_PI / 180.0), cy = std::cos(yr * M_PI / 180.0);
    const double sz = std::sin(zr * M_PI / 180.0), cz = std::cos(zr * M_PI / 180.0);
    // Independent ZYX rotation-matrix columns check tilt and roll, not just one torch vector.
    const QVector3D expected[] = {{float(cz * cy), float(sz * cy), float(-sy)},
                                  {float(cz * sy * sx - sz * cx), float(sz * sy * sx + cz * cx), float(cy * sx)},
                                  {float(cz * sy * cx + sz * sx), float(sz * sy * cx - cz * sx), float(cy * cx)}};
    const QVector3D basis[]    = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int axis = 0; axis < 3; ++axis) {
        if ((frame->rotatedVector(basis[axis]) - expected[axis]).length() > 1.0e-5f) { return false; }
    }
    return true;
}

bool positionerFrameMatches(const std::optional<QQuaternion>& frame, double ap, double cp) {
    if (!frame) { return false; }
    const double tilt = std::remainder(ap, 360.0) * M_PI / 180.0;
    const double spin = std::remainder(cp, 360.0) * M_PI / 180.0;
    const double sa = std::sin(tilt), ca = std::cos(tilt), sc = std::sin(spin), cc = std::cos(spin);
    // Forward Rx(AP)*Rz(CP) columns, plus the transposed matrix for the inverse work-object transform.
    const QVector3D forward[] = {{float(cc), float(ca * sc), float(sa * sc)},
                                 {float(-sc), float(ca * cc), float(sa * cc)},
                                 {0, float(-sa), float(ca)}};
    const QVector3D inverse[] = {{float(cc), float(-sc), 0},
                                 {float(ca * sc), float(ca * cc), float(-sa)},
                                 {float(sa * sc), float(sa * cc), float(ca)}};
    const QVector3D basis[]   = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int axis = 0; axis < 3; ++axis) {
        if ((frame->rotatedVector(basis[axis]) - forward[axis]).length() > 1.0e-5f ||
            (frame->inverted().rotatedVector(basis[axis]) - inverse[axis]).length() > 1.0e-5f) {
            return false;
        }
    }
    return true;
}

bool retainsModalPositionerFrames() {
    const auto commands = orientationCommands({"G0 X1 XR180 YR0 ZR-135 AP90", "G0 CP45", "G1 X2 F600", "G1 CP-405",
                                               "G1 X3 AP-30", "G1 X4 CP1125", "G1 AP60", "G1 X5", "G1 X6 AP0 CP0"});
    if (commands.size() != 1 || commands.first().size() != 6) { return false; }
    const auto& layer = commands.first();
    for (const auto& command : layer) {
        if (!toolFrameMatches(command.toolOrientation(), 180, 0, -135)) { return false; }
    }
    return !layer[0].positionerOrientation() && positionerFrameMatches(layer[1].positionerOrientation(), 90, 45) &&
           positionerFrameMatches(layer[2].positionerOrientation(), -30, -405) &&
           positionerFrameMatches(layer[3].positionerOrientation(), -30, 1125) &&
           positionerFrameMatches(layer[4].positionerOrientation(), 60, 1125) &&
           positionerFrameMatches(layer[5].positionerOrientation(), 0, 0) &&
           !layer[1].getOptionalParameters().contains('C') && !layer[2].getOptionalParameters().contains('C') &&
           layer[3].getOptionalParameters().value('C') == 1125 && !layer[4].getOptionalParameters().contains('C');
}

bool positionerRequiresBothKnownAxes() {
    const auto commands = orientationCommands({"G0 X1 CP17", "G1 X2 F600", "G0 AP12", "G1 X3"});
    if (commands.size() != 1 || commands.first().size() != 3) { return false; }
    const auto& layer = commands.first();
    return !layer[0].positionerOrientation() && !layer[1].positionerOrientation() &&
           positionerFrameMatches(layer[2].positionerOrientation(), 12, 17) && !layer[2].toolOrientation();
}

bool positionerHandlesSignedAndUnwrappedAngles() {
    const double poses[][2] = {{0, 0}, {90, -360}, {37, -450}, {450, 1080}, {-45, 7200000180.0}, {1.0e20, -1.0e20}};
    for (const auto& pose : poses) {
        const auto commands =
            orientationCommands({QString("G0 X1 AP=%1 CP=%2").arg(pose[0], 0, 'g', 17).arg(pose[1], 0, 'g', 17)});
        if (commands.size() != 1 || commands.first().size() != 1 ||
            !positionerFrameMatches(commands.first().first().positionerOrientation(), pose[0], pose[1])) {
            return false;
        }
    }
    return true;
}

bool positionerHonorsTrafoTransitions() {
    QStringList lines {"N10 #TrAfO oFf ; #TRAFO ON is a comment",
                       "G0 X1 XR180 YR0 ZR-135 AP90 CP20",
                       "; #TRAFO ON",
                       "G0 X2",
                       "N12 #TRAFO ON ; #TRAFO OFF is a comment",
                       "G1 X3 F600",
                       "G0 X4 ;WORLD APPROACH TRAVEL",
                       "n15 #trafo off",
                       "G0 CP50",
                       "G1 X5",
                       "#TRAFO ONWARD",
                       "G0 X6",
                       "#TRAFO ON",
                       "G1 X7"};
    QStringList upper_lines;
    for (const auto& line : lines) { upper_lines.append(line.toUpper()); }
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, lines, upper_lines);
    const auto commands = parser.parseLines();
    if (commands.size() != 1 || commands.first().size() != 7) { return false; }
    const auto& layer = commands.first();
    if (layer[0].positionerOrientation() || layer[1].positionerOrientation() || layer[3].positionerOrientation() ||
        layer[4].positionerOrientation() || layer[5].positionerOrientation() ||
        !positionerFrameMatches(layer[2].positionerOrientation(), 90, 20) ||
        !positionerFrameMatches(layer[6].positionerOrientation(), 90, 50)) {
        return false;
    }
    for (const auto& command : layer) {
        if (!toolFrameMatches(command.toolOrientation(), 180, 0, -135)) { return false; }
    }
    // Exercise raw-case directives too: parseLines normally supplies uppercase text to parseCommand.
    parser.parseCommand("n100 #trafo off ; #TRAFO ON", 100);
    if (parser.parseCommand("G0 X8", 101).positionerOrientation()) { return false; }
    parser.parseCommand("n[101] #TrAfO On ; #TRAFO OFF", 102);
    parser.parseCommand("; #TRAFO OFF", 103);
    return positionerFrameMatches(parser.parseCommand("G0 X9", 104).positionerOrientation(), 90, 50) &&
           !parser.parseCommand("#TRAFO OFF", 105).positionerOrientation();
}

bool retainsToolFramesForAllMotionForms() {
    const auto commands = orientationCommands({"N1 G00 X=1 Y=0 Z=0 XR=0 YR=0 ZR=0 AP=90 CP=10 ;TRAVEL",
                                               "N2 G01 X2 Y0 Z0 XR30 YR-25 ZR70 AP90 CP20 F600 ;PERIMETER",
                                               "N3 G02 X=1 Y=1 I=-1 J=0 XR=180 YR=0 ZR=-90 AP=90 CP=30 F600 ;PERIMETER",
                                               "N4 G03 X0 Y0 I-1 J0 XR180 YR0 ZR-135 AP90 CP40 F600 ;PERIMETER"});
    if (commands.size() != 1 || commands.first().size() != 4) { return false; }
    const auto& layer = commands.first();
    return toolFrameMatches(layer[0].toolOrientation(), 0, 0, 0) &&
           toolFrameMatches(layer[1].toolOrientation(), 30, -25, 70) &&
           toolFrameMatches(layer[2].toolOrientation(), 180, 0, -90) &&
           toolFrameMatches(layer[3].toolOrientation(), 180, 0, -135) &&
           layer[3].getOptionalParameters().value('C') == 40;
}

bool retainsPartialAndOrientationOnlyUpdates() {
    const auto commands = orientationCommands({"G0 X1 XR=0", "G1 X2 YR=0 F600", "G0 ZR=0", "G1 X3", "G1 XR=90", "G1 X4",
                                               "G1 X5 YR=30", "G1 X6 AP=80 CP=130"});
    if (commands.size() != 1 || commands.first().size() != 6) { return false; }
    const auto& layer = commands.first();
    return !layer[0].toolOrientation() && !layer[1].toolOrientation() &&
           toolFrameMatches(layer[2].toolOrientation(), 0, 0, 0) &&
           toolFrameMatches(layer[3].toolOrientation(), 90, 0, 0) &&
           toolFrameMatches(layer[4].toolOrientation(), 90, 30, 0) &&
           toolFrameMatches(layer[5].toolOrientation(), 90, 30, 0);
}

bool copiesAndClearsToolFrames() {
    const auto commands = orientationCommands({"G0 X1 XR=30 YR=-25 ZR=70 AP=37 CP=-450"});
    if (commands.size() != 1 || commands.first().size() != 1) { return false; }
    const ORNL::GcodeCommand source = commands.first().first();
    if (!source.toolOrientation() || !source.positionerOrientation()) { return false; }
    ORNL::GcodeCommand copied(source), assigned;
    assigned = source;
    ORNL::LineSegment segment(ORNL::Point(), ORNL::Point(1, 0, 0));
    segment.setToolOrientation(*source.toolOrientation());
    const auto cloned           = segment.clone();
    const bool copied_correctly = toolFrameMatches(copied.toolOrientation(), 30, -25, 70) &&
                                  toolFrameMatches(assigned.toolOrientation(), 30, -25, 70) &&
                                  positionerFrameMatches(copied.positionerOrientation(), 37, -450) &&
                                  positionerFrameMatches(assigned.positionerOrientation(), 37, -450) &&
                                  toolFrameMatches(cloned->toolOrientation(), 30, -25, 70);
    copied.clearParameters();
    assigned.clearToolOrientation();
    const bool clears_independently = positionerFrameMatches(assigned.positionerOrientation(), 37, -450);
    assigned                        = source;
    assigned.clearPositionerOrientation();
    segment.clearToolOrientation();
    return copied_correctly && clears_independently && !copied.toolOrientation() && !copied.positionerOrientation() &&
           !assigned.positionerOrientation() && toolFrameMatches(assigned.toolOrientation(), 30, -25, 70) &&
           !segment.toolOrientation() && source.toolOrientation() && source.positionerOrientation() &&
           cloned->toolOrientation();
}

bool clearsToolFramesWhenReconfigured() {
    QStringList lines {"G0 X1 XR=30 YR=-25 ZR=70 AP=90 CP=37"};
    QStringList upper_lines {lines.first().toUpper()};
    ORNL::ArcSpecialtiesParser parser(ORNL::GcodeMetaList::ArcSpecialtiesMeta, false, lines, upper_lines);
    const auto commands = parser.parseLines();
    if (commands.size() != 1 || commands.first().size() != 1 || !commands.first().first().toolOrientation()) {
        return false;
    }
    ORNL::CommonParser& base = parser;
    parser.parseCommand("#TRAFO OFF", 1);
    base.reset();
    parser.config();
    const auto unknown_frame = parser.parseCommand("G0 X2 XR=0 CP=23", 1);
    const auto completed     = parser.parseCommand("G0 X3 YR=0 ZR=0 AP=0", 2);
    const auto non_motion    = parser.parseCommand("G83", 3);
    return !unknown_frame.toolOrientation() && !unknown_frame.positionerOrientation() &&
           toolFrameMatches(completed.toolOrientation(), 0, 0, 0) &&
           positionerFrameMatches(completed.positionerOrientation(), 0, 23) && !non_motion.toolOrientation() &&
           !non_motion.positionerOrientation();
}

bool ordinarySyntaxHasNoToolFrame() {
    QStringList lines {"G1 X1 A2 F600", "G1 X2 B3 F600"};
    QStringList upper_lines = lines;
    ORNL::CommonParser parser(ORNL::GcodeMetaList::MarlinMeta, false, lines, upper_lines);
    const auto commands = parser.parseLines();
    return commands.size() == 1 && commands.first().size() == 2 && !commands.first().first().toolOrientation() &&
           !commands.first().last().toolOrientation();
}

bool loaderPreservesToolFrames(const QString& syntax, const QString& moves, bool expect_orientation) {
    QTemporaryFile file;
    if (!file.open()) { return false; }
    file.write((";G-Code Syntax: " + syntax + "\n" + moves).toUtf8());
    file.flush();
    file.close();
    ORNL::GCodeLoader loader(file.fileName(), false);
    QVector<QSharedPointer<ORNL::SegmentBase>> segments;
    bool failed = false;
    QObject::connect(&loader, &ORNL::GCodeLoader::gcodeLoadedVisualization, [&segments](const auto& layers) {
        for (const auto& layer : layers) { segments.append(layer); }
    });
    QObject::connect(&loader, &ORNL::GCodeLoader::error, [&failed](const QString& error) {
        std::cerr << error.toStdString() << '\n';
        failed = true;
    });
    loader.run();
    if (failed || segments.size() != 3) { return false; }
    for (const auto& segment : segments) {
        if (expect_orientation) {
            if (!toolFrameMatches(segment->toolOrientation(), 171.4524, -8.4540, -134.3671)) { return false; }
        }
        else if (segment->toolOrientation()) { return false; }
    }
    return dynamic_cast<ORNL::LineSegment*>(segments[0].data()) &&
           dynamic_cast<ORNL::LineSegment*>(segments[1].data()) && dynamic_cast<ORNL::ArcSegment*>(segments[2].data());
}

bool donutReferenceUsesWorkObjectToolFrames(bool perturb_tilt) {
    const QString fixture = QString::fromUtf8(ORNLSLICER_TEST_SOURCE_DIR) + "/fixtures/arc_specialties_donut1827.nc";
    const QString manual_reference =
        perturb_tilt ? QString() : qEnvironmentVariable("ORNLSLICER_ORIENTATION_REFERENCE_GCODE");
    const QString source_path = manual_reference.isEmpty() ? fixture : manual_reference;
    QFile source(source_path);
    if (!source.open(QIODevice::ReadOnly)) {
        std::cerr << "Could not read orientation reference " << source_path.toStdString() << '\n';
        return false;
    }
    QString text = QString::fromUtf8(source.readAll());
    if (perturb_tilt) {
        if (!text.contains("XR=171.5295")) { return false; }
        text.replace("XR=171.5295", "XR=161.5295");
    }
    // The loader can rewrite its input with statistics; always preserve the source NC program.
    QTemporaryFile copy;
    if (!copy.open()) { return false; }
    copy.write(text.toUtf8());
    copy.flush();
    copy.close();
    ORNL::GCodeLoader loader(copy.fileName(), false);
    bool passed             = true;
    int print_count         = 0;
    bool saw_world_approach = false;
    QObject::connect(&loader, &ORNL::GCodeLoader::error, [&passed](const QString& error) {
        std::cerr << error.toStdString() << '\n';
        passed = false;
    });
    QObject::connect(&loader, &ORNL::GCodeLoader::gcodeLoadedVisualization, [&](const auto& layers) {
        for (const auto& layer : layers) {
            for (const auto& segment : layer) {
                const auto frame    = segment->toolOrientation();
                const QString& type = segment->m_segment_info_meta.type;
                if (type.contains("WORLD APPROACH TRAVEL", Qt::CaseInsensitive)) {
                    saw_world_approach = true;
                    passed &=
                        frame && (frame->rotatedVector(QVector3D(0, 0, 1)) - QVector3D(0, 0, -1)).length() < 1.0e-5f;
                }
                const auto* arc = dynamic_cast<ORNL::ArcSegment*>(segment.data());
                if (!arc || !type.contains("HELICAL", Qt::CaseInsensitive)) { continue; }
                ++print_count;
                if (!frame) {
                    passed = false;
                    continue;
                }
                const QVector3D inward =
                    QVector3D(arc->center().x() - arc->end().x(), arc->center().y() - arc->end().y(), 0).normalized();
                const float dot = QVector3D::dotProduct(frame->rotatedVector(QVector3D(0, 0, 1)).normalized(), inward);
                if (!std::isfinite(dot) || (perturb_tilt ? dot >= 0.99f : dot <= 1.0f - 1.0e-5f)) {
                    std::cerr << "Unexpected tool-normal dot " << dot << " at reference line " << segment->lineNumber()
                              << '\n';
                    passed = false;
                }
            }
        }
    });
    loader.run();
    std::cout << "Checked " << print_count << (perturb_tilt ? " perturbed" : " perpendicular")
              << " helical tool orientations from " << source_path.toStdString() << '\n';
    return passed && saw_world_approach && (manual_reference.isEmpty() ? print_count == 30 : print_count > 0);
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
    segment_settings->setSetting(ORNL::SS::kHelicalToolStartAngleOffset, 0.0 * ORNL::degree);
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
    settings->setSetting(ORNL::PS::Optimizations::kCylindricalPathOrder,
                         static_cast<int>(ORNL::PathOrderOptimization::kNextClosest));
    settings->setSetting(ORNL::PS::Helical::kHelicalPathZClipRounding,
                         static_cast<int>(ORNL::HelicalPathZClipRounding::kExactIntersection));
    settings->setSetting(ORNL::PS::Helical::kHelicalToolStartArcLengthOffset, 0.0 * ORNL::mm);
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
    segment_settings->setSetting(ORNL::SS::kHelicalToolStartAngleOffset, 0.0 * ORNL::degree);
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

bool writesG80ScheduleSpeedVariableForGeneratedScheduleFile() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    settings->setSetting(ORNL::PS::Perimeter::kSpeed, 600.0 * ORNL::mm / ORNL::minute);
    settings->setSetting(ORNL::PRS::GCode::kArcSpecialtiesG80WeldScheduleFile, QString());
    settings->setSetting(ORNL::PS::ArcSpecialties::kGenerateWeldScheduleFile, true);

    ORNL::ArcSpecialtiesWriter setup_writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString setup =
        setup_writer.writeInitialSetup(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm, 1);

    ORNL::ArcSpecialtiesWriter line_writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString line_block = line_writer.writeLine(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                     ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                                                     helicalSegmentSettings(ORNL::RegionType::kPerimeter));
    const QString line       = lineContaining(line_block, ";HELICAL PERIMETER");

    return setup.contains("#FILE NAME[ G80=\"\" ]\n") && line.contains(" FV.S.SPEED ;HELICAL PERIMETER") &&
           !line.contains("F600.0000");
}

bool writesSkeletonG80ScheduleSelection() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);

    return writer.writeBeforeRegion(ORNL::RegionType::kSkeleton, 0) == "G80 [3] ;Skeleton Schedule\n";
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
    const ORNL::Distance radius = 100.0 * ORNL::mm;
    const ORNL::Distance arc_length_offset(radius() * (-12.0 * ORNL::degree)());
    segment_settings->setSetting(ORNL::SS::kHelicalToolStartAngleOffset,
                                 ORNL::HelicalToolStartAngle::angleOffsetForRadius(arc_length_offset, radius).value());
    auto point_at_angle = [radius](double angle_degrees, ORNL::Distance z) {
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

    return world_approach_line.contains("XR=180.0000 YR=0.0000 ZR=-90.0000") &&
           world_approach_line.contains("CP=-12.0000") && travel_line.contains("X=0.0000 Y=100.0000") &&
           travel_line.contains("XR=171.4524 YR=-8.4540 ZR=-134.3671") && travel_line.contains("CP=-12.0000") &&
           travel_lower_line.contains("X=0.0000 Y=100.0000") &&
           travel_lower_line.contains("XR=171.4524 YR=-8.4540 ZR=-134.3671") &&
           travel_lower_line.contains("CP=-12.0000") && arc_line.contains("X=20.7912 Y=97.8148") &&
           arc_line.contains("XR=171.4524 YR=-8.4540 ZR=-134.3671") && arc_line.contains("CP=0.0000");
}

bool keepsHelicalToolArcLengthConstantAcrossRadiiAndHandedness() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    const ORNL::Distance arc_length_offset      = 20.0 * ORNL::mm;

    auto write_at_radius = [&settings, arc_length_offset](ORNL::Distance radius,
                                                          ORNL::HelicalPathHandedness handedness) {
        QSharedPointer<ORNL::SettingsBase> segment_settings = helicalSegmentSettings(ORNL::RegionType::kPerimeter);
        segment_settings->populate(settings);
        segment_settings->setSetting(ORNL::PS::Helical::kHelicalPathHandedness, static_cast<int>(handedness));
        segment_settings->setSetting(
            ORNL::SS::kHelicalToolStartAngleOffset,
            ORNL::HelicalToolStartAngle::angleOffsetForRadius(arc_length_offset, radius).value());

        ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
        const ORNL::Point start(0.0 * ORNL::mm, radius, 0.0 * ORNL::mm);
        const ORNL::Point end(0.0 * ORNL::mm, radius, 1.0 * ORNL::mm);
        return lineContaining(writer.writeLine(start, end, segment_settings), ";HELICAL PERIMETER");
    };

    const QString right_100 = write_at_radius(100.0 * ORNL::mm, ORNL::HelicalPathHandedness::kRightHanded);
    const QString left_100  = write_at_radius(100.0 * ORNL::mm, ORNL::HelicalPathHandedness::kLeftHanded);
    const QString right_200 = write_at_radius(200.0 * ORNL::mm, ORNL::HelicalPathHandedness::kRightHanded);

    return right_100.contains("XR=-171.8429 YR=8.0756 ZR=-134.4232") && right_100.contains("CP=11.4592") &&
           left_100.contains("XR=-171.8429 YR=8.0756 ZR=-134.4232") && left_100.contains("CP=11.4592") &&
           right_200.contains("XR=-175.9418 YR=4.0480 ZR=-134.8565") && right_200.contains("CP=5.7296");
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
    settings->setSetting(ORNL::PS::Helical::kHelicalPathZClipRounding,
                         static_cast<int>(ORNL::HelicalPathZClipRounding::kCompleteRevolution));
    settings->setSetting(ORNL::SS::kHelicalToolStartAngleOffset, -12.0 * ORNL::degree);
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

    const int positive_layer_marker   = positive_layer.indexOf(";BEGINNING LAYER: 1");
    const int positive_opt_stop       = positive_layer.indexOf("V.E.OptStopMode = 1\n");
    const int positive_schedule       = positive_layer.indexOf("G80 [1] ;Perimeter Schedule\n");
    const int positive_print          = positive_layer.indexOf(";HELICAL PERIMETER\n");
    const int negative_layer_marker   = negative_layer.indexOf(";BEGINNING LAYER: 2");
    const int negative_opt_stop       = negative_layer.indexOf("V.E.OptStopMode = 2\n");
    const int negative_schedule       = negative_layer.indexOf("G80 [1] ;Perimeter Schedule\n");
    const int negative_print          = negative_layer.indexOf(";HELICAL PERIMETER\n");
    const QString positive_print_line = lineContaining(positive_layer, ";HELICAL PERIMETER");
    const QString negative_print_line = lineContaining(negative_layer, ";HELICAL PERIMETER");

    return positive_layer.contains(";BEGINNING LAYER: 1\nV.E.OptStopMode = 1\n") &&
           negative_layer.contains(";BEGINNING LAYER: 2\nV.E.OptStopMode = 2\n") && positive_layer_marker >= 0 &&
           positive_opt_stop > positive_layer_marker && positive_schedule > positive_opt_stop &&
           positive_print > positive_schedule && negative_layer_marker >= 0 &&
           negative_opt_stop > negative_layer_marker && negative_schedule > negative_opt_stop &&
           negative_print > negative_schedule && !positive_layer.contains("V.E.OptStopMode = 2") &&
           !negative_layer.contains("V.E.OptStopMode = 1") &&
           positive_print_line.contains("XR=171.4524 YR=-8.4540 ZR=-134.3671") &&
           negative_print_line.contains("XR=-171.4524 YR=8.4540 ZR=-134.3671");
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
    auto segmentSettings = [](std::optional<ORNL::RegionType> region_type) {
        QSharedPointer<ORNL::SettingsBase> segment_settings = helicalSegmentSettings(region_type);
        segment_settings->setSetting(ORNL::SS::kHelicalToolStartAngleOffset, 8.0 * ORNL::degree);
        return segment_settings;
    };
    const QString block = writer.writeLine(start, end, segmentSettings(ORNL::RegionType::kPerimeter)) %
                          writer.writeLine(start, end, segmentSettings(ORNL::RegionType::kInset)) %
                          writer.writeLine(start, end, segmentSettings(ORNL::RegionType::kInfill)) %
                          writer.writeLine(start, end, segmentSettings(ORNL::RegionType::kUnknown));

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

    return perimeter_line.contains("XR=9.2011 YR=4.2004 ZR=12.7459") &&
           inset_line.contains("XR=17.7785 YR=14.6065 ZR=21.9860") &&
           infill_line.contains("XR=26.1946 YR=25.1999 ZR=30.6946") &&
           fallback_line.contains("XR=-174.3247 YR=5.6476 ZR=-134.7198");
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
    segment_settings->setSetting(ORNL::SS::kHelicalToolStartAngleOffset, -12.0 * ORNL::degree);

    ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
    const QString travel_block = writer.writeTravel(ORNL::Point(1.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm),
                                                    ORNL::Point(0.0 * ORNL::mm, 1.0 * ORNL::mm, 1.0 * ORNL::mm),
                                                    ORNL::TravelLiftType::kNoLift, segment_settings);

    const QString world_approach_line = lineContaining(travel_block, ";WORLD APPROACH TRAVEL");
    const QString first_travel_line   = lineContaining(travel_block, ";TRAVEL");

    return world_approach_line.contains("XR=180.0000 YR=0.0000 ZR=-90.0000") &&
           first_travel_line.contains("XR=53.7793 YR=50.1307 ZR=52.2443");
}

double motionField(const QString& line, const QString& field) {
    const auto match = QRegularExpression("(?:^|\\s)" + field + "=([-+0-9.]+)").match(line);
    return match.hasMatch() ? match.captured(1).toDouble() : std::numeric_limits<double>::quiet_NaN();
}

bool matchesDonutReferenceToolFrames() {
    // Ordinary (untilted) moves from issue #372's layer 9/10 modified NC files.
    // Layer 10 reverses the generated path, so its effective arc-length offset is positive.
    struct Reference {
        double radius_mm, offset_mm, zr, xr_output, yr_output, zr_output, cp_output;
    };
    const Reference references[] = {{148.764, -25.4, -180.0, 180.0, -9.7827, -180.0, 2.2173},
                                    {152.064, 25.4, -135.0, -173.2011, 6.7514, -134.5985, -2.4296}};
    for (const Reference& reference : references) {
        auto settings = helicalWriterSettings(true);
        settings->setSetting(ORNL::PS::Helical::kHelicalPerimeterToolFrameZRotation, reference.zr * ORNL::degree);
        settings->setSetting(ORNL::PRS::MachineSetup::kAxisA, 90.0 * ORNL::degree);
        auto params = helicalSegmentSettings(ORNL::RegionType::kPerimeter);
        params->setSetting(ORNL::PS::Helical::kHelicalPathHandedness,
                           static_cast<int>(ORNL::HelicalPathHandedness::kLeftHanded));
        params->setSetting(ORNL::SS::kHelicalToolStartAngleOffset,
                           ORNL::HelicalToolStartAngle::angleOffsetForRadius(reference.offset_mm * ORNL::mm,
                                                                             reference.radius_mm * ORNL::mm)
                               .value());
        const double sweep          = reference.offset_mm < 0.0 ? -12.0 : 12.0;
        const double angle          = (90.0 + sweep) * M_PI / 180.0;
        const ORNL::Distance radius = reference.radius_mm * ORNL::mm;
        const ORNL::Point start(0.0 * ORNL::mm, radius, 0.0 * ORNL::mm);
        const ORNL::Point end(radius * std::cos(angle), radius * std::sin(angle), 1.0 * ORNL::mm);
        ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
        const QString line = lineContaining(
            writer.writeArc(start, end, ORNL::Point(), std::abs(sweep) * ORNL::degree, sweep > 0.0, params),
            ";HELICAL PERIMETER");
        // Hand-calculated reference angles are approximate; allow one unit in the last output decimal.
        if (!ORNL::Testing::near(motionField(line, "XR"), reference.xr_output, 0.00011) ||
            !ORNL::Testing::near(motionField(line, "YR"), reference.yr_output, 0.00011) ||
            !ORNL::Testing::near(motionField(line, "ZR"), reference.zr_output, 0.00011) ||
            !ORNL::Testing::near(motionField(line, "CP"), reference.cp_output, 0.00011) ||
            !ORNL::Testing::near(motionField(line, "AP"), 90.0)) {
            std::cerr << line.toStdString() << '\n';
            return false;
        }
    }
    return true;
}

bool preservesHelicalToolFrameAfterUndoingPositionerOffset() {
    struct Frame {
        double xr, yr, zr;
    };
    const Frame frames[] = {
        {180.0, 0.0, -135.0}, {180.0, 0.0, -180.0}, {11.0, 12.0, 13.0}, {45.0, 30.0, 0.0}, {0.0, 0.0, 0.0}};
    for (const Frame& frame : frames) {
        for (double offset : {-90.0, -60.0, -12.0, 0.0, 8.0, 90.0}) {
            auto settings = helicalWriterSettings(false);
            setHelicalToolFrameSettings(settings);
            auto params = helicalSegmentSettings(ORNL::RegionType::kPerimeter);
            params->setSetting(ORNL::PS::Helical::kHelicalPerimeterToolFrameXRotation, frame.xr * ORNL::degree);
            params->setSetting(ORNL::PS::Helical::kHelicalPerimeterToolFrameYRotation, frame.yr * ORNL::degree);
            params->setSetting(ORNL::PS::Helical::kHelicalPerimeterToolFrameZRotation, frame.zr * ORNL::degree);
            // Exact radians exercise pitch singularities despite the approximate degree unit constant.
            params->setSetting(ORNL::SS::kHelicalToolStartAngleOffset, ORNL::Angle(offset * M_PI / 180.0));
            ORNL::ArcSpecialtiesWriter writer(ORNL::GcodeMetaList::ArcSpecialtiesMeta, settings);
            const QString line = lineContaining(
                writer.writeLine(ORNL::Point(0.0, 1000.0, 0.0), ORNL::Point(0.0, 1000.0, 1000.0), params),
                ";HELICAL PERIMETER");
            const auto commands = orientationCommands({line});
            if (commands.size() != 1 || commands.first().size() != 1 ||
                !toolFrameMatches(commands.first().first().toolOrientation(), motionField(line, "XR"),
                                  motionField(line, "YR"), motionField(line, "ZR"))) {
                return false;
            }
            QMatrix4x4 configured;
            configured.rotate(frame.zr, 0.0f, 0.0f, 1.0f);
            configured.rotate(frame.yr, 0.0f, 1.0f, 0.0f);
            configured.rotate(frame.xr, 1.0f, 0.0f, 0.0f);
            QMatrix4x4 restored;
            restored.rotate(offset, 0.0f, 1.0f, 0.0f);
            restored.rotate(motionField(line, "ZR"), 0.0f, 0.0f, 1.0f);
            restored.rotate(motionField(line, "YR"), 0.0f, 1.0f, 0.0f);
            restored.rotate(motionField(line, "XR"), 1.0f, 0.0f, 0.0f);
            // Check the entire frame, including the torch's Z axis, after undoing the CP offset.
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    if (!ORNL::Testing::near(restored(row, col), configured(row, col), 2.0e-6)) {
                        std::cerr << "Frame offset " << offset << ": " << line.toStdString() << '\n';
                        return false;
                    }
                }
            }
            if (offset == 0.0 && (motionField(line, "XR") != frame.xr || motionField(line, "YR") != frame.yr ||
                                  motionField(line, "ZR") != frame.zr)) {
                return false;
            }
        }
    }
    return true;
}

bool writesHelicalToolFrameHeader() {
    QSharedPointer<ORNL::SettingsBase> settings = helicalWriterSettings(false);
    setHelicalToolFrameSettings(settings);
    settings->setSetting(ORNL::PS::Helical::kHelicalToolStartArcLengthOffset, 8.0 * ORNL::mm);
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
           header.contains(";Helical Travel Tool Frame Rotation: XR=41.0000deg YR=42.0000deg ZR=43.0000deg") &&
           header.contains(";Initial World Approach Tool Frame Rotation: XR=180.0000deg YR=0.0000deg ZR=-90.0000deg") &&
           header.contains(";Helical Tool Start Arc Length Offset: 8.0000mm") &&
           header.contains(
               ";Helical Tool Offset Conversion: signed angle = arc length / actual generated path radius") &&
           header.contains("the torch frame rotates about work-frame Y by the opposite angle") &&
           !header.contains("XR/YR each receive half");
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

bool infersLeftHandedHelicalAxisFromReversedCpDelta() {
    ORNL::Point center;
    const bool inferred = ORNL::ArcSpecialtiesAxisInference::cylindricalAxisFromCpDelta(
        QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 1.0, 0.0), 0.0, 270.0, true, std::nullopt, center);

    return inferred && ORNL::Testing::near2DPoint(center, 0.0, 0.0, kTolerance);
}

bool rightHandedCpDeltaDoesNotMirrorAxis() {
    ORNL::Point center;
    const bool inferred = ORNL::ArcSpecialtiesAxisInference::cylindricalAxisFromCpDelta(
        QVector3D(1.0, 0.0, 0.0), QVector3D(0.0, 1.0, 0.0), 0.0, 270.0, false, std::nullopt, center);

    return inferred && !ORNL::Testing::near(center.x(), 0.0, kTolerance) &&
           !ORNL::Testing::near(center.y(), 0.0, kTolerance);
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
    settings->setSetting(ORNL::PS::Helical::kHelicalToolStartArcLengthOffset, 0.0 * ORNL::mm);
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
    QStandardPaths::setTestModeEnabled(true);
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
    passed &= ORNL::Testing::expect(retainsToolFramesForAllMotionForms(),
                                    "Arc Specialties G0-G3 did not preserve the programmed ZYX tool frame.");
    passed &=
        ORNL::Testing::expect(retainsModalPositionerFrames(),
                              "Arc Specialties modal AP/CP updates changed the raw tool frame or CP inference data.");
    passed &= ORNL::Testing::expect(positionerRequiresBothKnownAxes(),
                                    "Arc Specialties invented a positioner frame before both AP and CP were known.");
    passed &=
        ORNL::Testing::expect(positionerHandlesSignedAndUnwrappedAngles(),
                              "Arc Specialties positioner frame failed signed, wrapped, or unwrapped angle checks.");
    passed &=
        ORNL::Testing::expect(positionerHonorsTrafoTransitions(),
                              "Arc Specialties positioner frames did not respect TRAFO or world-approach motion.");
    passed &= ORNL::Testing::expect(donutReferenceUsesWorkObjectToolFrames(false),
                                    "Donut1827 helical tool frames were not perpendicular in work-object coordinates.");
    passed &= ORNL::Testing::expect(donutReferenceUsesWorkObjectToolFrames(true),
                                    "Changing the Donut1827 programmed torch tilt did not remain visible.");
    passed &= ORNL::Testing::expect(retainsPartialAndOrientationOnlyUpdates(),
                                    "Arc Specialties modal or orientation-only tool-frame updates were lost.");
    passed &= ORNL::Testing::expect(copiesAndClearsToolFrames(),
                                    "Tool orientation did not survive copying or clear independently.");
    passed &= ORNL::Testing::expect(clearsToolFramesWhenReconfigured(),
                                    "Arc Specialties parser retained a stale tool frame after reset.");
    passed &= ORNL::Testing::expect(ordinarySyntaxHasNoToolFrame(),
                                    "Common syntax extrusion axes were incorrectly treated as tool orientation.");
    passed &= ORNL::Testing::expect(
        loaderPreservesToolFrames("Arc Specialties",
                                  "#TRAFO OFF\n"
                                  "G0 X=1 Y=0 Z=0 XR=171.4524 YR=-8.4540 ZR=-134.3671 AP=90 CP=-12 ;TRAVEL\n"
                                  "G1 X=1 Y=1 F600 ;HELICAL PERIMETER\n"
                                  "G3 X=0 Y=0 I=-1 J=0 F600 ;HELICAL PERIMETER\n",
                                  true),
        "Arc Specialties loader did not propagate compensated tool frames to line and arc segments.");
    passed &= ORNL::Testing::expect(
        loaderPreservesToolFrames("Marlin", "G0 X1 Y0 Z0 ;TRAVEL\nG1 X1 Y1 F600\nG3 X0 Y0 I-1 J0 F600\n", false),
        "Loader invented a tool frame for an unsupported syntax.");
    passed &= ORNL::Testing::expect(parsesArcLine(clockwise_arc), "Arc Specialties G02 did not ignore inline G81.");
    passed &=
        ORNL::Testing::expect(parsesArcLine(counter_clockwise_arc), "Arc Specialties G03 did not ignore inline G81.");
    passed &= ORNL::Testing::expect(parsesArcLine(clockwise_schedule_speed_arc),
                                    "Arc Specialties G02 did not accept the G80 schedule speed variable.");
    passed &= ORNL::Testing::expect(parsesArcLine(counter_clockwise_schedule_speed_arc),
                                    "Arc Specialties G03 did not accept the G80 schedule speed variable.");
    passed &= ORNL::Testing::expect(parsedArcKeepsCpForVisualization(clockwise_arc),
                                    "Arc Specialties parser did not retain CP for visualization.");
    passed &= ORNL::Testing::expect(parsedArcKeepsCpForVisualization(clockwise_schedule_speed_arc, false),
                                    "Arc Specialties parser did not retain CP with the G80 schedule speed variable.");
    passed &= ORNL::Testing::expect(parsedLineKeepsCpForVisualization(),
                                    "Arc Specialties parser did not retain linear CP for visualization.");
    passed &=
        ORNL::Testing::expect(parsedLineWithScheduleSpeedKeepsCpForVisualization(),
                              "Arc Specialties parser did not retain linear CP with the G80 schedule speed variable.");
    passed &= ORNL::Testing::expect(parsesArcSpecialtiesOptStopModeAssignment(),
                                    "Arc Specialties parser did not accept the OptStopMode assignment.");
    passed &= ORNL::Testing::expect(parsesNumberedArcSpecialtiesMotion(),
                                    "Arc Specialties parser did not accept Beckhoff block numbers.");
    passed &= ORNL::Testing::expect(rejectsDuplicateScheduleSpeedFeedrate(),
                                    "Arc Specialties parser did not reject duplicate schedule speed feedrates.");
    passed &= ORNL::Testing::expect(infersLeftHandedHelicalAxisFromReversedCpDelta(),
                                    "Arc Specialties loader did not reverse left-handed helical CP delta.");
    passed &= ORNL::Testing::expect(rightHandedCpDeltaDoesNotMirrorAxis(),
                                    "Arc Specialties loader unexpectedly reversed right-handed CP delta.");
    passed &= ORNL::Testing::expect(writesInlineArcOptionalStop(),
                                    "Arc Specialties writer did not emit inline G81 on G02/G03.");
    passed &= ORNL::Testing::expect(writesConfiguredG80WeldScheduleFile(),
                                    "Arc Specialties writer did not emit the configured G80 weld schedule file.");
    passed &=
        ORNL::Testing::expect(writesNumericSpeedWhenG80ScheduleFileIsEmpty(),
                              "Arc Specialties writer did not emit numeric speed without a G80 weld schedule file.");
    passed &=
        ORNL::Testing::expect(writesG80ScheduleSpeedVariableForLineAndArc(),
                              "Arc Specialties writer did not emit the G80 schedule speed variable for print motion.");
    passed &= ORNL::Testing::expect(
        writesG80ScheduleSpeedVariableForGeneratedScheduleFile(),
        "Arc Specialties writer did not use the G80 schedule speed variable for generated schedule files.");
    passed &= ORNL::Testing::expect(writesSkeletonG80ScheduleSelection(),
                                    "Arc Specialties writer did not emit the Skeleton G80 schedule selection.");
    passed &= ORNL::Testing::expect(writesHelicalCpFromStartOffsetBaseline(),
                                    "Arc Specialties writer did not preserve the helical start-offset CP baseline.");
    passed &= ORNL::Testing::expect(
        keepsHelicalToolArcLengthConstantAcrossRadiiAndHandedness(),
        "Arc Specialties writer did not convert one helical tool arc length across radii and handedness.");
    passed &= ORNL::Testing::expect(writesLayerScopedBlockNumbersWhenEnabled(),
                                    "Arc Specialties writer did not emit layer-scoped block numbers.");
    passed &= ORNL::Testing::expect(writesHelicalOptStopModeFromPostOrderingRotationDirection(),
                                    "Arc Specialties writer did not emit helical OptStopMode from rotation direction.");
    passed &= ORNL::Testing::expect(writesCompactCylindricalPrintComments(),
                                    "Arc Specialties writer did not emit compact cylindrical comments.");
    passed &= ORNL::Testing::expect(writesHelicalRegionLineComments(),
                                    "Arc Specialties writer did not emit helical line region comments.");
    passed &= ORNL::Testing::expect(writesHelicalRegionArcComments(),
                                    "Arc Specialties writer did not emit helical arc region comments.");
    passed &= ORNL::Testing::expect(writesGenericHelicalCommentForMissingOrUnknownRegion(),
                                    "Arc Specialties writer did not fall back to generic helical comments.");
    passed &= ORNL::Testing::expect(helicalLayerFinalizesOnlyAtPhysicalPathEnd(),
                                    "Helical layer emitted region end G-code at internal region transitions.");
    passed &= ORNL::Testing::expect(writesHelicalRegionToolFrameRotations(),
                                    "Arc Specialties writer did not emit helical region tool-frame rotations.");
    passed &= ORNL::Testing::expect(writesHelicalTravelToolFrameRotation(),
                                    "Arc Specialties writer did not emit the helical travel tool-frame rotation.");
    passed &= ORNL::Testing::expect(matchesDonutReferenceToolFrames(),
                                    "Arc Specialties torch correction does not match the Donut layer 9/10 references.");
    passed &= ORNL::Testing::expect(preservesHelicalToolFrameAfterUndoingPositionerOffset(),
                                    "Arc Specialties torch frame changed after undoing the positioner offset.");
    passed &=
        ORNL::Testing::expect(writesHelicalToolFrameHeader(),
                              "Arc Specialties writer did not report helical tool-frame rotations in the header.");
    passed &= ORNL::Testing::expect(writesFirstTravelWithWorkObjectToolFrame(),
                                    "Arc Specialties first travel did not use ZR=-135.");
    passed &= ORNL::Testing::expect(writesStartupWorldApproachAbovePartOrCylinderHeight(),
                                    "Arc Specialties startup world approach did not use part/cylinder safe Z.");
    passed &= ORNL::Testing::expect(writesStartupWorldApproachAboveGeneratedHelicalMaxZ(),
                                    "Arc Specialties startup world approach did not use generated helical safe Z.");
    passed &= ORNL::Testing::expect(writesHelicalZClipRoundingHeader(),
                                    "Arc Specialties writer did not emit the helical Z clip rounding header.");
    passed &= ORNL::Testing::expect(writesCylindricalTravelWithConfiguredArcDensity(),
                                    "Arc Specialties cylindrical travel did not honor Arcs per Revolution.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
