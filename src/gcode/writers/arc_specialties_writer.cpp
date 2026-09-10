#include "gcode/writers/arc_specialties_writer.h"

#include <math.h>
#include <sys/types.h>

#include <QStringBuilder>
#include <algorithm>
#include <cmath>
#include <limits>

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
//! @brief Segment setting key used to recover the cylinder center X.
const QString kRadialCenterX = "radial_center_x";

//! @brief Segment setting key used to recover the cylinder center Y.
const QString kRadialCenterY = "radial_center_y";

//! @brief First TRAFO-off move comment used to keep the approach orientation distinct from work-object motion.
const QString kWorldApproachTravelComment = "WORLD APPROACH TRAVEL";

//! @brief Default Arc Specialties G80 weld schedule file path.
const QString kDefaultG80WeldScheduleFile = "";

//! @brief Arc Specialties schedule-selected speed variable.
const QString kG80ScheduleSpeedVariable = "V.S.SPEED";

//! @brief Fixed tool-frame XR.
constexpr double kToolFrameXR = 180.0;

//! @brief Fixed tool-frame YR.
constexpr double kToolFrameYR = 0.0;

//! @brief Fixed tool-frame ZR.
constexpr double kToolFrameZR = -135.0;

//! @brief Tool-frame ZR used for rapid travel moves.
constexpr double kRapidTravelToolFrameZR = -90.0;

//! @brief First-pass clearance above the highest point of the build parts for the startup world approach.
static const Distance kStartupWorldApproachZBuffer = 100.0 * mm;

//! @brief Returns a point offset away from the radial cylinder axis by the configured lift height.
Point radialLiftedPoint(const Point& point, const QSharedPointer<SettingsBase>& params, Distance lift_height) {
    const double center_x = params->setting<Distance>(kRadialCenterX)();
    const double center_y = params->setting<Distance>(kRadialCenterY)();
    const double dx       = point.x() - center_x;
    const double dy       = point.y() - center_y;
    const double length   = std::hypot(dx, dy);

    if (length <= std::numeric_limits<double>::epsilon()) {
        // There is no outward radial direction on the cylinder axis.
        return point;
    }

    const double scale = lift_height() / length;
    return Point(point.x() + dx * scale, point.y() + dy * scale, point.z());
}

//! @brief Returns the XY radius of a point around the radial cylinder axis.
double radialDistance(const Point& point, const QSharedPointer<SettingsBase>& params) {
    const double center_x = params->setting<Distance>(kRadialCenterX)();
    const double center_y = params->setting<Distance>(kRadialCenterY)();
    return std::hypot(point.x() - center_x, point.y() - center_y);
}

//! @brief Returns the shortest signed angular sweep from start to end.
double shortestAngularDelta(double start_angle, double end_angle) {
    double delta = end_angle - start_angle;
    while (delta > M_PI) { delta -= 2.0 * M_PI; }
    while (delta < -M_PI) { delta += 2.0 * M_PI; }
    return delta;
}

//! @brief Normalizes an angle in degrees to [0, 360).
double normalizeDegrees(double degrees) {
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0) { degrees += 360.0; }

    return degrees;
}

//! @brief Machine setup setting value for relative G02/G03 center interpretation.
constexpr int kRelativeArcCenterMode = 1;

//! @brief Default G83 mode: stop welding only.
constexpr int kDefaultG83Mode = 0;

//! @brief Highest supported Arc Specialties G83 mode.
constexpr int kMaxG83Mode = 4;

//! @brief Returns a supported G83 mode, falling back to weld-stop-only behavior for invalid input.
int validatedG83Mode(int mode) {
    return mode >= kDefaultG83Mode && mode <= kMaxG83Mode ? mode : kDefaultG83Mode;
}

//! @brief Formats a distance using the output unit declared by the active gcode metadata.
QString formatDistance(Distance value, Distance unit) {
    return QString::number(value.to(unit), 'f', 4) % unit.toString();
}

//! @brief Formats an angle using the output unit declared by the active gcode metadata.
QString formatAngle(Angle value, Angle unit) {
    return QString::number(value.to(unit), 'f', 4) % unit.toString();
}

//! @brief Returns the display label for supported cylindrical path order choices.
QString cylindricalPathOrderText(PathOrderOptimization path_order) {
    return path_order == PathOrderOptimization::kNextFarthest ? "Next Farthest" : "Next Closest";
}

//! @brief Resolves a zero helical region stepover to the default bead-width pitch.
Distance helicalRegionPitch(Distance stepover, Distance bead_width) {
    return stepover() == 0.0 ? bead_width : stepover;
}

//! @brief Returns a helical region comment, or generic HELICAL when region metadata is not usable.
QString helicalRegionComment(RegionType region_type) {
    switch (region_type) {
        case RegionType::kPerimeter:
            return Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kPerimeter;
        case RegionType::kInset:
            return Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kInset;
        case RegionType::kInfill:
            return Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kInfill;
        default:
            return Constants::RegionTypeStrings::kHelical;
    }
}
}  // namespace

ArcSpecialtiesWriter::ArcSpecialtiesWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb)
    : WriterBase(meta, sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
}

void ArcSpecialtiesWriter::setHelicalPathZClipRounding(
    const QVector<QPair<QString, HelicalPathZClipRounding>>& rounding) {
    m_helical_path_z_clip_rounding = rounding;
}

void ArcSpecialtiesWriter::setHelicalPathHandedness(const QVector<QPair<QString, HelicalPathHandedness>>& handedness) {
    m_helical_path_handedness = handedness;
}

void ArcSpecialtiesWriter::startLayerBlockNumbering() {
    if (!shouldEmitBlockNumbers()) { return; }

    initializeLayerBlockNumbering();
    m_layer_block_numbering_active = true;
}

void ArcSpecialtiesWriter::initializeLayerBlockNumbering() {
    if (!m_layer_block_numbering_started) {
        m_next_block_number             = std::max(1, m_current_layer) * 10000;
        m_layer_block_numbering_started = true;
    }
}

void ArcSpecialtiesWriter::stopLayerBlockNumbering() {
    m_layer_block_numbering_active = false;
}

bool ArcSpecialtiesWriter::shouldEmitBlockNumbers() const {
    return m_sb != nullptr && m_sb->contains(PRS::GCode::kArcSpecialtiesEmitBlockNumbers) &&
           m_sb->setting<bool>(PRS::GCode::kArcSpecialtiesEmitBlockNumbers);
}

QString ArcSpecialtiesWriter::writeNumberedBlock(const QString& block, bool force) {
    if (!shouldEmitBlockNumbers() || (!m_layer_block_numbering_active && !force) || block.isEmpty()) { return block; }

    initializeLayerBlockNumbering();

    QString rv;
    int line_start = 0;
    while (line_start < block.size()) {
        const int newline_index = block.indexOf(m_newline, line_start);
        const bool has_newline  = newline_index >= 0;
        const QString line = has_newline ? block.mid(line_start, newline_index - line_start) : block.mid(line_start);
        const QString trimmed_line = line.trimmed();

        if (trimmed_line.isEmpty() || (!m_meta.m_comment_starting_delimiter.isEmpty() &&
                                       trimmed_line.startsWith(m_meta.m_comment_starting_delimiter))) {
            rv += line;
        }
        else {
            rv += "N" % QString::number(m_next_block_number) % " " % line;
            ++m_next_block_number;
        }

        if (!has_newline) { break; }

        rv += m_newline;
        line_start = newline_index + 1;
    }

    return rv;
}

