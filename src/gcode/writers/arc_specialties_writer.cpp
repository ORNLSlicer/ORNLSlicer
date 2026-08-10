#include "gcode/writers/arc_specialties_writer.h"

#include <math.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <QStringBuilder>
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

//! @brief Cylindrical comment field used by the preview to recover the cylinder axis X.
const QString kCylindricalAxisXComment = "AXIS_X=";

//! @brief Cylindrical comment field used by the preview to recover the cylinder axis Y.
const QString kCylindricalAxisYComment = "AXIS_Y=";

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
    const double dx = point.x() - center_x;
    const double dy = point.y() - center_y;
    const double length = std::hypot(dx, dy);

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
    while (delta > M_PI) {
        delta -= 2.0 * M_PI;
    }
    while (delta < -M_PI) {
        delta += 2.0 * M_PI;
    }
    return delta;
}

//! @brief Normalizes an angle in degrees to [0, 360).
double normalizeDegrees(double degrees) {
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0) {
        degrees += 360.0;
    }

    return degrees;
}

//! @brief Smallest travel arc segment used to keep arc-like travel output bounded.
const Distance kMinTravelArcSegmentLength = 100.0 * micron;

//! @brief Machine setup setting value for relative G02/G03 center interpretation.
constexpr int kRelativeArcCenterMode = 1;

//! @brief Default G83 mode: stop welding only.
constexpr int kDefaultG83Mode = 0;

//! @brief Highest supported Arc Specialties G83 mode.
constexpr int kMaxG83Mode = 4;

//! @brief Returns a supported G83 mode, falling back to weld-stop-only behavior for invalid input.
int validatedG83Mode(int mode) { return mode >= kDefaultG83Mode && mode <= kMaxG83Mode ? mode : kDefaultG83Mode; }

//! @brief Formats a distance using the output unit declared by the active gcode metadata.
QString formatDistance(Distance value, Distance unit) {
    return QString::number(value.to(unit), 'f', 4) % unit.toString();
}

//! @brief Formats an angle using the output unit declared by the active gcode metadata.
QString formatAngle(Angle value, Angle unit) { return QString::number(value.to(unit), 'f', 4) % unit.toString(); }
} // namespace

ArcSpecialtiesWriter::ArcSpecialtiesWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb)
    : WriterBase(meta, sb) {}

void ArcSpecialtiesWriter::setHelicalPathBoundaryPolicy(
    const QVector<QPair<QString, HelicalPathBoundaryPolicy>>& methods) {
    m_helical_path_boundary_policy = methods;
}

void ArcSpecialtiesWriter::setHelicalPathHandedness(const QVector<QPair<QString, HelicalPathHandedness>>& handedness) {
    m_helical_path_handedness = handedness;
}

