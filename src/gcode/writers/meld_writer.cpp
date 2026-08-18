#include "gcode/writers/meld_writer.h"

#include <algorithm>
#include <cmath>

#include <QStringBuilder>

#include <qhashfunctions.h>
#include <qnumeric.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
//! @brief Segment setting key used by radial/helical cylindrical slicers to recover the cylinder center X.
const QString kRadialCenterX = "radial_center_x";

//! @brief Segment setting key used by radial/helical cylindrical slicers to recover the cylinder center Y.
const QString kRadialCenterY = "radial_center_y";

//! @brief Segment setting key used by MeldWriter to recover the path center X.
const QString kMeldScalingCenterX = "meld_scaling_center_x";

//! @brief Segment setting key used by MeldWriter to recover the path center Y.
const QString kMeldScalingCenterY = "meld_scaling_center_y";

//! @brief Segment setting key used by MeldWriter to recover the path radius.
const QString kMeldScalingRadius = "meld_scaling_radius";

constexpr double kDepositionOutputTolerance = 1.0e-5;
constexpr double kMeldVelocityOutputScale = 1000.0;

Distance averageEndpointRadius(const Point& start_point, const Point& target_point, const Point& center) {
    const double start_radius = std::hypot(start_point.x() - center.x(), start_point.y() - center.y());
    const double end_radius = std::hypot(target_point.x() - center.x(), target_point.y() - center.y());
    return Distance((start_radius + end_radius) / 2.0);
}

double meldVelocityOutputValue(double deposition_value) {
    return Velocity(deposition_value).to(in / minute) * kMeldVelocityOutputScale;
}
} // namespace