QString ArcSpecialtiesWriter::writeSettingsHeader(GcodeSyntax) {
    QString text;
    auto formatToolFrameRotation = [](const ToolFrameRotation& rotation) {
        return QString("XR=") % QString::number(rotation.xr, 'f', 4) % "deg YR=" %
               QString::number(rotation.yr, 'f', 4) % "deg ZR=" % QString::number(rotation.zr, 'f', 4) % "deg";
    };

    const SlicingMode slicing_mode = static_cast<SlicingMode>(m_sb->setting<int>(PS::Slicing::kSlicingMode));
    const CylindricalPathPattern path_pattern =
        static_cast<CylindricalPathPattern>(m_sb->setting<int>(PS::Slicing::kCylindricalPathPattern));
    const bool cylindrical_mode = slicing_mode == SlicingMode::kCylindrical;
    const bool helical_mode     = path_pattern == CylindricalPathPattern::kHelical;
    if (cylindrical_mode) {
        text += commentLine(helical_mode ? "Arc Specialties Helical Slicing Parameters"
                                         : "Arc Specialties Radial Slicing Parameters");
        text += commentLine("Cylindrical Path Pattern: " % toString(path_pattern));
        text += commentLine("Cylindrical Path Order Optimization: " %
                            cylindricalPathOrderText(static_cast<PathOrderOptimization>(
                                m_sb->setting<int>(PS::Optimizations::kCylindricalPathOrder))));
        text += commentLine(
            "Motion Coordinates: X/Y/Z are user-frame endpoint coordinates relative to the active work "
            "offset");
        text += commentLine(
            "G-Code Coordinate Frame Rotation: X=" %
            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kGCodeCoordinateFrameRotationX), m_meta.m_angle_unit) %
            " Y=" %
            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kGCodeCoordinateFrameRotationY), m_meta.m_angle_unit) %
            " Z=" %
            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kGCodeCoordinateFrameRotationZ), m_meta.m_angle_unit));
        text += commentLine("Arc Specialties partner frame: set G-Code Frame Rotation Z to -90deg");
        text += commentLine("Work Offset Setup: manual and probe setup commands are not emitted by this first pass");
        if (helical_mode) {
            text += commentLine(
                "Helical Perimeter Tool Frame Rotation: " %
                formatToolFrameRotation(toolFrameRotationForMotion(
                    Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kPerimeter, m_sb)));
            text += commentLine(
                "Helical Inset Tool Frame Rotation: " %
                formatToolFrameRotation(toolFrameRotationForMotion(
                    Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kInset, m_sb)));
            text += commentLine(
                "Helical Infill Tool Frame Rotation: " %
                formatToolFrameRotation(toolFrameRotationForMotion(
                    Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kInfill, m_sb)));
            text += commentLine("Helical Travel Tool Frame Rotation: " %
                                formatToolFrameRotation(toolFrameRotationForMotion("TRAVEL", m_sb)));
        }
        else {
            text += commentLine(QString("Tool Frame Rotation: XR=") % QString::number(kToolFrameXR, 'f', 4) %
                                "deg YR=" % QString::number(kToolFrameYR, 'f', 4) % "deg ZR=" %
                                QString::number(kToolFrameZR, 'f', 4) % "deg");
        }
        text += commentLine(QString("Initial World Approach Tool Frame Rotation: XR=") %
                            QString::number(kToolFrameXR, 'f', 4) % "deg YR=" % QString::number(kToolFrameYR, 'f', 4) %
                            "deg ZR=" % QString::number(kRapidTravelToolFrameZR, 'f', 4) % "deg");
        text += commentLine(
            "Initial Approach: TRAFO-off world approach uses cylinder center XY and the greater of "
            "part maximum Z and Cylinder Height plus " %
            formatDistance(kStartupWorldApproachZBuffer, m_meta.m_distance_unit) % " before work-object kinematics");
        text += commentLine(
            QString("Cylinder Inner Radius: ") %
            formatDistance(m_sb->setting<Distance>(PS::Slicing::kCylinderInnerRadius), m_meta.m_distance_unit));
        text +=
            commentLine(QString("Cylinder Height: ") %
                        formatDistance(m_sb->setting<Distance>(PS::Slicing::kCylinderHeight), m_meta.m_distance_unit));
        const CylinderAxisSource cylinder_axis_source =
            static_cast<CylinderAxisSource>(m_sb->setting<int>(PS::Slicing::kCylinderAxisSource));
        text += commentLine("Cylinder Axis Source: " % toString(cylinder_axis_source));
        if (cylinder_axis_source == CylinderAxisSource::kCustomXY) {
            text +=
                commentLine("Cylinder Axis X: " % formatDistance(m_sb->setting<Distance>(PS::Slicing::kCylinderAxisX),
                                                                 m_meta.m_distance_unit));
            text +=
                commentLine("Cylinder Axis Y: " % formatDistance(m_sb->setting<Distance>(PS::Slicing::kCylinderAxisY),
                                                                 m_meta.m_distance_unit));
        }
        text += commentLine(QString(helical_mode ? "Helical Radial Spacing: " : "Radial Layer Spacing: ") %
                            formatDistance(m_sb->setting<Distance>(PS::Layer::kLayerHeight), m_meta.m_distance_unit));
        const Distance bead_width = m_sb->setting<Distance>(PS::Layer::kBeadWidth);
        if (helical_mode) {
            text += commentLine(
                "Helical Tool Start Angle Offset: " %
                formatAngle(m_sb->setting<Angle>(PS::Helical::kHelicalToolStartAngleOffset), m_meta.m_angle_unit));
            text += commentLine("Helical Region Pitch Fallback: " % formatDistance(bead_width, m_meta.m_distance_unit) %
                                " when a region stepover is 0");
            text += commentLine("Helical Perimeter Revolutions: " %
                                QString::number(m_sb->setting<int>(PS::Helical::kHelicalPerimeterRevolutions)));
            text += commentLine(
                "Helical Perimeter Pitch: " %
                formatDistance(
                    helicalRegionPitch(m_sb->setting<Distance>(PS::Helical::kHelicalPerimeterStepover), bead_width),
                    m_meta.m_distance_unit));
            text += commentLine("Helical Inset Revolutions: " %
                                QString::number(m_sb->setting<int>(PS::Helical::kHelicalInsetRevolutions)));
            text +=
                commentLine("Helical Inset Pitch: " %
                            formatDistance(helicalRegionPitch(
                                               m_sb->setting<Distance>(PS::Helical::kHelicalInsetStepover), bead_width),
                                           m_meta.m_distance_unit));
            text += commentLine(
                "Helical Infill Pitch: " %
                formatDistance(
                    helicalRegionPitch(m_sb->setting<Distance>(PS::Helical::kHelicalInfillStepover), bead_width),
                    m_meta.m_distance_unit));
            text += commentLine("Helical Infill Revolutions Rounding: " %
                                toString(static_cast<HelicalInfillRevolutionsRounding>(
                                    m_sb->setting<int>(PS::Helical::kHelicalInfillRevolutionsRounding))));
        }
        else {
            text +=
                commentLine("Radial Path Start Angle: " %
                            formatAngle(m_sb->setting<Angle>(PS::Radial::kRadialPathStartAngle), m_meta.m_angle_unit));
            text += commentLine("Vertical Bead Spacing: " % formatDistance(bead_width, m_meta.m_distance_unit));
        }
        if (helical_mode) {
            if (m_helical_path_z_clip_rounding.size() == 1) {
                text +=
                    commentLine("Helical Z Clip Rounding: " % toString(m_helical_path_z_clip_rounding.first().second));
            }
            else if (m_helical_path_z_clip_rounding.size() > 1) {
                for (const QPair<QString, HelicalPathZClipRounding>& part_rounding : m_helical_path_z_clip_rounding) {
                    const QString part_name = part_rounding.first.isEmpty() ? "Unnamed Part" : part_rounding.first;
                    text +=
                        commentLine("Helical Z Clip Rounding (" % part_name % "): " % toString(part_rounding.second));
                }
            }
            else {
                text += commentLine("Helical Z Clip Rounding: " %
                                    toString(static_cast<HelicalPathZClipRounding>(
                                        m_sb->setting<int>(PS::Helical::kHelicalPathZClipRounding))));
            }

            if (m_helical_path_handedness.size() == 1) {
                text += commentLine("Helical Path Handedness: " % toString(m_helical_path_handedness.first().second));
            }
            else if (m_helical_path_handedness.size() > 1) {
                for (const QPair<QString, HelicalPathHandedness>& part_handedness : m_helical_path_handedness) {
                    const QString part_name = part_handedness.first.isEmpty() ? "Unnamed Part" : part_handedness.first;
                    text +=
                        commentLine("Helical Path Handedness (" % part_name % "): " % toString(part_handedness.second));
                }
            }
            else {
                text += commentLine("Helical Path Handedness: " %
                                    toString(static_cast<HelicalPathHandedness>(
                                        m_sb->setting<int>(PS::Helical::kHelicalPathHandedness))));
            }
        }
        else {
            text += commentLine("Radial Path Boundary Policy: " %
                                toString(static_cast<RadialPathBoundaryPolicy>(
                                    m_sb->setting<int>(PS::Radial::kRadialPathBoundaryPolicy))));
        }
        text += commentLine("Travel Lift Distance: " %
                            formatDistance(m_sb->setting<Distance>(PS::Travel::kLiftHeight), m_meta.m_distance_unit));
        text += commentLine("AP Positioner Tilt: " %
                            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kAxisA), m_meta.m_angle_unit));
        text += commentLine("CP Positioner Offset: " %
                            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kAxisC), m_meta.m_angle_unit));
        text += commentLine(QString("Arc Feed Moves: ") %
                            (m_sb->setting<bool>(PRS::MachineSetup::kSupportG3) ? "G02/G03 enabled" : "G01 segmented"));
        if (m_sb->setting<bool>(PRS::MachineSetup::kSupportG3)) {
            text += commentLine(QString("G02/G03 Center Point Interpretation: ") %
                                (usesAbsoluteArcCenters() ? "Absolute" : "Relative"));
            if (usesAbsoluteArcCenters()) {
                text += commentLine(
                    "G02/G03 Absolute Center: I=" %
                    formatDistance(m_sb->setting<Distance>(PRS::MachineSetup::kG2G3AbsoluteI), m_meta.m_distance_unit) %
                    " J=" %
                    formatDistance(m_sb->setting<Distance>(PRS::MachineSetup::kG2G3AbsoluteJ), m_meta.m_distance_unit));
            }
        }
        text += commentLine("Arcs per Revolution: " %
                            QString::number(std::max(1, m_sb->setting<int>(PS::Slicing::kArcsPerRevolution))));
        text += m_newline;
    }
    else if (slicing_mode == SlicingMode::kPlanar) {
        text += commentLine("Arc Specialties Planar Slicing Parameters");
        text += commentLine(
            "Motion Coordinates: X/Y/Z are user-frame endpoint coordinates relative to the active work "
            "offset");
        text += commentLine(QString("Slice Plane Normal: X=") %
                            QString::number(m_sb->setting<float>(PS::Slicing::kSlicePlaneNormalX), 'f', 4) % " Y=" %
                            QString::number(m_sb->setting<float>(PS::Slicing::kSlicePlaneNormalY), 'f', 4) % " Z=" %
                            QString::number(m_sb->setting<float>(PS::Slicing::kSlicePlaneNormalZ), 'f', 4));
        text += commentLine(
            "G-Code Coordinate Frame Rotation: X=" %
            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kGCodeCoordinateFrameRotationX), m_meta.m_angle_unit) %
            " Y=" %
            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kGCodeCoordinateFrameRotationY), m_meta.m_angle_unit) %
            " Z=" %
            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kGCodeCoordinateFrameRotationZ), m_meta.m_angle_unit));
        text += commentLine("Arc Specialties partner frame: set G-Code Frame Rotation Z to -90deg");
        text += commentLine("Work Offset Setup: manual and probe setup commands are not emitted by this first pass");
        text += commentLine(QString("Tool Frame Rotation: XR=") % QString::number(kToolFrameXR, 'f', 4) % "deg YR=" %
                            QString::number(kToolFrameYR, 'f', 4) % "deg ZR=" % QString::number(kToolFrameZR, 'f', 4) %
                            "deg");
        text += commentLine(QString("Initial World Approach Tool Frame Rotation: XR=") %
                            QString::number(kToolFrameXR, 'f', 4) % "deg YR=" % QString::number(kToolFrameYR, 'f', 4) %
                            "deg ZR=" % QString::number(kRapidTravelToolFrameZR, 'f', 4) % "deg");
        text += commentLine(
            "Initial Approach: TRAFO-off world approach uses the first travel XY and part maximum Z plus " %
            formatDistance(kStartupWorldApproachZBuffer, m_meta.m_distance_unit) % " before work-object kinematics");
        text += commentLine("Travel Lift Direction: slice plane normal");
        text += commentLine("Travel Lift Distance: " %
                            formatDistance(m_sb->setting<Distance>(PS::Travel::kLiftHeight), m_meta.m_distance_unit));
        text += commentLine("AP Positioner Tilt: " %
                            formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kAxisA), m_meta.m_angle_unit));
        text += commentLine(
            "Planar CP Positioner: " %
            formatAngle(Angle(normalizeDegrees(m_sb->setting<Angle>(PRS::MachineSetup::kAxisC).to(degree)) * degree),
                        m_meta.m_angle_unit));
        text += commentLine(QString("Arc Feed Moves: ") %
                            (m_sb->setting<bool>(PRS::MachineSetup::kSupportG3) ? "G02/G03 enabled" : "G01 fallback"));
        text += commentLine(QString("G02/G03 Center Point Interpretation: ") %
                            (usesAbsoluteArcCenters() ? "Absolute" : "Relative"));
        if (usesAbsoluteArcCenters()) {
            text += commentLine(
                "G02/G03 Absolute Center: I=" %
                formatDistance(m_sb->setting<Distance>(PRS::MachineSetup::kG2G3AbsoluteI), m_meta.m_distance_unit) %
                " J=" %
                formatDistance(m_sb->setting<Distance>(PRS::MachineSetup::kG2G3AbsoluteJ), m_meta.m_distance_unit));
        }
        text += m_newline;
    }

    return text;
}