QString ArcSpecialtiesWriter::writeSettingsHeader(GcodeSyntax) {
    QString text;
    const SlicingMode slicing_mode = static_cast<SlicingMode>(m_sb->setting<int>(PS::Slicing::kSlicingMode));
    const CylindricalPathPattern path_pattern =
        static_cast<CylindricalPathPattern>(m_sb->setting<int>(PS::Slicing::kCylindricalPathPattern));
    const bool cylindrical_mode = slicing_mode == SlicingMode::kCylindrical;
    const bool helical_mode = path_pattern == CylindricalPathPattern::kHelical;
    if (cylindrical_mode) {
        text += commentLine(helical_mode ? "Arc Specialties Helical Slicing Parameters"
                                         : "Arc Specialties Radial Slicing Parameters");
        text += commentLine("Cylindrical Path Pattern: " % toString(path_pattern));
        text += commentLine("Motion Coordinates: X/Y/Z are user-frame endpoint coordinates relative to the active work "
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
        text += commentLine(QString("Tool Frame Rotation: XR=") % QString::number(kToolFrameXR, 'f', 4) % "deg YR=" %
                            QString::number(kToolFrameYR, 'f', 4) % "deg ZR=" % QString::number(kToolFrameZR, 'f', 4) %
                            "deg");
        text += commentLine(QString("Rapid Travel Tool Frame Rotation: XR=") % QString::number(kToolFrameXR, 'f', 4) %
                            "deg YR=" % QString::number(kToolFrameYR, 'f', 4) % "deg ZR=" %
                            QString::number(kRapidTravelToolFrameZR, 'f', 4) % "deg");
        text += commentLine(
            "Initial Approach: TRAFO-off world approach uses cylinder center XY and part maximum Z plus " %
            formatDistance(kStartupWorldApproachZBuffer, m_meta.m_distance_unit) % " before work-object kinematics");
        text += commentLine(
            QString("Cylinder Inner Radius: ") %
            formatDistance(m_sb->setting<Distance>(PS::Slicing::kCylinderInnerRadius), m_meta.m_distance_unit));
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
                "Helical Path Start Angle: " %
                formatAngle(m_sb->setting<Angle>(PS::Slicing::kHelicalPathStartAngle), m_meta.m_angle_unit));
            text += commentLine("Helical Rise Per Revolution: " % formatDistance(bead_width, m_meta.m_distance_unit));
            text += commentLine("Helical Rise Per Radian: " %
                                formatDistance(bead_width / (2.0 * M_PI), m_meta.m_distance_unit));
        }
        else {
            text +=
                commentLine("Radial Path Start Angle: " %
                            formatAngle(m_sb->setting<Angle>(PS::Slicing::kRadialPathStartAngle), m_meta.m_angle_unit));
            text += commentLine("Vertical Bead Spacing: " % formatDistance(bead_width, m_meta.m_distance_unit));
        }
        if (helical_mode) {
            if (m_helical_path_boundary_policy.size() == 1) {
                text += commentLine("Helical Path Boundary Policy: " %
                                    toString(m_helical_path_boundary_policy.first().second));
            }
            else if (m_helical_path_boundary_policy.size() > 1) {
                for (const QPair<QString, HelicalPathBoundaryPolicy>& part_method : m_helical_path_boundary_policy) {
                    const QString part_name = part_method.first.isEmpty() ? "Unnamed Part" : part_method.first;
                    text += commentLine("Helical Path Boundary Policy (" % part_name % "): " %
                                        toString(part_method.second));
                }
            }
            else {
                text += commentLine("Helical Path Boundary Policy: " %
                                    toString(static_cast<HelicalPathBoundaryPolicy>(
                                        m_sb->setting<int>(PS::Slicing::kHelicalPathBoundaryPolicy))));
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
                                        m_sb->setting<int>(PS::Slicing::kHelicalPathHandedness))));
            }
        }
        else {
            text += commentLine("Radial Path Boundary Policy: " %
                                toString(static_cast<RadialPathBoundaryPolicy>(
                                    m_sb->setting<int>(PS::Slicing::kRadialPathBoundaryPolicy))));
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
        text += commentLine("Motion Coordinates: X/Y/Z are user-frame endpoint coordinates relative to the active work "
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
        text += commentLine(QString("Rapid Travel Tool Frame Rotation: XR=") % QString::number(kToolFrameXR, 'f', 4) %
                            "deg YR=" % QString::number(kToolFrameYR, 'f', 4) % "deg ZR=" %
                            QString::number(kRapidTravelToolFrameZR, 'f', 4) % "deg");
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
    m_first_travel = true;
    m_layer_start = true;
    m_absolute_arc_center_mode_enabled = usesAbsoluteArcCenters();
    m_startup_kinematics_written = false;
    m_pending_layer_change.clear();
    setFeedrate(0.0);

    QString rv;

    rv += "V.E.Sch.Preflow = 3  ;Preflow Time in Seconds" % m_newline;
    rv += "V.E.Sch.TravelDelay = 0  ;Travel Start Delay in Seconds" % m_newline;
    rv += "V.E.Sch.Postflow = 10   ;Stationary Postflow Time" % m_newline;
    rv += "V.E.Sch.PostPurge = 0   ;Postflow Time after moving away (can reduce preflow delay)" % m_newline;
    rv += "V.E.Sch.Weld.Program = 4   ;Program/Mode in Main Weld" % m_newline;
    rv += "V.E.Sch.Weld.WFS = 250   ;IPM Wire Feed Speed in Main Weld" % m_newline;
    rv += "V.E.Sch.Weld.Volts = 65   ;Trim or Volts in Main Weld" % m_newline;
    rv += "V.E.Sch.Weld.Control = 25   ;Arc Control in Main Weld" % m_newline;
    rv += "V.E.Sch.Crater.Program = 4   ;Program/Mode in Crater Fill" % m_newline;
    rv += "V.E.Sch.Crater.WFS = 250   ;IPM Wire Feed Speed in Crater Fill" % m_newline;
    rv += "V.E.Sch.Crater.Volts = 70   ;Trim or Volts in Crater Fill" % m_newline;
    rv += "V.E.Sch.Crater.Control = 25   ;Arc Control in Crater Fill" % m_newline;
    rv += "V.E.Sch.Crater.Time = .5   ;Crater Fill Time in Seconds" % m_newline;
    rv += "#CONTOUR MODE [DEV PATH_DEV=2 CONST_VEL=1]" % m_newline;
    /// TODO: M06 command does not work as of 2026-08-07 and is disabled for now. Must be re-enabled when the M06
    /// command is fixed in the Arc Specialties controller or be replaced with a different command that achieves the
    /// same effect.
    rv += ";M06 T1   ;Select Tool 1" % m_newline;
    rv += "M49 ;Send Robot Home" % m_newline;
    rv += "#CHANNEL INIT [CMDPOS]" % m_newline;
    rv += "" % m_newline;
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
    if (!m_startup_kinematics_written) {
        m_pending_layer_change += layer_change;
        return QString();
    }

    return layer_change;
}

QString ArcSpecialtiesWriter::writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) {
    QString rv;
    m_layer_start = true;
    m_current_bead = 1;
    m_current_layer++;
    m_first_deposition = true;
    return rv;
}

