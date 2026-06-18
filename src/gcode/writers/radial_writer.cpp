#include "gcode/writers/radial_writer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QStringBuilder>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
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

//! @brief Returns a point offset away from the radial cylinder axis by the configured lift height.
Point radialLiftedPoint(const Point& point, const QSharedPointer<SettingsBase>& params, Distance lift_height) {
    const double center_x = params->setting<Distance>(kRadialCenterX)();
    const double center_y = params->setting<Distance>(kRadialCenterY)();
    const double dx = point.x() - center_x;
    const double dy = point.y() - center_y;
    const double length = std::hypot(dx, dy);

    if (length <= std::numeric_limits<double>::epsilon()) {
        return Point(point.x(), point.y(), point.z() + lift_height());
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

//! @brief Smallest travel arc segment used to keep arc-like travel output bounded.
const Distance kMinTravelArcSegmentLength = 100.0 * micron;

//! @brief Formats a distance using the output unit declared by the active radial gcode metadata.
QString formatDistance(Distance value, Distance unit) {
    return QString::number(value.to(unit), 'f', 4) % unit.toString();
}

//! @brief Formats an angle using the output unit declared by the active radial gcode metadata.
QString formatAngle(Angle value, Angle unit) { return QString::number(value.to(unit), 'f', 4) % unit.toString(); }
} // namespace

RadialWriter::RadialWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb), m_c(" C") {}

QString RadialWriter::writeSettingsHeader(GcodeSyntax) {
    const SlicerType slicer_type = static_cast<SlicerType>(m_sb->setting<int>(PS::Slicing::kSlicerType));
    const bool helical_mode = slicer_type == SlicerType::kHelicalSlice;

    QString text;
    text += commentLine(helical_mode ? "Helical Slicing Parameters" : "Radial Slicing Parameters");
    text +=
        commentLine(QString(helical_mode ? "Helical Initial Radius: " : "Radial Initial Radius: ") %
                    formatDistance(m_sb->setting<Distance>(PS::Slicing::kRadialInitialRadius), m_meta.m_distance_unit));
    const RadialAxisMode radial_axis_mode =
        static_cast<RadialAxisMode>(m_sb->setting<int>(PS::Slicing::kRadialAxisMode));
    text +=
        commentLine(QString(helical_mode ? "Helical Axis Mode: " : "Radial Axis Mode: ") % toString(radial_axis_mode));
    if (radial_axis_mode == RadialAxisMode::kCustomXY) {
        text += commentLine(QString(helical_mode ? "Helical Axis X: " : "Radial Axis X: ") %
                            formatDistance(m_sb->setting<Distance>(PS::Slicing::kRadialAxisX), m_meta.m_distance_unit));
        text += commentLine(QString(helical_mode ? "Helical Axis Y: " : "Radial Axis Y: ") %
                            formatDistance(m_sb->setting<Distance>(PS::Slicing::kRadialAxisY), m_meta.m_distance_unit));
    }
    text += commentLine(QString(helical_mode ? "Helical Radial Spacing: " : "Radial Layer Spacing: ") %
                        formatDistance(m_sb->setting<Distance>(PS::Layer::kLayerHeight), m_meta.m_distance_unit));
    if (helical_mode) {
        const Distance bead_width = m_sb->setting<Distance>(PS::Layer::kBeadWidth);
        text += commentLine("Helical Rise Per Revolution: " % formatDistance(bead_width, m_meta.m_distance_unit));
        text += commentLine("Helical Rise Per Radian: " %
                            formatDistance(bead_width / (2.0 * M_PI), m_meta.m_distance_unit));
    }
    else {
        text += commentLine("Vertical Bead Spacing: " %
                            formatDistance(m_sb->setting<Distance>(PS::Layer::kBeadWidth), m_meta.m_distance_unit));
    }
    text += commentLine(
        QString(helical_mode ? "Helical Boundary Handling: " : "Radial Boundary Handling: ") %
        toString(static_cast<RadialBoundaryHandling>(m_sb->setting<int>(PS::Slicing::kRadialBoundaryHandling))));
    text += commentLine("Travel Lift Distance: " %
                        formatDistance(m_sb->setting<Distance>(PS::Travel::kLiftHeight), m_meta.m_distance_unit));
    text += commentLine("A Axis Tilt: " %
                        formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kAxisA), m_meta.m_angle_unit));
    text += commentLine("C Axis Offset: " %
                        formatAngle(m_sb->setting<Angle>(PRS::MachineSetup::kAxisC), m_meta.m_angle_unit));

    return text;
}