QString ArcSpecialtiesWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x,
                                                Distance maximum_y, int num_layers) {
    m_first_travel                     = true;
    m_layer_start                      = true;
    m_absolute_arc_center_mode_enabled = usesAbsoluteArcCenters();
    m_startup_kinematics_written       = false;
    m_pending_layer_change.clear();
    m_current_bead                  = 0;
    m_current_layer                 = 0;
    m_next_block_number             = 0;
    m_layer_block_numbering_started = false;
    m_layer_block_numbering_active  = false;
    setFeedrate(0.0);

    QString rv;

    const QString g80_schedule_file = g80WeldScheduleFile();

    rv += commentLine("DEFINE G80 WELD SCHEDULES");
    rv += "#FILE NAME[ G80=\"" % g80_schedule_file % "\" ]" % m_newline;
    rv += "#DELETE V.S.LASTBLOCK" % m_newline;
    rv += "$IF EXIST[V.S.SPEED]==FALSE" % m_newline;
    rv += "M00" % m_newline;
    rv += "#DELETE V.S.SPEED" % m_newline;
    rv += "#VAR" % m_newline;
    rv += "V.S.SPEED" % m_newline;
    rv += "#ENDVAR" % m_newline;
    rv += "M00" % m_newline;
    rv += "$ENDIF" % m_newline;
    rv += m_newline;

    rv += "G80 [0] ;Default Schedule (Infill)" % m_newline;
    rv += "#CONTOUR MODE [DEV PATH_DEV=2 CONST_VEL=1]" % m_newline;
    /// TODO: M06 command does not work as of 2026-08-07 and is disabled for now. Must be re-enabled when the M06
    /// command is fixed in the Arc Specialties controller or be replaced with a different command that achieves the
    /// same effect.
    rv += ";M06 T1 ;Select Tool 1" % m_newline;
    rv += "M49 ;Send Robot Home" % m_newline;
    rv += "#CHANNEL INIT [CMDPOS]" % m_newline;
    rv += m_newline;

    rv += "G90" % m_newline;
    rv += "#TRAFO OFF" % m_newline;
    rv += "#FLUSH WAIT" % m_newline;

    if (m_sb->setting<int>(PRS::GCode::kEnableBoundingBox)) {
        rv += commentLine(QString("Bounding Box: X=") % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) %
                          " Y=" % QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % " to X=" %
                          QString::number(maximum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y=" %
                          QString::number(maximum_y.to(m_meta.m_distance_unit), 'f', 4));
    }

    if (!m_sb->setting<QString>(PRS::GCode::kStartCode).isEmpty()) {
        rv += m_sb->setting<QString>(PRS::GCode::kStartCode) % m_newline;
    }

    rv += m_newline % commentLine(m_meta.m_layer_count_delimiter % ":" % QString::number(num_layers)) % m_newline;
    return rv;
}