QString ArcSpecialtiesWriter::writeBeforePart(QVector3D normal) { return QString(); }

QString ArcSpecialtiesWriter::writeBeforeIsland() { return QString(); }

QString ArcSpecialtiesWriter::writeBeforeRegion(RegionType type, int pathSize) { return QString(); }

QString ArcSpecialtiesWriter::writeBeforePath(RegionType type) {
    m_region_type = type;
    return QString();
}

QString ArcSpecialtiesWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                          QSharedPointer<SettingsBase> params) {
    QString rv;
    Velocity speed = params->setting<Velocity>(PS::Travel::kSpeed);
    if (speed <= 0) {
        speed = params->setting<Velocity>(PRS::MachineSpeed::kMaxXYSpeed);
    }

    Velocity lift_speed = m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed);
    if (lift_speed <= 0) {
        lift_speed = speed;
    }

    // Determine if travel length is short enough to keep welder on
    Distance travel_distance = start_location.distance(target_location);
    if (m_deposition_active && travel_distance > m_sb->setting<Distance>(PS::Travel::kMinTravelLength)) {
        rv += writeWelderOff();
    }
    else if (!m_first_travel && travel_distance < m_sb->setting<Distance>(PS::Travel::kMinTravelLength)) {
        rv += writeWelderOn();
    }

    const Distance lift_height = m_sb->setting<Distance>(PS::Travel::kLiftHeight);
    bool travel_lift_required = lift_height > 0 && lType != TravelLiftType::kNoLift;
    if (start_location.distance(target_location) < m_sb->setting<Distance>(PS::Travel::kMinTravelForLift)) {
        travel_lift_required = false;
    }

    const bool cylindrical_mode = isCylindricalSlicingMode();

    auto writeBeginningBead = [this]() -> QString {
        QString rv;
        rv += commentLine(QString("BEGINNING BEAD: ") % QString::number(m_current_layer) % "." %
                          QString::number(m_current_bead));
        m_current_bead++;
        return rv;
    };

    auto writeRadialArcTravel = [this, &params, &writeBeginningBead](Point start, Point end,
                                                                     Velocity move_speed) -> QString {
        QString rv;
        const double center_x = params->setting<Distance>(kRadialCenterX)();
        const double center_y = params->setting<Distance>(kRadialCenterY)();
        const double start_radius = radialDistance(start, params);
        const double end_radius = radialDistance(end, params);

        if (start_radius <= std::numeric_limits<double>::epsilon() ||
            end_radius <= std::numeric_limits<double>::epsilon()) {
            rv += writeMotion("G00", end, move_speed, params, "TRAVEL");
        }
        else {
            const double start_angle = std::atan2(start.y() - center_y, start.x() - center_x);
            const double end_angle = std::atan2(end.y() - center_y, end.x() - center_x);
            const double delta_angle = shortestAngularDelta(start_angle, end_angle);
            const double max_radius = std::max(start_radius, end_radius);
            const double arc_length = std::abs(delta_angle) * max_radius;
            const double target_segment_length =
                std::max(params->setting<Distance>(SS::kWidth)() / 2.0, kMinTravelArcSegmentLength());

            if (arc_length <= target_segment_length) {
                rv += writeMotion("G00", end, move_speed, params, "TRAVEL");
            }
            else {
                const int segments =
                    std::clamp(static_cast<int>(std::ceil(arc_length / target_segment_length)), 2, 180);

                for (int i = 1; i <= segments; ++i) {
                    const double t = static_cast<double>(i) / static_cast<double>(segments);
                    const double angle = start_angle + delta_angle * t;
                    const double radius = start_radius + (end_radius - start_radius) * t;
                    Point waypoint(center_x + radius * std::cos(angle), center_y + radius * std::sin(angle),
                                   start.z() + (end.z() - start.z()) * t);
                    if (i == segments) {
                        waypoint = end;
                    }

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
        if (cylindrical_mode) {
            return radialLiftedPoint(point, params, lift_height);
        }

        return point + Point::fromQVector3D(getLiftVector(lift_height));
    };

    Point travel_start = start_location;

    if (travel_lift_required && !m_first_travel &&
        (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        travel_start = liftPoint(start_location);
        rv += writeMotion("G00", travel_start, lift_speed, params, "TRAVEL LIFT");
    }

    Point travel_destination = target_location;
    if (travel_lift_required) {
        travel_destination = liftPoint(target_location);
    }

    const bool travel_lower_required =
        travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly);

    if (m_first_travel && !m_startup_kinematics_written) {
        const Point first_travel_destination =
            travel_lower_required
                ? firstTravelPointAboveTravelLowerDestination(travel_destination, target_location, lift_height)
                : travel_destination;
        const Point startup_world_approach = safeStartupWorldApproachPoint(first_travel_destination, params);
        rv += commentLine("INITIAL WORLD APPROACH");
        rv += writeMotion("G00", startup_world_approach, speed, params, "WORLD APPROACH TRAVEL",
                          first_travel_destination);
        rv += "#FLUSH WAIT" % m_newline;
        rv += m_newline;
        rv += commentLine("ENABLE WORK-OBJECT KINEMATICS");
        rv += writeStartupKinematics();
        rv += writePendingLayerChange();
        rv += writeMotion("G00", first_travel_destination, speed, params, "TRAVEL");
        rv += writeBeginningBead();
    }
    else {
        rv += cylindrical_mode ? writeRadialArcTravel(travel_start, travel_destination, speed)
                               : writeLinearTravel(travel_destination, speed);
    }

    if (travel_lower_required) {
        rv += "G81" % commentSpaceLine("OPTIONAL STOP ROUTINE");
        rv += writeMotion("G01", target_location, lift_speed, params, "TRAVEL LOWER");
    }

    m_first_travel = false;
    return rv;
}

QString ArcSpecialtiesWriter::writeLine(const Point&, const Point& target_point,
                                        const QSharedPointer<SettingsBase> params) {
    QString rv;

    rv += writeStartupKinematics();
    rv += writePendingLayerChange();

    Velocity speed = params->setting<Velocity>(SS::kSpeed);

    m_layer_start = false;

    if (!m_deposition_active) {
        rv += writeWelderOn();
    }

    if (speed <= 0) {
        speed = 10.0 * mm / s;
    }

    rv += writeMotion("G01", target_point, speed, params, printMoveComment(params));
    m_first_deposition = false;
    return rv;
}

QString ArcSpecialtiesWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                                       const Angle&, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    if (!m_sb->setting<bool>(PRS::MachineSetup::kSupportG3)) {
        return writeLine(start_point, end_point, params);
    }

    QString rv;
    rv += writeStartupKinematics();
    rv += writePendingLayerChange();

    if (!m_deposition_active) {
        rv += writeWelderOn();
    }

    if (m_sb->setting<bool>(PRS::GCode::kArcSpecialtiesG2G3OptionalStop) && !m_first_deposition) {
        rv += "G81" % commentSpaceLine("OPTIONAL STOP ROUTINE");
    }

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    if (speed <= 0) {
        speed = 10.0 * mm / s;
    }

    setFeedrate(speed);
    m_layer_start = false;

    rv += QString(ccw ? "G03" : "G02") % writeCoordinates(end_point, params, kToolFrameZR) %
          writeArcCenterParameters(start_point, center_point) % m_f %
          QString::number(speed.to(m_meta.m_velocity_unit), 'f', 4) % commentSpaceLine(printMoveComment(params));
    m_first_deposition = false;
    return rv;
}

QString ArcSpecialtiesWriter::writeAfterPath(RegionType type) {
    QString rv;
    if (!m_spiral_layer) {
        // rv += writeWelderOff(); // update to turn off the welder
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
    return rv;
}

QString ArcSpecialtiesWriter::writeAfterRegion(RegionType type) { return QString(); }

QString ArcSpecialtiesWriter::writeAfterIsland() { return QString(); }

QString ArcSpecialtiesWriter::writeAfterPart() { return QString(); }

QString ArcSpecialtiesWriter::writeAfterLayer() {
    QString layer_code = m_sb->setting<QString>(PRS::GCode::kLayerCodeChange);
    return layer_code.isEmpty() ? QString() : layer_code % m_newline;
}

QString ArcSpecialtiesWriter::writeShutdown() {
    QString rv;
    rv += writeWelderOff();
    if (!m_sb->setting<QString>(PRS::GCode::kEndCode).isEmpty()) {
        rv += m_sb->setting<QString>(PRS::GCode::kEndCode) % m_newline;
    }
    if (m_startup_kinematics_written && m_absolute_arc_center_mode_enabled) {
        rv += "G164" % m_newline;
    }
    rv += "M49" % commentSpaceLine("ROBOT GO HOME");
    rv += "#CHANNEL INIT [CMDPOS]" % m_newline;
    rv += "M02" % commentSpaceLine("PROGRAM END");
    return rv;
}

QString ArcSpecialtiesWriter::writeDwell(Time time) {
    if (time > 0) {
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    }
    return QString();
}

QString ArcSpecialtiesWriter::writeWelderOn() {
    if (!m_deposition_active) {
        QString rv;
        rv += "G82" % commentSpaceLine("WIRE ARC WELDER ON");
        rv += "G261" % commentSpaceLine("BLENDING ON");
        m_deposition_active = true;
        return rv;
    }
    else {
        return QString();
    }
}

QString ArcSpecialtiesWriter::writeWelderOff(int mode) {
    if (m_deposition_active) {
        QString rv;
        const int g83_mode = validatedG83Mode(mode);
        rv += "G260" % commentSpaceLine("BLENDING OFF");
        rv += "G83 [" % QString::number(g83_mode) % "]" % commentSpaceLine("WIRE ARC WELDER OFF");
        m_deposition_active = false;
        return rv;
    }
    else {
        return QString();
    }
}

QString ArcSpecialtiesWriter::writeMotion(const QString& command, const Point& destination, Velocity speed,
                                          const QSharedPointer<SettingsBase>& params, const QString& comment) {
    return writeMotion(command, destination, speed, params, comment, destination);
}

QString ArcSpecialtiesWriter::writeMotion(const QString& command, const Point& destination, Velocity speed,
                                          const QSharedPointer<SettingsBase>& params, const QString& comment,
                                          const Point& cp_reference) {
    setFeedrate(speed);
    if (command == "G00") {
        return command % writeCoordinates(destination, params, kRapidTravelToolFrameZR, cp_reference) %
               commentSpaceLine(comment);
    }
    else {
        return command % writeCoordinates(destination, params, kToolFrameZR, cp_reference) % m_f %
               QString::number(speed.to(m_meta.m_velocity_unit), 'f', 4) % commentSpaceLine(comment);
    }
}

QString ArcSpecialtiesWriter::writeStartupKinematics() {
    if (m_startup_kinematics_written) {
        return QString();
    }

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
    if (m_pending_layer_change.isEmpty()) {
        return QString();
    }

    m_pending_layer_change.prepend(m_newline);
    QString rv = m_pending_layer_change;
    m_pending_layer_change.clear();
    return rv;
}

QString ArcSpecialtiesWriter::writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params,
                                               double tool_frame_zr) {
    return writeCoordinates(destination, params, tool_frame_zr, destination);
}

QString ArcSpecialtiesWriter::writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params,
                                               double tool_frame_zr, const Point& cp_reference) {
    const double ap_output = m_sb->setting<Angle>(PRS::MachineSetup::kAxisA).to(m_meta.m_angle_unit);
    const double cp_output = Angle(cpAxisForPoint(cp_reference, params) * degree).to(m_meta.m_angle_unit);
    const Point output_destination = rotateGCodeCoordinateFramePoint(destination);

    return QString(" X=") % QString::number(Distance(output_destination.x()).to(m_meta.m_distance_unit), 'f', 4) %
           " Y=" % QString::number(Distance(output_destination.y()).to(m_meta.m_distance_unit), 'f', 4) % " Z=" %
           QString::number(Distance(output_destination.z()).to(m_meta.m_distance_unit), 'f', 4) % " XR=" %
           QString::number(kToolFrameXR, 'f', 4) % " YR=" % QString::number(kToolFrameYR, 'f', 4) % " ZR=" %
           QString::number(tool_frame_zr, 'f', 4) % " AP=" % QString::number(ap_output, 'f', 4) % " CP=" %
           QString::number(cp_output, 'f', 4);
}

