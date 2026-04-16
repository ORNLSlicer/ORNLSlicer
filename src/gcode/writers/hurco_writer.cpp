#include "gcode/writers/hurco_writer.h"

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
HurcoWriter::HurcoWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString HurcoWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                       int num_layers) {
    m_current_z = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);
    m_current_w = m_sb->setting<Distance>(PRS::Dimensions::kWMax);
    m_current_rpm = 0;
    m_extruders_on[0] = false;
    m_first_travel = true;
    m_first_print = true;
    m_layer_start = true;
    m_min_z = 0.0f;
    m_material_number = -1;
    QString rv;
    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        rv += "T2M6" % m_newline % "G00G90G21" % m_newline % "G54" % m_newline % "(toolbegin)" % m_newline % "T2" %
              m_newline % "M06" % m_newline % "G90" % m_newline % "G43H2Z100." % m_newline % "G05.3P50." % m_newline %
              "G1 X0 Y0 F4000 (MOVE TO ORIGIN)" % m_newline;
    }

    if (m_sb->setting<int>(PRS::GCode::kEnableBoundingBox)) {
        rv += m_G0 % m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" %
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

    if (m_sb->setting<QString>(PRS::GCode::kStartCode) != "")
        rv += m_sb->setting<QString>(PRS::GCode::kStartCode);

    rv += m_newline;

    rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

    return rv;
}

QString HurcoWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
    m_layer_start = true;
    QString rv;
    return rv;
}

QString HurcoWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString HurcoWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString HurcoWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
}

QString HurcoWriter::writeBeforePath(RegionType type) {
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

QString HurcoWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                 QSharedPointer<SettingsBase> params) {
    QString rv;

    Point new_start_location;

    // Use updated start location if this is the first travel
    if (m_first_travel)
        new_start_location = m_start_point;
    else
        new_start_location = start_location;

    Distance liftDist;
    liftDist = m_sb->setting<Distance>(PS::Travel::kLiftHeight);

    bool travel_lift_required = liftDist > 0; // && !m_first_travel; //do not write a lift on first travel

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
        Point lift_destination = new_start_location + travel_lift; // lift destination is above start location
        rv += m_G0 % writeCoordinates(lift_destination) % commentSpaceLine("TRAVEL LIFT Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // write the travel
    Point travel_destination = target_location;
    if (m_first_travel)
        travel_destination.z(qAbs(m_sb->setting<Distance>(PRS::Dimensions::kZOffset)()));
    else if (travel_lift_required)
        travel_destination = travel_destination + travel_lift; // travel destination is above the target point

    rv += m_G0 % writeCoordinates(travel_destination) % commentSpaceLine("TRAVEL");
    setFeedrate(m_sb->setting<Velocity>(PS::Travel::kSpeed));

    // write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G0 % writeCoordinates(target_location) % commentSpaceLine("TRAVEL LOWER Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    if (m_first_travel)         // if this is the first travel
        m_first_travel = false; // update for next one

    return rv;
}

QString HurcoWriter::writeLine(const Point& start_point, const Point& target_point,
                               const QSharedPointer<SettingsBase> params) {
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    int rpm = params->setting<int>(SS::kExtruderSpeed);
    RegionType region_type = params->setting<RegionType>(SS::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    float output_rpm = rpm * m_sb->setting<float>(PRS::MachineSpeed::kGearRatio);

    QString rv;

    // turn on the extruder if it isn't already on
    if (m_extruders_on[0] == false && rpm > 0) {
        rv += writeExtruderOn(region_type, rpm);
    }

    rv += m_G1;
    // Forces first motion of layer to issue speed (needed for spiralize mode so that feedrate is scaled properly)
    if (m_layer_start) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));

        rv += m_s % QString::number(output_rpm);
        m_current_rpm = rpm;

        m_layer_start = false;
    }

    // Update feedrate and extruder speed if needed
    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }

    if (rpm != m_current_rpm) {
        rv += m_s % QString::number(output_rpm);
        m_current_rpm = rpm;
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

QString HurcoWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                              const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    QString rv;

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    int rpm = params->setting<int>(SS::kExtruderSpeed);
    int material_number = params->setting<int>(SS::kMaterialNumber);
    auto region_type = params->setting<RegionType>(SS::kRegionType);
    auto path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    float output_rpm = rpm * m_sb->setting<float>(PRS::MachineSpeed::kGearRatio);

    // Turn on the extruder if it isn't already on
    if (!m_extruders_on[0] && rpm > 0) {
        rv += writeExtruderOn(region_type, rpm);
    }

    rv += ((ccw) ? m_G3 : m_G2);

    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }
    if (rpm != m_current_rpm) {
        rv += m_s % QString::number(output_rpm);
        m_current_rpm = rpm;
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
        m_last_z = target_z;
    }

    // Add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    return rv;
}