QString ArcSpecialtiesWriter::writeLayerChange(uint layer_number) {
    const QString layer_change = WriterBase::writeLayerChange(layer_number);
    if (!m_startup_kinematics_written || isHelicalPathPattern()) {
        m_pending_layer_change += layer_change;
        return QString();
    }

    return layer_change;
}

QString ArcSpecialtiesWriter::writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) {
    QString rv;
    m_layer_start  = true;
    m_current_bead = 1;
    m_current_layer++;
    m_next_block_number             = std::max(1, m_current_layer) * 10000;
    m_layer_block_numbering_started = false;
    m_layer_block_numbering_active  = false;
    m_helical_opt_stop_mode_written = false;
    m_pending_layer_prefix.clear();
    m_pending_region_schedule.clear();
    return rv;
}

QString ArcSpecialtiesWriter::writeBeforePart(QVector3D normal) {
    return QString();
}

QString ArcSpecialtiesWriter::writeBeforeIsland() {
    return QString();
}

QString ArcSpecialtiesWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    if (type == RegionType::kPerimeter) { rv += "G80 [1] ;Perimeter Schedule" % m_newline; }
    else if (type == RegionType::kInset) { rv += "G80 [2] ;Inset Schedule" % m_newline; }
    else if (type == RegionType::kInfill) { rv += "G80 [0] ;Infill Schedule" % m_newline; }
    if (isHelicalPathPattern()) {
        m_pending_region_schedule += rv;
        return QString();
    }

    return writeNumberedBlock(rv, true);
}

QString ArcSpecialtiesWriter::writeBeforePath(RegionType type) {
    m_region_type = type;
    return QString();
}