Point ArcSpecialtiesWriter::firstTravelPointAboveTravelLowerDestination(const Point& travel_destination,
                                                                        const Point& travel_lower_destination,
                                                                        Distance travel_lift_height) const {
    const Point lower_output_destination = rotateGCodeCoordinateFramePoint(travel_lower_destination);
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
    if (hasBuildMaximumZ()) {
        safe_z = getBuildMaximumZ() + kStartupWorldApproachZBuffer;
    }
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

    const double center_x = params->setting<Distance>(kRadialCenterX)();
    const double center_y = params->setting<Distance>(kRadialCenterY)();
    const Point transformed_destination = rotateGCodeCoordinateFramePoint(destination);
    const Point transformed_center = rotateGCodeCoordinateFramePoint(Point(center_x, center_y, destination.z()));
    double cp_degrees = std::atan2(transformed_destination.y() - transformed_center.y(),
                                   transformed_destination.x() - transformed_center.x()) *
                        180.0 / M_PI;

    if (isHelicalPathPattern()) {
        const HelicalPathHandedness handedness =
            static_cast<HelicalPathHandedness>(params->setting<int>(PS::Slicing::kHelicalPathHandedness));
        const double start_angle = helicalStartAngle(params);
        cp_degrees =
            handedness == HelicalPathHandedness::kLeftHanded ? start_angle - cp_degrees : cp_degrees - start_angle;
    }

    cp_degrees += m_sb->setting<Angle>(PRS::MachineSetup::kAxisC).to(degree);

    return normalizeDegrees(cp_degrees);
}