QString HurcoWriter::writeAfterPath(RegionType type) {
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

QString HurcoWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString HurcoWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString HurcoWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString HurcoWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString HurcoWriter::writeShutdown() {
    QString rv;

    rv += "G91G28Z0.0" % m_newline;
    rv += "G91G28Y0.0" % m_newline;

    rv += m_sb->setting<QString>(PRS::GCode::kEndCode) % m_newline % "M19" % commentSpaceLine("ORIENT SPINDLE") %
          "M30" % commentSpaceLine("END OF G-CODE") % "%" % m_newline;
    return rv;
}

QString HurcoWriter::writePurge(int RPM, int duration, int delay) {
    return "M69 F" % QString::number(RPM) % m_p % QString::number(duration) % m_s % QString::number(delay) %
           commentSpaceLine("PURGE");
}

QString HurcoWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString HurcoWriter::writeExtruderOn(RegionType type, int rpm) {
    QString rv;
    m_extruders_on[0] = true;
    float output_rpm;

    if (m_sb->setting<int>(MS::Extruder::kInitialSpeed) > 0) {
        output_rpm =
            m_sb->setting<float>(PRS::MachineSpeed::kGearRatio) * m_sb->setting<int>(MS::Extruder::kInitialSpeed);

        // Only update the current rpm if not using feedrate scaling. An updated rpm value here could prevent the S
        // parameter from being issued during the first G1 motion of the path and thus the extruder rate won't properly
        // scale
        if (!(m_sb->setting<int>(MS::Cooling::kForceMinLayerTime) &&
              m_sb->setting<int>(MS::Cooling::kForceMinLayerTimeMethod) == (int)ForceMinimumLayerTime::kSlow_Feedrate))
            m_current_rpm = m_sb->setting<int>(MS::Extruder::kInitialSpeed);

        rv += m_M3 % m_s % QString::number(output_rpm) % commentSpaceLine("TURN EXTRUDER ON");

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
        output_rpm = m_sb->setting<float>(PRS::MachineSpeed::kGearRatio) * rpm;
        rv += m_M3 % m_s % QString::number(output_rpm) % commentSpaceLine("TURN EXTRUDER ON");
        // Only update the current rpm if not using feedrate scaling. An updated rpm value here could prevent the S
        // parameter from being issued during the first G1 motion of the path and thus the extruder rate won't properly
        // scale
        if (!(m_sb->setting<int>(MS::Cooling::kForceMinLayerTime) &&
              m_sb->setting<int>(MS::Cooling::kForceMinLayerTimeMethod) == (int)ForceMinimumLayerTime::kSlow_Feedrate))
            m_current_rpm = rpm;
    }

    return rv;
}

QString HurcoWriter::writeExtruderOff() {
    QString rv;
    m_extruders_on[0] = false;
    if (m_sb->setting<Time>(MS::Extruder::kOffDelay) > 0) {
        rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOffDelay));
    }
    rv += m_M5 % commentSpaceLine("TURN EXTRUDER OFF");
    return rv;
}

QString HurcoWriter::writeCoordinates(Point destination) {
    QString rv;

    // always specify X and Y
    rv += m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4);

    // write vertical coordinate along the correct axis (Z or W) according to printer settings
    // only output Z/W coordinate if there was a change in Z/W
    Distance z_offset = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);

    Distance target_z = destination.z() + z_offset;
    if (qAbs(target_z - m_last_z) > 10) {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z = target_z;
    }
    return rv;
}

} // namespace ORNL