QString ArcSpecialtiesWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                          QSharedPointer<SettingsBase> params) {
    QString rv;
    QString layer_rv;
    Velocity speed = params->setting<Velocity>(PS::Travel::kSpeed);
    if (speed <= 0) { speed = params->setting<Velocity>(PRS::MachineSpeed::kMaxXYSpeed); }

    Velocity lift_speed = m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed);
    if (lift_speed <= 0) { lift_speed = speed; }

    // Determine if travel length is short enough to keep welder on
    Distance travel_distance = start_location.distance(target_location);
    if (m_deposition_active && travel_distance > m_sb->setting<Distance>(PS::Travel::kMinTravelLength)) {
        layer_rv += writeWelderOff();
    }
    else if (!m_first_travel && travel_distance < m_sb->setting<Distance>(PS::Travel::kMinTravelLength)) {
        layer_rv += writeWelderOn();
    }

    const Distance lift_height = m_sb->setting<Distance>(PS::Travel::kLiftHeight);
    bool travel_lift_required  = lift_height > 0 && lType != TravelLiftType::kNoLift;
    if (start_location.distance(target_location) < m_sb->setting<Distance>(PS::Travel::kMinTravelForLift)) {
        travel_lift_required = false;
    }

    const bool cylindrical_mode = isCylindricalSlicingMode();

    auto writeBeginningBead = [this]() -> QString {
        QString rv;
        rv += commentLine(QString("BEGINNING BEAD: ") % QString::number(m_current_layer) % "." %
                          QString::number(m_current_bead));
        m_current_bead++;
        startLayerBlockNumbering();
        return rv;
    };

    auto writeRadialArcTravel = [this, &params, &writeBeginningBead](Point start, Point end,
                                                                     Velocity move_speed) -> QString {
        QString rv;
        const double center_x     = params->setting<Distance>(kRadialCenterX)();
        const double center_y     = params->setting<Distance>(kRadialCenterY)();
        const double start_radius = radialDistance(start, params);
        const double end_radius   = radialDistance(end, params);

        if (start_radius <= std::numeric_limits<double>::epsilon() ||
            end_radius <= std::numeric_limits<double>::epsilon()) {
            rv += writeMotion("G00", end, move_speed, params, "TRAVEL");
        }
        else {
            const double start_angle          = std::atan2(start.y() - center_y, start.x() - center_x);
            const double end_angle            = std::atan2(end.y() - center_y, end.x() - center_x);
            const double delta_angle          = shortestAngularDelta(start_angle, end_angle);
            const int arcs_per_revolution     = std::max(1, params->contains(PS::Slicing::kArcsPerRevolution)
                                                                ? params->setting<int>(PS::Slicing::kArcsPerRevolution)
                                                                : m_sb->setting<int>(PS::Slicing::kArcsPerRevolution));
            const double target_segment_angle = (2.0 * M_PI) / static_cast<double>(arcs_per_revolution);
            const double travel_angle         = std::abs(delta_angle);

            if (travel_angle <= target_segment_angle) { rv += writeMotion("G00", end, move_speed, params, "TRAVEL"); }
            else {
                const int segments =
                    std::clamp(static_cast<int>(std::ceil(travel_angle / target_segment_angle)), 2, 180);

                for (int i = 1; i <= segments; ++i) {
                    const double t      = static_cast<double>(i) / static_cast<double>(segments);
                    const double angle  = start_angle + delta_angle * t;
                    const double radius = start_radius + (end_radius - start_radius) * t;
                    Point waypoint(center_x + radius * std::cos(angle), center_y + radius * std::sin(angle),
                                   start.z() + (end.z() - start.z()) * t);
                    if (i == segments) { waypoint = end; }

                    rv += writeMotion("G00", waypoint, move_speed, params, "TRAVEL ARC");
                }
            }
        }

        rv += writeBeginningBead();

        return rv;
    };

    auto writeLinearTravel = [this, &params, &writeBeginningBead](Point end, Velocity move_speed) -> QString {
        QString rv;
        rv += writeMotion("G00", end, move_speed, params, "TRAVEL");
        rv += writeBeginningBead();
        return rv;
    };

    auto liftPoint = [this, &params, cylindrical_mode, lift_height](const Point& point) -> Point {
        if (cylindrical_mode) { return radialLiftedPoint(point, params, lift_height); }

        return point + Point::fromQVector3D(getLiftVector(lift_height));
    };

    Point travel_start = start_location;

    if (travel_lift_required && !m_first_travel &&
        (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        travel_start = liftPoint(start_location);
        layer_rv += writeMotion("G00", travel_start, lift_speed, params, "TRAVEL LIFT");
    }

    Point travel_destination = target_location;
    if (travel_lift_required) { travel_destination = liftPoint(target_location); }

    const bool travel_lower_required =
        travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly);

    if (m_first_travel && !m_startup_kinematics_written) {
        const Point first_travel_destination =
            travel_lower_required
                ? firstTravelPointAboveTravelLowerDestination(travel_destination, target_location, lift_height)
                : travel_destination;
        const Point first_travel_cp_reference =
            isHelicalPathPattern(params) ? target_location : first_travel_destination;
        const Point startup_world_approach = safeStartupWorldApproachPoint(first_travel_destination, params);
        rv += commentLine("INITIAL WORLD APPROACH");
        rv += writeMotion("G00", startup_world_approach, speed, params, kWorldApproachTravelComment,
                          first_travel_cp_reference);
        rv += "#FLUSH WAIT" % m_newline;
        rv += m_newline;
        rv += commentLine("ENABLE WORK-OBJECT KINEMATICS");
        rv += writeStartupKinematics();
        if (!shouldBufferHelicalLayerPrefix(params)) { layer_rv += writePendingLayerChange(); }
        layer_rv += writeMotion("G00", first_travel_destination, speed, params, "TRAVEL", first_travel_cp_reference);
        layer_rv += writeBeginningBead();
    }
    else {
        layer_rv += cylindrical_mode ? writeRadialArcTravel(travel_start, travel_destination, speed)
                                     : writeLinearTravel(travel_destination, speed);
    }

    if (travel_lower_required) {
        layer_rv += writeNumberedBlock("G81" % commentSpaceLine("OPTIONAL STOP ROUTINE"));
        layer_rv += writeMotion("G01", target_location, lift_speed, params, "TRAVEL LOWER");
    }

    m_first_travel = false;
    if (shouldBufferHelicalLayerPrefix(params)) {
        m_pending_layer_prefix += layer_rv;
        return rv;
    }

    return rv + layer_rv;
}

QString ArcSpecialtiesWriter::writeLine(const Point& start_point, const Point& target_point,
                                        const QSharedPointer<SettingsBase> params) {
    QString rv;

    rv += writeStartupKinematics();
    rv += writeHelicalLayerStart(start_point, target_point, params);
    rv += writePendingRegionSchedule();
    startLayerBlockNumbering();

    Velocity speed = params->setting<Velocity>(SS::kSpeed);

    m_layer_start = false;

    if (!m_deposition_active) { rv += writeWelderOn(); }

    if (speed <= 0) { speed = 10.0 * mm / s; }

    rv += writeMotion("G01", target_point, speed, params, printMoveComment(params));
    return rv;
}

QString ArcSpecialtiesWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                                       const Angle&, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    if (!m_sb->setting<bool>(PRS::MachineSetup::kSupportG3)) { return writeLine(start_point, end_point, params); }

    QString rv;
    rv += writeStartupKinematics();
    rv += writeHelicalLayerStart(start_point, end_point, params);
    rv += writePendingRegionSchedule();
    startLayerBlockNumbering();

    if (!m_deposition_active) { rv += writeWelderOn(); }

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    if (speed <= 0) { speed = 10.0 * mm / s; }

    setFeedrate(speed);
    m_layer_start = false;

    const QString inline_optional_stop = m_sb->setting<bool>(PRS::GCode::kArcSpecialtiesG2G3OptionalStop) ? " G81" : "";

    const QString print_comment = printMoveComment(params);
    rv += writeNumberedBlock(QString(ccw ? "G03" : "G02") %
                             writeCoordinates(end_point, params, toolFrameRotationForMotion(print_comment, params)) %
                             writeArcCenterParameters(start_point, center_point) % writeMotionFeedrate(speed) %
                             inline_optional_stop % commentSpaceLine(print_comment));
    return rv;
}

QString ArcSpecialtiesWriter::writeAfterPath(RegionType type) {
    QString rv;
    if (!m_spiral_layer) {
        if (type == RegionType::kPerimeter) {
            if (!m_sb->setting<QString>(PS::GCode::kPerimeterEnd).isEmpty()) {
                rv += m_sb->setting<QString>(PS::GCode::kPerimeterEnd) % m_newline;
            }
        }
        else if (type == RegionType::kInset) {
            if (!m_sb->setting<QString>(PS::GCode::kInsetEnd).isEmpty()) {
                rv += m_sb->setting<QString>(PS::GCode::kInsetEnd) % m_newline;
            }
        }
        else if (type == RegionType::kSkeleton) {
            if (!m_sb->setting<QString>(PS::GCode::kSkeletonEnd).isEmpty()) {
                rv += m_sb->setting<QString>(PS::GCode::kSkeletonEnd) % m_newline;
            }
        }
        else if (type == RegionType::kSkin) {
            if (!m_sb->setting<QString>(PS::GCode::kSkinEnd).isEmpty()) {
                rv += m_sb->setting<QString>(PS::GCode::kSkinEnd) % m_newline;
            }
        }
        else if (type == RegionType::kInfill) {
            if (!m_sb->setting<QString>(PS::GCode::kInfillEnd).isEmpty()) {
                rv += m_sb->setting<QString>(PS::GCode::kInfillEnd) % m_newline;
            }
        }
        else if (type == RegionType::kSupport) {
            if (!m_sb->setting<QString>(PS::GCode::kSupportEnd).isEmpty()) {
                rv += m_sb->setting<QString>(PS::GCode::kSupportEnd) % m_newline;
            }
        }
    }
    return writeNumberedBlock(rv);
}