MeldWriter::MeldWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString MeldWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                      int num_layers) {
    m_current_z = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);
    m_current_w = m_sb->setting<Distance>(PRS::Dimensions::kWMax);
    m_current_deposition_commanded_value = 0.0;
    m_current_deposition_output_value = 0.0;
    m_deposition_active = false;
    m_first_travel      = true;
    m_first_print       = true;
    m_layer_start       = true;
    m_min_z             = 0.0f;
    m_material_number   = -1;
    QString rv;
    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        rv += commentLine("preamble");
        rv += "G700" % commentSpaceLine("INCH, IPM");
        rv += "G54" % commentSpaceLine("COORDINATES");
        rv += "G90" % commentSpaceLine("ABSOLUTE");
    }

    if (m_sb->setting<int>(PRS::GCode::kEnableBoundingBox)) {
        rv += "G0 Z0" % commentSpaceLine("RAISE Z TO DEMO BOUNDING BOX") % m_G0 % m_x %
              QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" %
              QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(maximum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" %
              QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(maximum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" %
              QString::number(maximum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" %
              QString::number(maximum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % m_G0 %
              m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" %
              QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX") % "M0" %
              commentSpaceLine("WAIT FOR USER");

        m_start_point = Point(minimum_x, minimum_y, 0);
    }

    if (m_sb->setting<QString>(PRS::GCode::kStartCode) != "") rv += m_sb->setting<QString>(PRS::GCode::kStartCode);

    rv += m_newline;

    rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

    return rv;
}

QString MeldWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
    m_layer_start  = true;
    QString rv;
    return rv;
}

QString MeldWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString MeldWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString MeldWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
}

QString MeldWriter::writeBeforePath(RegionType type) {
    QString rv;
    if (!m_spiral_layer || m_first_print) {
        if (type == RegionType::kPerimeter) {
            if (!m_sb->setting<QString>(PS::GCode::kPerimeterStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kPerimeterStart) % m_newline;
        }
        else if (type == RegionType::kInset) {
            if (!m_sb->setting<QString>(PS::GCode::kInsetStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kInsetStart) % m_newline;
        }
        else if (type == RegionType::kSkeleton) {
            if (!m_sb->setting<QString>(PS::GCode::kSkeletonStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSkeletonStart) % m_newline;
        }
        else if (type == RegionType::kSkin) {
            if (!m_sb->setting<QString>(PS::GCode::kSkinStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSkinStart) % m_newline;
        }
        else if (type == RegionType::kInfill) {
            if (!m_sb->setting<QString>(PS::GCode::kInfillStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kInfillStart) % m_newline;
        }
        else if (type == RegionType::kSupport) {
            if (!m_sb->setting<QString>(PS::GCode::kSupportStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSupportStart) % m_newline;
        }
    }
    return rv;
}

QString MeldWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                QSharedPointer<SettingsBase> params) {
    QString rv;
    Velocity speed  = params->setting<Velocity>(SS::kSpeed);
    Velocity zSpeed = m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed);

    Point new_start_location;

    // Use updated start location if this is the first travel
    if (m_first_travel)
        new_start_location = m_start_point;
    else
        new_start_location = start_location;

    Distance liftDist;
    liftDist = m_sb->setting<Distance>(PS::Travel::kLiftHeight);

    bool travel_lift_required = liftDist > 0;  // && !m_first_travel; //do not write a lift on first travel

    // Don't lift for short travel moves
    if (start_location.distance(target_location) < m_sb->setting<Distance>(PS::Travel::kMinTravelForLift)) {
        travel_lift_required = false;
    }

    // travel_lift vector in direction normal to the layer
    // with length = lift height as defined in settings
    QVector3D travel_lift = getTravelLift();

    // write the lift
    if (travel_lift_required && !m_first_travel &&
        (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        Point lift_destination = new_start_location + travel_lift;  // lift destination is above start location
        rv += m_G1 % m_f % QString::number(zSpeed.to(m_meta.m_velocity_unit)) % writeCoordinates(lift_destination) %
              commentSpaceLine("TRAVEL LIFT Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // write the travel
    Point travel_destination = target_location;
    if (m_first_travel)
        travel_destination.z(qAbs(m_sb->setting<Distance>(PRS::Dimensions::kZOffset)()));
    else if (travel_lift_required)
        travel_destination = travel_destination + travel_lift;  // travel destination is above the target point

    if (m_first_travel) {
        rv += m_G0 % writeXYCoordinates(travel_destination) % commentSpaceLine("TRAVEL");
    }
    else {
        rv += m_G1 % m_f % QString::number(speed.to(m_meta.m_velocity_unit)) % writeCoordinates(travel_destination) %
              commentSpaceLine("TRAVEL");
        setFeedrate(m_sb->setting<Velocity>(PS::Travel::kSpeed));
    }

    // write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G1 % m_f % QString::number(zSpeed.to(m_meta.m_velocity_unit)) %
              writeCoordinates(target_location, true) %
              commentSpaceLine("TRAVEL LOWER Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    if (m_first_travel) {
        // Meld expects L003 after the initial positioning moves and before any actuator command.
        rv += "L003" % m_newline;
    }

    if (m_first_travel)         // if this is the first travel
        m_first_travel = false; // update for next one

    return rv;
}

QString MeldWriter::writeLine(const Point& start_point, const Point& target_point,
                              const QSharedPointer<SettingsBase> params) {
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    const double deposition_value = params->setting<double>(SS::kExtruderSpeed);
    RegionType region_type = params->setting<RegionType>(SS::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    const double output_value = depositionOutputValue(deposition_value, params, start_point, target_point);

    QString rv;

    // turn on the extruder if it isn't already on
    if (m_deposition_active == false && deposition_value > 0) {
        rv += writeExtruderOn(region_type, deposition_value, params, start_point, target_point);
    }
    if (deposition_value == 0 && m_deposition_active == true) {
        rv += writeExtruderOff();
    }

    const bool update_deposition = m_deposition_active && deposition_value > 0 && depositionOutputChanged(output_value);
    const bool use_actuator_update = update_deposition && depositionUpdateUsesActuatorCommand();
    if (use_actuator_update) {
        rv += writeDepositionUpdate(deposition_value, output_value);
    }

    rv += m_G1;
    // update feedrate if needed
    if (getFeedrate() != speed || m_layer_start) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
        m_layer_start = false;
    }

    if (update_deposition && !use_actuator_update) {
        rv += writeDepositionUpdate(deposition_value, output_value);
    }

    // writes WXYZ to destination
    rv += writeCoordinates(target_point);

    // add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    m_first_print = false;

    return rv;
}

QString MeldWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                             const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    QString rv;

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    const double deposition_value = params->setting<double>(SS::kExtruderSpeed);
    int material_number = params->setting<int>(SS::kMaterialNumber);
    auto region_type    = params->setting<RegionType>(SS::kRegionType);
    auto path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    const double output_value = depositionOutputValue(deposition_value, params, start_point, end_point);

    // Turn on the extruder if it isn't already on
    if (!m_deposition_active && deposition_value > 0) {
        rv += writeExtruderOn(region_type, deposition_value, params, start_point, end_point);
    }

    const bool update_deposition = m_deposition_active && deposition_value > 0 && depositionOutputChanged(output_value);
    const bool use_actuator_update = update_deposition && depositionUpdateUsesActuatorCommand();
    if (use_actuator_update) {
        rv += writeDepositionUpdate(deposition_value, output_value);
    }

    rv += ((ccw) ? m_G3 : m_G2);

    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }
    if (update_deposition && !use_actuator_update) {
        rv += writeDepositionUpdate(deposition_value, output_value);
    }

    rv += m_i % QString::number(Distance(center_point.x() - start_point.x()).to(m_meta.m_distance_unit), 'f', 4) % m_j %
          QString::number(Distance(center_point.y() - start_point.y()).to(m_meta.m_distance_unit), 'f', 4) % m_x %
          QString::number(Distance(end_point.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(end_point.y()).to(m_meta.m_distance_unit), 'f', 4);

    // write vertical coordinate along the correct axis (Z or W) according to printer settings
    // only output Z/W coordinate if there was a change in Z/W
    Distance z_offset = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);

    Distance target_z = end_point.z() + z_offset;
    if (qAbs(target_z - m_last_z) > 10) {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z    = target_z;
    }

    // Add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    return rv;
}

QString MeldWriter::writeAfterPath(RegionType type) {
    QString rv;
    if (!m_spiral_layer) {
        rv += writeExtruderOff();
        if (type == RegionType::kPerimeter) {
            if (!m_sb->setting<QString>(PS::GCode::kPerimeterEnd).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kPerimeterEnd) % m_newline;
        }
        else if (type == RegionType::kInset) {
            if (!m_sb->setting<QString>(PS::GCode::kInsetEnd).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kInsetEnd) % m_newline;
        }
        else if (type == RegionType::kSkeleton) {
            if (!m_sb->setting<QString>(PS::GCode::kSkeletonEnd).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSkeletonEnd) % m_newline;
        }
        else if (type == RegionType::kSkin) {
            if (!m_sb->setting<QString>(PS::GCode::kSkinEnd).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSkinEnd) % m_newline;
        }
        else if (type == RegionType::kInfill) {
            if (!m_sb->setting<QString>(PS::GCode::kInfillEnd).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kInfillEnd) % m_newline;
        }
        else if (type == RegionType::kSupport) {
            if (!m_sb->setting<QString>(PS::GCode::kSupportEnd).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSupportEnd) % m_newline;
        }
    }
    return rv;
}

QString MeldWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString MeldWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString MeldWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString MeldWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString MeldWriter::writeShutdown() {
    QString rv;
    rv += commentLine("CONCLUSION");
    rv += "M34 S500" % commentSpaceLine("EXTRUDER RATE .5IPM");
    rv += "G91" % commentSpaceLine("RELATIVE POSITIONING");
    rv += "G1 Z3.0 F10.0" % commentSpaceLine("RAISE TIP FROM WORK");
    rv += "G90" % commentSpaceLine("ABSOLUTE POSITIONING");
    rv += "G4 X0.5" % commentSpaceLine("DWELL");
    rv += "M5" % commentSpaceLine("SPINDLE STOP");
    rv += "M35" % commentSpaceLine("EXTRUDER STOP");
    rv += "G4 X0.5" % commentSpaceLine("DWELL");
    rv += "M30" % commentSpaceLine("END OF G-CODE");
    rv += "%" % m_newline;
    return rv;
}

QString MeldWriter::writePurge(int RPM, int duration, int delay) {
    return "M69 F" % QString::number(RPM) % m_p % QString::number(duration) % m_s % QString::number(delay) %
           commentSpaceLine("PURGE");
}

QString MeldWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString MeldWriter::writeExtruderOn(RegionType type, double deposition_value,
                                    const QSharedPointer<SettingsBase>& params,
                                    const Point& start_point, const Point& target_point) {
    QString rv;
    m_deposition_active = true;
    const bool friction_stir =
        m_sb->setting<MachineType>(PRS::MachineSetup::kMachineType) == MachineType::kFrictionStir;
    const double initial_deposition_value = friction_stir ? 0.0 : getInitialExtruderSpeed(params);
    const double actuator_deposition_value = initial_deposition_value > 0 ? initial_deposition_value : deposition_value;
    const double output_value = depositionOutputValue(actuator_deposition_value, params, start_point, target_point);

    if (!m_sb->setting<int>(PRS::MachineSpeed::kMeldDiscrete)) {
        rv += "M4" % m_s % QString::number(output_value) % " @714" % commentSpaceLine("TURN SPINDLE ON");
        rv += "M34" % m_s % QString::number(output_value) % " @714" % commentSpaceLine("TURN EXTRUDER ON");
        rv += "M54" % commentSpaceLine("HOLD FOR DEPOSITION START");
        setCurrentDepositionValue(actuator_deposition_value, output_value);
    }
    else {
        if (initial_deposition_value > 0) {
            setCurrentDepositionValue(initial_deposition_value, output_value);

            rv += "M24 S" % QString::number(output_value) % commentSpaceLine("TURN ACTUATOR ON");

            if (type == RegionType::kInset) {
                if (m_sb->setting<Time>(MS::Extruder::kOnDelayInset) > 0)
                    rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayInset));
            }
            else if (type == RegionType::kSkin) {
                if (m_sb->setting<Time>(MS::Extruder::kOnDelaySkin) > 0)
                    rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelaySkin));
            }
            else if (type == RegionType::kInfill) {
                if (m_sb->setting<Time>(MS::Extruder::kOnDelayInfill) > 0)
                    rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayInfill));
            }
            else if (type == RegionType::kSkeleton) {
                if (m_sb->setting<Time>(MS::Extruder::kOnDelaySkeleton) > 0)
                    rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelaySkeleton));
            }
            else {
                if (m_sb->setting<Time>(MS::Extruder::kOnDelayPerimeter) > 0)
                    rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayPerimeter));
            }
        }
        else {
            rv += "M24 S" % QString::number(output_value) % commentSpaceLine("TURN ACTUATOR ON");
            // Only update the current value if not using feedrate scaling. An updated value here could prevent the S
            // parameter from being issued during the first G1 motion of the path and thus the extruder rate won't
            // properly scale. Meld Friction Stir actuator updates are emitted as M24 commands, so recording this M24
            // prevents an immediate duplicate update without removing any G1 S parameter.
            const bool force_feedrate_update =
                m_sb->setting<int>(MS::Cooling::kForceMinLayerTime) &&
                m_sb->setting<int>(MS::Cooling::kForceMinLayerTimeMethod) ==
                    (int)ForceMinimumLayerTime::kSlow_Feedrate;
            if (!force_feedrate_update || depositionUpdateUsesActuatorCommand()) {
                setCurrentDepositionValue(deposition_value, output_value);
            }
            else {
                m_current_deposition_commanded_value = 0.0;
                m_current_deposition_output_value = -1.0;
            }
        }
    }

    return rv;
}