QString RadialWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                        int num_layers) {
    m_first_travel = true;
    m_layer_start = true;
    m_have_last_c = false;
    m_last_c_degrees = 0.0;
    setFeedrate(0.0);

    QString rv;
    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        rv += "G90" % commentSpaceLine("USE ABSOLUTE COORDINATES");
    }

    if (m_sb->setting<int>(PRS::GCode::kEnableBoundingBox)) {
        rv += m_G0 % m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % m_y %
              QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(maximum_x.to(m_meta.m_distance_unit), 'f', 4) % m_y %
              QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(maximum_x.to(m_meta.m_distance_unit), 'f', 4) % m_y %
              QString::number(maximum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % m_y %
              QString::number(maximum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % m_y %
              QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX");
    }

    if (!m_sb->setting<QString>(PRS::GCode::kStartCode).isEmpty()) {
        rv += m_sb->setting<QString>(PRS::GCode::kStartCode) % m_newline;
    }

    rv += m_newline % commentLine(m_meta.m_layer_count_delimiter % ":" % QString::number(num_layers));
    return rv;
}

QString RadialWriter::writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) {
    m_layer_start = true;
    return QString();
}

QString RadialWriter::writeBeforePart(QVector3D normal) { return QString(); }

QString RadialWriter::writeBeforeIsland() { return QString(); }

QString RadialWriter::writeBeforeRegion(RegionType type, int pathSize) { return QString(); }

QString RadialWriter::writeBeforePath(RegionType type) { return QString(); }

QString RadialWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                  QSharedPointer<SettingsBase> params) {
    Velocity speed = params->setting<Velocity>(PS::Travel::kSpeed);
    if (speed <= 0) {
        speed = params->setting<Velocity>(PRS::MachineSpeed::kMaxXYSpeed);
    }

    Velocity lift_speed = m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed);
    if (lift_speed <= 0) {
        lift_speed = speed;
    }

    const Distance lift_height = m_sb->setting<Distance>(PS::Travel::kLiftHeight);
    bool travel_lift_required = lift_height > 0 && lType != TravelLiftType::kNoLift;
    if (start_location.distance(target_location) < m_sb->setting<Distance>(PS::Travel::kMinTravelForLift)) {
        travel_lift_required = false;
    }

    auto writeTravelMove = [this, &params](Point destination, Velocity move_speed, QString comment) -> QString {
        QString line = m_G0;
        if (move_speed > 0 && getFeedrate() != move_speed) {
            line += m_f % QString::number(move_speed.to(m_meta.m_velocity_unit), 'f', 4);
            setFeedrate(move_speed);
        }

        return QString(line % writeCoordinates(destination, params) % commentSpaceLine(comment));
    };

    auto writeRadialArcTravel = [&params, &writeTravelMove](Point start, Point end, Velocity move_speed) -> QString {
        const double center_x = params->setting<Distance>(kRadialCenterX)();
        const double center_y = params->setting<Distance>(kRadialCenterY)();
        const double start_radius = radialDistance(start, params);
        const double end_radius = radialDistance(end, params);

        if (start_radius <= std::numeric_limits<double>::epsilon() ||
            end_radius <= std::numeric_limits<double>::epsilon()) {
            return writeTravelMove(end, move_speed, "TRAVEL");
        }

        const double start_angle = std::atan2(start.y() - center_y, start.x() - center_x);
        const double end_angle = std::atan2(end.y() - center_y, end.x() - center_x);
        const double delta_angle = shortestAngularDelta(start_angle, end_angle);
        const double max_radius = std::max(start_radius, end_radius);
        const double arc_length = std::abs(delta_angle) * max_radius;
        const double target_segment_length =
            std::max(params->setting<Distance>(SS::kWidth)() / 2.0, kMinTravelArcSegmentLength());

        if (arc_length <= target_segment_length) {
            return writeTravelMove(end, move_speed, "TRAVEL");
        }

        const int segments = std::clamp(static_cast<int>(std::ceil(arc_length / target_segment_length)), 2, 180);

        QString rv;
        for (int i = 1; i <= segments; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(segments);
            const double angle = start_angle + delta_angle * t;
            const double radius = start_radius + (end_radius - start_radius) * t;
            Point waypoint(center_x + radius * std::cos(angle), center_y + radius * std::sin(angle),
                           start.z() + (end.z() - start.z()) * t);
            if (i == segments) {
                waypoint = end;
            }

            rv += writeTravelMove(waypoint, move_speed, "TRAVEL ARC");
        }

        return rv;
    };

    QString rv;
    Point travel_start = start_location;

    if (travel_lift_required && !m_first_travel &&
        (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        travel_start = radialLiftedPoint(start_location, params, lift_height);
        rv += writeTravelMove(travel_start, lift_speed, "TRAVEL LIFT");
    }

    Point travel_destination = target_location;
    if (travel_lift_required) {
        travel_destination = radialLiftedPoint(target_location, params, lift_height);
    }

    rv += writeRadialArcTravel(travel_start, travel_destination, speed);

    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += writeTravelMove(target_location, lift_speed, "TRAVEL LOWER");
    }

    m_first_travel = false;
    return rv;
}