QString ArcSpecialtiesWriter::writeAfterRegion(RegionType type) {
    return QString();
}

QString ArcSpecialtiesWriter::writeAfterIsland() {
    return QString();
}

QString ArcSpecialtiesWriter::writeAfterPart() {
    return QString();
}

QString ArcSpecialtiesWriter::writeAfterLayer() {
    QString layer_code = m_sb->setting<QString>(PRS::GCode::kLayerCodeChange);
    stopLayerBlockNumbering();
    QString rv;
    rv += writePendingLayerChange();
    rv += writePendingLayerPrefix();
    m_pending_region_schedule.clear();
    return layer_code.isEmpty() ? rv : rv % layer_code % m_newline;
}

QString ArcSpecialtiesWriter::writeShutdown() {
    stopLayerBlockNumbering();
    QString rv;
    rv += writePendingLayerChange();
    rv += writePendingLayerPrefix();
    m_pending_region_schedule.clear();

    rv += writeWelderOff();
    if (!m_sb->setting<QString>(PRS::GCode::kEndCode).isEmpty()) {
        rv += m_sb->setting<QString>(PRS::GCode::kEndCode) % m_newline;
    }
    if (m_startup_kinematics_written && m_absolute_arc_center_mode_enabled) { rv += "G164" % m_newline; }
    rv += "M49" % commentSpaceLine("ROBOT GO HOME");
    rv += "#CHANNEL INIT [CMDPOS]" % m_newline;
    rv += "M02" % commentSpaceLine("PROGRAM END");
    return rv;
}

QString ArcSpecialtiesWriter::writeDwell(Time time) {
    if (time > 0) {
        return writeNumberedBlock(m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) %
                                  commentSpaceLine("DWELL"));
    }
    return QString();
}

QString ArcSpecialtiesWriter::writeWelderOn() {
    if (!m_deposition_active) {
        QString rv;
        rv += "G82" % commentSpaceLine("WIRE ARC WELDER ON");
        rv += "G261" % commentSpaceLine("BLENDING ON");
        m_deposition_active = true;
        return writeNumberedBlock(rv);
    }
    else { return QString(); }
}

QString ArcSpecialtiesWriter::writeWelderOff(int mode) {
    if (m_deposition_active) {
        QString rv;
        const int g83_mode = validatedG83Mode(mode);
        rv += "G260" % commentSpaceLine("BLENDING OFF");
        rv += "G83 [" % QString::number(g83_mode) % "]" % commentSpaceLine("WIRE ARC WELDER OFF");
        m_deposition_active = false;
        return writeNumberedBlock(rv);
    }
    else { return QString(); }
}

QString ArcSpecialtiesWriter::g80WeldScheduleFile() const {
    QString g80_schedule_file = m_sb->contains(PRS::GCode::kArcSpecialtiesG80WeldScheduleFile)
                                    ? m_sb->setting<QString>(PRS::GCode::kArcSpecialtiesG80WeldScheduleFile)
                                    : kDefaultG80WeldScheduleFile;
    if (g80_schedule_file.isEmpty()) { g80_schedule_file = kDefaultG80WeldScheduleFile; }
    g80_schedule_file.remove('"');
    return g80_schedule_file;
}

bool ArcSpecialtiesWriter::usesG80ScheduleSpeedVariable() const {
    return !g80WeldScheduleFile().isEmpty();
}

QString ArcSpecialtiesWriter::writeMotionFeedrate(Velocity speed) const {
    if (usesG80ScheduleSpeedVariable()) { return m_f % kG80ScheduleSpeedVariable; }

    return m_f % QString::number(speed.to(m_meta.m_velocity_unit), 'f', 4);
}

QString ArcSpecialtiesWriter::writeMotion(const QString& command, const Point& destination, Velocity speed,
                                          const QSharedPointer<SettingsBase>& params, const QString& comment) {
    return writeMotion(command, destination, speed, params, comment, destination);
}

QString ArcSpecialtiesWriter::writeMotion(const QString& command, const Point& destination, Velocity speed,
                                          const QSharedPointer<SettingsBase>& params, const QString& comment,
                                          const Point& cp_reference) {
    setFeedrate(speed);
    const ToolFrameRotation tool_frame_rotation = toolFrameRotationForMotion(comment, params);
    if (command == "G00") {
        return writeNumberedBlock(command % writeCoordinates(destination, params, tool_frame_rotation, cp_reference) %
                                  commentSpaceLine(comment));
    }
    else {
        return writeNumberedBlock(command % writeCoordinates(destination, params, tool_frame_rotation, cp_reference) %
                                  writeMotionFeedrate(speed) % commentSpaceLine(comment));
    }
}

QString ArcSpecialtiesWriter::writeStartupKinematics() {
    if (m_startup_kinematics_written) { return QString(); }

    QString rv;
    rv += "#KIN ID [9]" % m_newline;
    rv += "#FLUSH WAIT" % m_newline;
    rv += "" % m_newline;
    rv += "V.G.KIN[9].PROGRAMMING_MODE            = -1" % m_newline;
    rv += "V.G.KIN[9].RTCP                        = 0" % m_newline;
    rv += "#ORI MODE [ANGLE]" % m_newline;
    rv += "V.G.WZ_AKT.L = 0" % m_newline;
    rv += "M01" % m_newline;
    rv += "#FLUSH WAIT" % m_newline;
    const bool enable_trafo =
        !m_sb->contains(PRS::MachineSetup::kEnableTrafo) || m_sb->setting<bool>(PRS::MachineSetup::kEnableTrafo);
    rv += QString(enable_trafo ? "#TRAFO ON" : "#TRAFO OFF") % m_newline;
    rv += "#FLUSH WAIT" % m_newline;
    rv += "#CHANNEL INIT [CMDPOS]" % m_newline;
    rv += "#FLUSH WAIT" % m_newline;
    rv += QString(m_absolute_arc_center_mode_enabled ? "G161" : "G162") % m_newline;

    m_startup_kinematics_written = true;
    return rv;
}

QString ArcSpecialtiesWriter::writePendingLayerChange() {
    if (m_pending_layer_change.isEmpty()) { return QString(); }

    m_pending_layer_change.prepend(m_newline);
    QString rv = m_pending_layer_change;
    m_pending_layer_change.clear();
    return rv;
}

QString ArcSpecialtiesWriter::writePendingLayerPrefix() {
    if (m_pending_layer_prefix.isEmpty()) { return QString(); }

    const QString rv = m_pending_layer_prefix;
    m_pending_layer_prefix.clear();
    return rv;
}

