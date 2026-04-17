#include "gcode/writers/aerobasic_writer.h"

#include <QStringBuilder>
#include <qhashfunctions.h>
#include <qnumeric.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
AeroBasicWriter::AeroBasicWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {
    // Set to one extruder that is off
    m_extruders_on.clear();
    m_extruders_on.push_back(false);
}

QString AeroBasicWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x,
                                           Distance maximum_y, int num_layers) {
    QString rv;

    m_current_z = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);
    m_filament_location = 0.0;

    m_first_print = true;
    m_first_travel = true;
    m_min_z = 0.0f;

    rv += commentLine("Layer height: " %
                      QString::number(m_sb->setting<Distance>(PS::Layer::kLayerHeight).to(m_meta.m_distance_unit)));

    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        // Enable each axis
        rv += "ENABLE X Y Z" % m_newline;

        // Set all axis to absolute
        rv += "ABSOLUTE" % m_newline;

        // Allows for continuous motion from one command into the next (does not set velocity to zero between motion
        // commands)
        rv += "VELOCITY ON" % m_newline;

        // Lookahead is used by the VELOCITY ON function
        rv += "LOOKAHEAD FAST" % m_newline;

        rv += m_newline;
    }

    // Adds user-defined start code
    if (m_sb->setting<QString>(PRS::GCode::kStartCode) != "")
        rv += m_sb->setting<QString>(PRS::GCode::kStartCode);

    rv += m_newline;

    // Add layer count
    rv += commentLine(m_meta.m_layer_count_delimiter % ":" % QString::number(num_layers));

    return rv;
}

QString AeroBasicWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writeBeforePath(RegionType type) {
    QString rv;

    // Only write out before paths that are not spiralized, or always if this is the first of the print
    // These write out the user defined start codes
    if (!m_sb->setting<bool>(PS::SpecialModes::kEnableSpiralize) || m_first_print) {
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

QString AeroBasicWriter::writeLayerChange(uint layer_number) {
    QString rv;

    rv += commentLine(QString("BEGINNING LAYER: ") % QString::number(layer_number + 1));

    return rv;
}

QString AeroBasicWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                     QSharedPointer<SettingsBase> params) {
    QString rv;

    // Disable extruder for travels
    if (!m_extruders_on.empty() && m_extruders_on.first())
        rv += "$DO[0].X=0" % commentLine("Extruder Off for travel");

    Velocity speed = params->setting<Velocity>(SS::kSpeed);

    Point new_start_location;

    // Use updated start location if this is the first travel
    if (m_first_travel)
        new_start_location = m_start_point;
    else
        new_start_location = start_location;

    Distance lift_height = m_sb->setting<Distance>(PS::Travel::kLiftHeight);

    bool travel_lift_required = lift_height > 0;

    // Don't lift for short travel moves
    if (start_location.distance(target_location) < m_sb->setting<Distance>(PS::Travel::kMinTravelForLift)) {
        travel_lift_required = false;
    }

    // travel_lift vector in direction normal to the layer
    // with length = lift height as defined in settings
    QVector3D travel_lift = getTravelLift();

    // write the lift
    if (travel_lift_required) {
        Point lift_destination = new_start_location + travel_lift; // lift destination is above start location
        rv += m_G1 % writeCoordinates(lift_destination) % m_f %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              commentSpaceLine("TRAVEL LIFT Z");

        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // write the travel
    Point travel_destination = target_location;
    if (travel_lift_required)
        travel_destination = travel_destination + travel_lift; // travel destination is above the target point

    rv += m_G1 % writeCoordinates(travel_destination) % m_f % QString::number(speed.to(m_meta.m_velocity_unit)) %
          commentSpaceLine("TRAVEL");

    setFeedrate(speed);

    // write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G1 % writeCoordinates(target_location) % m_f %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              commentSpaceLine("TRAVEL LOWER Z");

        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // Enable extruder after travel
    if (!m_extruders_on.empty() && m_extruders_on.first())
        rv += "$DO[0].X=1" % commentLine("Extruder On after travel");

    m_first_travel = false;
    return rv;
}

QString AeroBasicWriter::writeLine(const Point& start_point, const Point& target_point,
                                   const QSharedPointer<SettingsBase> params) {
    QString rv;

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    RegionType region_type = params->setting<RegionType>(SS::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);

    rv += m_G1;

    // Writes XYZ to destination
    rv += writeCoordinates(target_point);

    // Update feedrate and speed if needed
    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }

    // Add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    m_first_print = false;

    return rv;
}

QString AeroBasicWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                                  const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    QString rv;

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    auto region_type = params->setting<RegionType>(SS::kRegionType);
    auto path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);

    rv += ((ccw) ? m_G3 : m_G2);

    // write vertical coordinate along the correct axis (Z or W) according to printer settings
    // only output Z/W coordinate if there was a change in Z/W
    Distance z_offset = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);

    rv += m_x % QString::number(Distance(end_point.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(end_point.y()).to(m_meta.m_distance_unit), 'f', 4);

    // TODO: might need to handle Z using a seperate G1
    // Only add z if there was a change
    Distance target_z = end_point.z() + z_offset;
    if (qAbs(target_z - m_last_z) > 10) {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z = target_z;
    }

    rv += m_r % QString::number(Distance(end_point.y()).to(m_meta.m_distance_unit), 'f', 4);

    // Add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    return rv;
}

QString AeroBasicWriter::writeAfterPath(RegionType type) {
    QString rv;
    if (!m_sb->setting<bool>(PS::SpecialModes::kEnableSpiralize)) {
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

QString AeroBasicWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString AeroBasicWriter::writeShutdown() {
    QString rv;

    // Disable extruder and UV
    rv += "$DO[0].X=0" % commentSpaceLine("Disable extruder");

    // Reset head in X and Y
    rv += "LINEAR X0 Y0" % commentSpaceLine("Move head to home X and Y");

    // Set to incremental/ relative mode and move up
    rv += "INCREMENTAL" % commentSpaceLine("Set to relative mode");
    rv += "RAPID F50 Z50" % commentSpaceLine("Move up");
    rv += "G53" % m_newline;
    rv += "$DO[1].X=0" % commentSpaceLine("Disable UV");

    rv += "ABSOLUTE" % commentSpaceLine("Set to ABSOLUTE mode");

    return rv;
}

QString AeroBasicWriter::writePurge(int RPM, int duration, int delay) { return {}; }

QString AeroBasicWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % " " % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString AeroBasicWriter::writeCoordinates(Point destination) {
    QString rv;

    // always specify X and Y
    rv += m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4);

    // write vertical coordinate only if there was a change in Z
    Distance z_offset = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);

    Distance target_z = destination.z() + z_offset;
    if (qAbs(target_z - m_last_z) > 10) {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z = target_z;
    }
    return rv;
}

QString AeroBasicWriter::writeRetraction() {
    QString rv;
    return rv;
}

QString AeroBasicWriter::writePrime() {
    QString rv;
    return rv;
}
} // namespace ORNL