double ArcSpecialtiesWriter::helicalStartAngle(const QSharedPointer<SettingsBase>& params) const {
    const Angle start_angle = params->setting<Angle>(PS::Slicing::kHelicalPathStartAngle);
    const Point start_direction(std::cos(start_angle()), std::sin(start_angle()), 0.0);
    const Point transformed_start_direction = rotateGCodeCoordinateFrameDelta(start_direction);
    if (std::hypot(transformed_start_direction.x(), transformed_start_direction.y()) <=
        std::numeric_limits<double>::epsilon()) {
        return 90.0;
    }

    return std::atan2(transformed_start_direction.y(), transformed_start_direction.x()) * 180.0 / M_PI;
}

bool ArcSpecialtiesWriter::isHelicalPathPattern() const {
    const CylindricalPathPattern path_pattern =
        static_cast<CylindricalPathPattern>(m_sb->setting<int>(PS::Slicing::kCylindricalPathPattern));
    return isCylindricalSlicingMode() && path_pattern == CylindricalPathPattern::kHelical;
}

QString ArcSpecialtiesWriter::printMoveComment(const QSharedPointer<SettingsBase>& params) const {
    if (isCylindricalSlicingMode()) {
        const QString region =
            isHelicalPathPattern() ? Constants::RegionTypeStrings::kHelical : Constants::RegionTypeStrings::kRadial;
        const Point output_center = rotateGCodeCoordinateFramePoint(
            Point(params->setting<Distance>(kRadialCenterX), params->setting<Distance>(kRadialCenterY)));
        return region % " " % kCylindricalAxisXComment %
               QString::number(Distance(output_center.x()).to(m_meta.m_distance_unit), 'f', 4) % " " %
               kCylindricalAxisYComment %
               QString::number(Distance(output_center.y()).to(m_meta.m_distance_unit), 'f', 4);
    }

    return toString(m_region_type);
}

bool ArcSpecialtiesWriter::isCylindricalSlicingMode() const {
    const SlicingMode slicing_mode = static_cast<SlicingMode>(m_sb->setting<int>(PS::Slicing::kSlicingMode));
    return slicing_mode == SlicingMode::kCylindrical;
}
} // namespace ORNL