QString ArcSpecialtiesWriter::writePendingRegionSchedule() {
    if (m_pending_region_schedule.isEmpty()) { return QString(); }

    const QString rv = writeNumberedBlock(m_pending_region_schedule, true);
    m_pending_region_schedule.clear();
    return rv;
}

QString ArcSpecialtiesWriter::writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params,
                                               const ToolFrameRotation& tool_frame_rotation) {
    return writeCoordinates(destination, params, tool_frame_rotation, destination);
}

QString ArcSpecialtiesWriter::writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params,
                                               const ToolFrameRotation& tool_frame_rotation,
                                               const Point& cp_reference) {
    const double ap_output         = m_sb->setting<Angle>(PRS::MachineSetup::kAxisA).to(m_meta.m_angle_unit);
    const double cp_output         = Angle(cpAxisForPoint(cp_reference, params) * degree).to(m_meta.m_angle_unit);
    const Point output_destination = rotateGCodeCoordinateFramePoint(destination);

    return QString(" X=") % QString::number(Distance(output_destination.x()).to(m_meta.m_distance_unit), 'f', 4) %
           " Y=" % QString::number(Distance(output_destination.y()).to(m_meta.m_distance_unit), 'f', 4) % " Z=" %
           QString::number(Distance(output_destination.z()).to(m_meta.m_distance_unit), 'f', 4) % " XR=" %
           QString::number(tool_frame_rotation.xr, 'f', 4) % " YR=" % QString::number(tool_frame_rotation.yr, 'f', 4) %
           " ZR=" % QString::number(tool_frame_rotation.zr, 'f', 4) % " AP=" % QString::number(ap_output, 'f', 4) %
           " CP=" % QString::number(cp_output, 'f', 4);
}

ArcSpecialtiesWriter::ToolFrameRotation ArcSpecialtiesWriter::toolFrameRotationForMotion(
    const QString& comment, const QSharedPointer<SettingsBase>& params) const {
    auto settingAngleOrDefault = [this, &params](const QString& key, double fallback) {
        if (params != nullptr && params->contains(key)) { return params->setting<Angle>(key).to(m_meta.m_angle_unit); }
        if (m_sb != nullptr && m_sb->contains(key)) { return m_sb->setting<Angle>(key).to(m_meta.m_angle_unit); }

        return fallback;
    };

    auto helicalToolFrameRotation = [&settingAngleOrDefault](const QString& x_key, const QString& y_key,
                                                             const QString& z_key) {
        return ToolFrameRotation {settingAngleOrDefault(x_key, kToolFrameXR),
                                  settingAngleOrDefault(y_key, kToolFrameYR),
                                  settingAngleOrDefault(z_key, kToolFrameZR)};
    };

    if (comment == kWorldApproachTravelComment) {
        return ToolFrameRotation {kToolFrameXR, kToolFrameYR, kRapidTravelToolFrameZR};
    }

    if (!isHelicalPathPattern()) { return ToolFrameRotation {kToolFrameXR, kToolFrameYR, kToolFrameZR}; }

    if (comment.startsWith("TRAVEL")) {
        return helicalToolFrameRotation(PS::Helical::kHelicalTravelToolFrameXRotation,
                                        PS::Helical::kHelicalTravelToolFrameYRotation,
                                        PS::Helical::kHelicalTravelToolFrameZRotation);
    }

    if (comment == Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kPerimeter) {
        return helicalToolFrameRotation(PS::Helical::kHelicalPerimeterToolFrameXRotation,
                                        PS::Helical::kHelicalPerimeterToolFrameYRotation,
                                        PS::Helical::kHelicalPerimeterToolFrameZRotation);
    }
    if (comment == Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kInset) {
        return helicalToolFrameRotation(PS::Helical::kHelicalInsetToolFrameXRotation,
                                        PS::Helical::kHelicalInsetToolFrameYRotation,
                                        PS::Helical::kHelicalInsetToolFrameZRotation);
    }
    if (comment == Constants::RegionTypeStrings::kHelical % " " % Constants::RegionTypeStrings::kInfill) {
        return helicalToolFrameRotation(PS::Helical::kHelicalInfillToolFrameXRotation,
                                        PS::Helical::kHelicalInfillToolFrameYRotation,
                                        PS::Helical::kHelicalInfillToolFrameZRotation);
    }

    if (params != nullptr && params->contains(SS::kRegionType)) {
        switch (params->setting<RegionType>(SS::kRegionType)) {
            case RegionType::kPerimeter:
                return helicalToolFrameRotation(PS::Helical::kHelicalPerimeterToolFrameXRotation,
                                                PS::Helical::kHelicalPerimeterToolFrameYRotation,
                                                PS::Helical::kHelicalPerimeterToolFrameZRotation);
            case RegionType::kInset:
                return helicalToolFrameRotation(PS::Helical::kHelicalInsetToolFrameXRotation,
                                                PS::Helical::kHelicalInsetToolFrameYRotation,
                                                PS::Helical::kHelicalInsetToolFrameZRotation);
            case RegionType::kInfill:
                return helicalToolFrameRotation(PS::Helical::kHelicalInfillToolFrameXRotation,
                                                PS::Helical::kHelicalInfillToolFrameYRotation,
                                                PS::Helical::kHelicalInfillToolFrameZRotation);
            default:
                break;
        }
    }

    return ToolFrameRotation {kToolFrameXR, kToolFrameYR, kToolFrameZR};
}

Point ArcSpecialtiesWriter::firstTravelPointAboveTravelLowerDestination(const Point& travel_destination,
                                                                        const Point& travel_lower_destination,
                                                                        Distance travel_lift_height) const {
    const Point lower_output_destination  = rotateGCodeCoordinateFramePoint(travel_lower_destination);
    Point first_travel_output_destination = rotateGCodeCoordinateFramePoint(travel_destination);
    first_travel_output_destination.y(lower_output_destination.y());
    first_travel_output_destination.z(Distance(lower_output_destination.z()) + travel_lift_height);

    return inverseRotateGCodeCoordinateFramePoint(first_travel_output_destination);
}

Point ArcSpecialtiesWriter::safeStartupWorldApproachPoint(const Point& travel_destination,
                                                          const QSharedPointer<SettingsBase>& params) const {
    Point approach = travel_destination;
    if (isCylindricalSlicingMode()) {
        approach.x(params->setting<Distance>(kRadialCenterX));
        approach.y(params->setting<Distance>(kRadialCenterY));
    }

    Distance safe_z(travel_destination.z());
    bool apply_startup_buffer = false;
    if (hasBuildMaximumZ()) {
        safe_z               = getBuildMaximumZ();
        apply_startup_buffer = true;
    }

    if (isCylindricalSlicingMode()) {
        const Distance cylinder_height = m_sb->setting<Distance>(PS::Slicing::kCylinderHeight);
        if (cylinder_height > safe_z) { safe_z = cylinder_height; }
        apply_startup_buffer = apply_startup_buffer || cylinder_height > 0;
    }

    if (apply_startup_buffer) { safe_z += kStartupWorldApproachZBuffer; }
    approach.z(static_cast<float>(safe_z()));

    return approach;
}