QString RadialWriter::writeLine(const Point&, const Point& target_point, const QSharedPointer<SettingsBase> params) {
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    if (speed <= 0) {
        speed = 10.0 * mm / s;
    }

    QString rv = m_G1;
    if (getFeedrate() != speed || m_layer_start) {
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit), 'f', 4);
        setFeedrate(speed);
        m_layer_start = false;
    }

    rv += writeCoordinates(target_point, params);
    const SlicerType slicer_type = static_cast<SlicerType>(m_sb->setting<int>(PS::Slicing::kSlicerType));
    rv += commentSpaceLine(slicer_type == SlicerType::kHelicalSlice ? Constants::RegionTypeStrings::kHelical
                                                                    : Constants::RegionTypeStrings::kRadial);

    return rv;
}

QString RadialWriter::writeAfterPath(RegionType type) { return QString(); }

QString RadialWriter::writeAfterRegion(RegionType type) { return QString(); }

QString RadialWriter::writeAfterIsland() { return QString(); }

QString RadialWriter::writeAfterPart() { return QString(); }

QString RadialWriter::writeAfterLayer() {
    QString layer_code = m_sb->setting<QString>(PRS::GCode::kLayerCodeChange);
    return layer_code.isEmpty() ? QString() : layer_code % m_newline;
}

QString RadialWriter::writeShutdown() {
    QString rv;
    if (!m_sb->setting<QString>(PRS::GCode::kEndCode).isEmpty()) {
        rv += m_sb->setting<QString>(PRS::GCode::kEndCode) % m_newline;
    }
    rv += "M84" % commentSpaceLine("DISABLE MOTORS");
    return rv;
}

QString RadialWriter::writeDwell(Time time) {
    if (time > 0) {
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    }
    return QString();
}

QString RadialWriter::writeCoordinates(const Point& destination, const QSharedPointer<SettingsBase>& params) {
    // A is a user-configured fixed table tilt for this 3+2 output. C follows
    // the endpoint angle around the radial slicing center.
    const double a_output = m_sb->setting<Angle>(PRS::MachineSetup::kAxisA).to(m_meta.m_angle_unit);
    const double c_output = Angle(cAxisForPoint(destination, params) * degree).to(m_meta.m_angle_unit);

    return m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
           QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4) % m_z %
           QString::number(Distance(destination.z()).to(m_meta.m_distance_unit), 'f', 4) % m_a %
           QString::number(a_output, 'f', 4) % m_c % QString::number(c_output, 'f', 4);
}

double RadialWriter::cAxisForPoint(const Point& destination, const QSharedPointer<SettingsBase>& params) {
    const double center_x = params->setting<Distance>(kRadialCenterX)();
    const double center_y = params->setting<Distance>(kRadialCenterY)();
    double c_degrees = std::atan2(destination.y() - center_y, destination.x() - center_x) * 180.0 / M_PI;
    c_degrees += m_sb->setting<Angle>(PRS::MachineSetup::kAxisC).to(degree);

    // Keep C continuous across the +/-180 degree boundary so adjacent points on
    // the same ring do not request large avoidable rotary moves.
    if (m_have_last_c) {
        while (c_degrees - m_last_c_degrees > 180.0) {
            c_degrees -= 360.0;
        }
        while (c_degrees - m_last_c_degrees < -180.0) {
            c_degrees += 360.0;
        }
    }

    m_last_c_degrees = c_degrees;
    m_have_last_c = true;
    return c_degrees;
}
} // namespace ORNL