QString MeldWriter::writeExtruderOff() {
    QString rv;
    if (m_sb->setting<int>(PRS::MachineSpeed::kMeldDiscrete)) {
        rv += "M25" % commentSpaceLine("TURN ACTUATOR OFF");
        m_deposition_active = false;
        setCurrentDepositionValue(0.0, 0.0);
    }
    return rv;
}

QString MeldWriter::writeDepositionUpdate(double deposition_value, double output_value) {
    QString rv;
    if (depositionUpdateUsesActuatorCommand())
        rv += "M24 S" % QString::number(output_value) % commentSpaceLine("UPDATE ACTUATOR");
    else
        rv += m_s % QString::number(output_value);

    setCurrentDepositionValue(deposition_value, output_value);
    return rv;
}

bool MeldWriter::depositionUpdateUsesActuatorCommand() const {
    return m_sb->setting<int>(PRS::MachineSpeed::kMeldDiscrete) &&
           m_sb->setting<MachineType>(PRS::MachineSetup::kMachineType) == MachineType::kFrictionStir;
}

double MeldWriter::depositionOutputValue(double deposition_value, const QSharedPointer<SettingsBase>& params,
                                         const Point& start_point, const Point& target_point) const {
    if (m_sb->setting<MachineType>(PRS::MachineSetup::kMachineType) != MachineType::kFrictionStir)
        return deposition_value * m_sb->setting<float>(PRS::MachineSpeed::kGearRatio);

    const double output_value = meldVelocityOutputValue(deposition_value);
    if (!m_sb->setting<bool>(PRS::MachineSpeed::kMeldDepositionRateScaling))
        return output_value;

    if (params == nullptr || !params->contains(SS::kWidth))
        return output_value;

    const Distance bead_width = params->setting<Distance>(SS::kWidth);
    if (bead_width <= 0)
        return output_value;

    Distance radius;
    if (params->contains(kMeldScalingRadius)) {
        radius = params->setting<Distance>(kMeldScalingRadius);
    }
    else if (params->contains(kRadialCenterX) && params->contains(kRadialCenterY)) {
        radius = averageEndpointRadius(start_point, target_point,
                                       Point(params->setting<Distance>(kRadialCenterX),
                                             params->setting<Distance>(kRadialCenterY), Distance(start_point.z())));
    }
    else if (params->contains(kMeldScalingCenterX) && params->contains(kMeldScalingCenterY)) {
        radius =
            averageEndpointRadius(start_point, target_point,
                                  Point(params->setting<Distance>(kMeldScalingCenterX),
                                        params->setting<Distance>(kMeldScalingCenterY), Distance(start_point.z())));
    }
    else {
        return output_value;
    }

    if (radius < 0)
        return output_value;

    const double scale = std::min(1.0, (2.0 * radius()) / bead_width());
    return output_value * scale;
}

bool MeldWriter::depositionOutputChanged(double deposition_value) const {
    const double tolerance =
        std::max(kDepositionOutputTolerance, std::fabs(m_current_deposition_output_value) * 1.0e-6f);
    return std::fabs(deposition_value - m_current_deposition_output_value) > tolerance;
}

void MeldWriter::setCurrentDepositionValue(double commanded_value, double output_value) {
    m_current_deposition_commanded_value = commanded_value;
    m_current_deposition_output_value = output_value;
}

QString MeldWriter::writeXYCoordinates(Point destination) {
    QString rv;

    // always specify X and Y
    rv += m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4);
    return rv;
}

QString MeldWriter::writeCoordinates(Point destination, bool force_z) {
    QString rv = writeXYCoordinates(destination);

    // write vertical coordinate along the correct axis (Z or W) according to printer settings
    // only output Z/W coordinate if there was a change in Z/W
    Distance z_offset = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);

    Distance target_z = destination.z() + z_offset;
    if (force_z || qAbs(target_z - m_last_z) > 10) {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z    = target_z;
    }
    return rv;
}

}  // namespace ORNL