QString ArcSpecialtiesWriter::writeArcCenterParameters(const Point& start_point, const Point& center_point) {
    if (usesAbsoluteArcCenters()) {
        return QString(" I=") %
               QString::number(m_sb->setting<Distance>(PRS::MachineSetup::kG2G3AbsoluteI).to(m_meta.m_distance_unit),
                               'f', 4) %
               " J=" %
               QString::number(m_sb->setting<Distance>(PRS::MachineSetup::kG2G3AbsoluteJ).to(m_meta.m_distance_unit),
                               'f', 4);
    }

    const Point center_offset(center_point.x() - start_point.x(), center_point.y() - start_point.y(),
                              center_point.z() - start_point.z());
    const Point output_offset = rotateGCodeCoordinateFrameDelta(center_offset);

    return QString(" I=") % QString::number(Distance(output_offset.x()).to(m_meta.m_distance_unit), 'f', 4) % " J=" %
           QString::number(Distance(output_offset.y()).to(m_meta.m_distance_unit), 'f', 4);
}

bool ArcSpecialtiesWriter::usesAbsoluteArcCenters() const {
    return m_sb->setting<int>(PRS::MachineSetup::kG2G3CenterPointInterpretation) != kRelativeArcCenterMode;
}

double ArcSpecialtiesWriter::cpAxisForPoint(const Point& destination, const QSharedPointer<SettingsBase>& params) {
    if (!isCylindricalSlicingMode()) {
        return normalizeDegrees(m_sb->setting<Angle>(PRS::MachineSetup::kAxisC).to(degree));
    }

    const double center_x               = params->setting<Distance>(kRadialCenterX)();
    const double center_y               = params->setting<Distance>(kRadialCenterY)();
    const Point transformed_destination = rotateGCodeCoordinateFramePoint(destination);
    const Point transformed_center      = rotateGCodeCoordinateFramePoint(Point(center_x, center_y, destination.z()));
    double cp_degrees                   = std::atan2(transformed_destination.y() - transformed_center.y(),
                                                     transformed_destination.x() - transformed_center.x()) *
                                          180.0 / M_PI;

    if (isHelicalPathPattern()) {
        const HelicalPathHandedness handedness =
            static_cast<HelicalPathHandedness>(params->setting<int>(PS::Helical::kHelicalPathHandedness));
        const double start_angle = helicalStartAngle(params);
        const double sweep_degrees =
            handedness == HelicalPathHandedness::kLeftHanded ? start_angle - cp_degrees : cp_degrees - start_angle;
        cp_degrees = params->setting<Angle>(PS::Helical::kHelicalToolStartAngleOffset).to(degree) + sweep_degrees +
                     m_sb->setting<Angle>(PRS::MachineSetup::kAxisC).to(degree);
        return cp_degrees;
    }

    cp_degrees += m_sb->setting<Angle>(PRS::MachineSetup::kAxisC).to(degree);

    return normalizeDegrees(cp_degrees);
}

double ArcSpecialtiesWriter::helicalStartAngle(const QSharedPointer<SettingsBase>& params) const {
    Q_UNUSED(params)
    const Angle start_angle = 90.0 * degree;
    const Point start_direction(std::cos(start_angle()), std::sin(start_angle()), 0.0);
    const Point transformed_start_direction = rotateGCodeCoordinateFrameDelta(start_direction);
    if (std::hypot(transformed_start_direction.x(), transformed_start_direction.y()) <=
        std::numeric_limits<double>::epsilon()) {
        return 90.0;
    }

    return std::atan2(transformed_start_direction.y(), transformed_start_direction.x()) * 180.0 / M_PI;
}

bool ArcSpecialtiesWriter::isHelicalPathPattern() const {
    return isHelicalPathPattern(m_sb);
}

bool ArcSpecialtiesWriter::isHelicalPathPattern(const QSharedPointer<SettingsBase>& params) const {
    auto settingOrDefault = [this, &params](const QString& key, int fallback) {
        if (params != nullptr && params->contains(key)) { return params->setting<int>(key); }
        if (m_sb != nullptr && m_sb->contains(key)) { return m_sb->setting<int>(key); }

        return fallback;
    };

    const CylindricalPathPattern path_pattern = static_cast<CylindricalPathPattern>(
        settingOrDefault(PS::Slicing::kCylindricalPathPattern, static_cast<int>(CylindricalPathPattern::kRadial)));
    const SlicingMode slicing_mode =
        static_cast<SlicingMode>(settingOrDefault(PS::Slicing::kSlicingMode, static_cast<int>(SlicingMode::kPlanar)));
    return slicing_mode == SlicingMode::kCylindrical && path_pattern == CylindricalPathPattern::kHelical;
}

bool ArcSpecialtiesWriter::shouldBufferHelicalLayerPrefix(const QSharedPointer<SettingsBase>& params) const {
    return !m_helical_opt_stop_mode_written && !m_pending_layer_change.isEmpty() && isHelicalPathPattern(params);
}

QString ArcSpecialtiesWriter::printMoveComment(const QSharedPointer<SettingsBase>& params) const {
    if (isCylindricalSlicingMode()) {
        if (!isHelicalPathPattern()) { return Constants::RegionTypeStrings::kRadial; }

        if (params == nullptr || !params->contains(SS::kRegionType)) { return Constants::RegionTypeStrings::kHelical; }

        return helicalRegionComment(params->setting<RegionType>(SS::kRegionType));
    }

    PathModifiers path_modifiers = PathModifiers::kNone;
    if (params != nullptr && params->contains(SS::kPathModifiers)) {
        path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    }

    return regionComment(m_region_type, path_modifiers, params);
}

bool ArcSpecialtiesWriter::isCylindricalSlicingMode() const {
    const SlicingMode slicing_mode = static_cast<SlicingMode>(m_sb->setting<int>(PS::Slicing::kSlicingMode));
    return slicing_mode == SlicingMode::kCylindrical;
}

QString ArcSpecialtiesWriter::writeHelicalOptStopMode(const Point& start_point, const Point& end_point,
                                                      const QSharedPointer<SettingsBase>& params) {
    if (m_helical_opt_stop_mode_written || !isHelicalPathPattern(params)) { return QString(); }

    const double start_cp                  = cpAxisForPoint(start_point, params) * M_PI / 180.0;
    const double end_cp                    = cpAxisForPoint(end_point, params) * M_PI / 180.0;
    const bool positive_rotation_direction = shortestAngularDelta(start_cp, end_cp) >= 0.0;
    const int opt_stop_mode                = positive_rotation_direction ? 1 : 2;
    m_helical_opt_stop_mode_written        = true;
    const QString opt_stop_mode_line       = "V.E.OptStopMode = " % QString::number(opt_stop_mode) % m_newline;
    if (!m_startup_kinematics_written) {
        m_pending_layer_change += opt_stop_mode_line;
        return QString();
    }

    return opt_stop_mode_line;
}

QString ArcSpecialtiesWriter::writeHelicalLayerStart(const Point& start_point, const Point& end_point,
                                                     const QSharedPointer<SettingsBase>& params) {
    QString rv;
    rv += writePendingLayerChange();
    rv += writeHelicalOptStopMode(start_point, end_point, params);
    rv += writePendingLayerPrefix();
    return rv;
}
}  // namespace ORNL
