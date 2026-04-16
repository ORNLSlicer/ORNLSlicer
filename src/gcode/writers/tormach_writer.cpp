#include "gcode/writers/tormach_writer.h"

#include <QStringBuilder>
#include <qcontainerfwd.h>
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
TormachWriter::TormachWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString TormachWriter::writeSettingsHeader(GcodeSyntax syntax) {
    QString text = "";
    text += commentLine("Slicing Parameters");
    WriterBase::writeSettingsHeader(syntax);

    text += m_newline;
    return text;
}

QString TormachWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                         int num_layers) {
    m_current_z = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);
    m_current_rpm = 0;
    for (int ext = 0, end = m_extruders_on.size(); ext < end; ++ext) // all extruders off initially
        m_extruders_on[ext] = false;
    m_first_travel = true;
    m_first_print = true;
    m_layer_start = true;
    m_tip_wipe = false;
    m_min_z = 0.0f;
    m_bead_number = 0;
    m_minimum_x = minimum_x;
    m_maximum_x = maximum_x;
    m_minimum_y = minimum_y;
    m_maximum_y = maximum_y;
    QString rv;
    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        rv += commentLine("SAFETY BLOCK - ESTABLISH OPERATIONAL MODES");
        rv += "G21" % commentSpaceLine("SET UNITS TO MILLIMETERS");
        rv += "G90" % commentSpaceLine("USE ABSOLUTE POSITIONING");
        rv += "G54" % commentSpaceLine("WORK COORDINATE SYSTEM");
        rv += "T1 G43 H1" % commentSpaceLine("SET TOOL HEIGHT OF TORCH");
        // rv += "M64 P1" % commentSpaceLine("ROBOT READY HIGH - WAIT FOR INPUT HIGH*****");
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

    if (m_sb->setting<QString>(PRS::GCode::kStartCode) != "")
        rv += m_sb->setting<QString>(PRS::GCode::kStartCode);

    rv += m_newline;

    rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

    return rv;
}

QString TormachWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
    m_layer_start = true;
    m_bead_number = 0;
    QString rv;
    rv += "M1 " % commentLine("OPTIONAL STOP - LAYER CHANGE");
    return rv;
}

QString TormachWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString TormachWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString TormachWriter::writeBeforeScan(Point min, Point max, int layer, int boundingBox, Axis axis, Angle angle) {
    QString rv;
    return rv;
}

QString TormachWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
}

QString TormachWriter::writeBeforePath(RegionType type) {
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
        else {}
    }
    return rv;
}

QString TormachWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                   QSharedPointer<SettingsBase> params) {
    QString rv;

    Point new_start_location;
    RegionType rType = params->setting<RegionType>(SS::kRegionType);

    m_tip_wipe = false;
    m_bead_number++;

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

    // Travel_lift vector in direction normal to the layer
    // With length = lift height as defined in settings
    QVector3D travel_lift = getTravelLift();

    // Write the lift
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        Point lift_destination = new_start_location + travel_lift; // lift destination is above start location

        rv += m_G0 % writeCoordinates(lift_destination) % commentSpaceLine("TRAVEL LIFT Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // Travel Pause
    // if pause is enabled and not first travel
    // if cnetroid move is enabled
    // centroid move
    // issue pause
    if (m_sb->setting<bool>(PS::Travel::kEnableTravelPause) && !m_first_travel) {
        if (m_sb->setting<bool>(PS::Travel::kEnableTravelCentroidMove)) {
            Distance average_x, average_y;
            average_x = (m_minimum_x + m_maximum_x) / 2;
            average_y = (m_minimum_y + m_maximum_y) / 2;
            Point pause_location(average_x, average_y, m_current_z);
            rv += m_G0 % writeCoordinates(pause_location) % commentSpaceLine("TRAVEL TO PAUSE LOCATION");
        }
        rv += writeDwell(m_sb->setting<Time>(PS::Travel::kTravelPauseDuration));
    }

    // Write the travel
    Point travel_destination = target_location;
    if (travel_lift_required)
        travel_destination = travel_destination + travel_lift; // travel destination is above the target point

    rv += m_G0 % writeCoordinates(travel_destination) % commentSpaceLine("TRAVEL");
    setFeedrate(m_sb->setting<Velocity>(PS::Travel::kSpeed));

    if (m_first_travel)         // if this is the first travel
        m_first_travel = false; // update for next one

    // Write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G0 % writeCoordinates(target_location) % commentSpaceLine("TRAVEL LOWER Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    return rv;
}

QString TormachWriter::writeLine(const Point& start_point, const Point& target_point,
                                 const QSharedPointer<SettingsBase> params) {
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    int rpm = params->setting<int>(SS::kExtruderSpeed);
    int material_number = params->setting<int>(SS::kMaterialNumber);
    RegionType region_type = params->setting<RegionType>(SS::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    float output_rpm = rpm * m_sb->setting<float>(PRS::MachineSpeed::kGearRatio);

    QString rv;

    for (int extruder : params->setting<QVector<int>>(SS::kExtruders)) {
        // Turn on the extruder if it isn't already on
        if (m_extruders_on[0] == false && rpm > 0) // only check first extruder
        {
            rv += writeExtruderOn(region_type, rpm, extruder);
            // Set Feedrate to 0 if turning extruder on so that an F parameter
            // is issued with the first G1 of the path
            setFeedrate(0);
        }
    }

    rv += m_G1;
    // Forces first motion of layer to issue speed (needed for spiralize mode so that feedrate is scaled properly)
    if (m_layer_start) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
        m_current_rpm = rpm;
        m_layer_start = false;
    }

    // Update feedrate and extruder speed if needed
    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }

    // writes XYZ to destination
    rv += writeCoordinates(target_point);

    // add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type) % m_space % "Bead #" % QString::number(m_bead_number));

    m_first_print = false;

    return rv;
}

QString TormachWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                                const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    QString rv;
    return rv;
}

QString TormachWriter::writeScan(Point target_point, Velocity speed, bool on_off) {
    QString rv;
    return rv;
}

QString TormachWriter::writeAfterPath(RegionType type) {
    QString rv;
    if (!m_spiral_layer) {
        rv += writeExtruderOff(0); // update to turn off appropriate extruders
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

QString TormachWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString TormachWriter::writeAfterScan(Distance beadWidth, Distance laserStep, Distance laserResolution) {
    QString rv;
    return rv;
}

QString TormachWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString TormachWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString TormachWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString TormachWriter::writeShutdown() {
    QString rv;
    rv +=
        m_sb->setting<QString>(PRS::GCode::kEndCode) % m_newline % "M65 P1" % commentSpaceLine("ROBOT READY LOW *****");
    return rv;
}

QString TormachWriter::writePurge(int RPM, int duration, int delay) {
    QString rv;
    return rv;
}

QString TormachWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString TormachWriter::writeExtruderOn(RegionType type, int rpm, int extruder_number) {
    QString rv;
    m_extruders_on[extruder_number] = true;

    if (type == RegionType::kInset) {
        if (m_sb->setting<Time>(MS::Extruder::kOnDelayInset) > 0) {
            rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayInset));
        }
    }
    else if (type == RegionType::kSkin) {
        if (m_sb->setting<Time>(MS::Extruder::kOnDelaySkin) > 0) {
            rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelaySkin));
        }
    }
    else if (type == RegionType::kInfill) {
        if (m_sb->setting<Time>(MS::Extruder::kOnDelayInfill) > 0) {
            rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayInfill));
        }
    }
    else if (type == RegionType::kSkeleton) {
        if (m_sb->setting<Time>(MS::Extruder::kOnDelaySkeleton) > 0) {
            rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelaySkeleton));
        }
    }
    else {
        if (m_sb->setting<Time>(MS::Extruder::kOnDelayPerimeter) > 0) {
            rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayPerimeter));
        }
    }

    rv += "M64 P0" % commentSpaceLine("WELDING START -HIGH *******");
    rv += "M66 P0 L1 Q10" % commentSpaceLine("ARC STABLE, WAIT FOR FEEDBACK FOR MAX 10 SECONDS BEFORE TIME OUT*******");

    return rv;
}

QString TormachWriter::writeExtruderOff(int extruder_number) {
    QString rv;
    m_extruders_on[extruder_number] = false;
    if (m_sb->setting<Time>(MS::Extruder::kOffDelay) > 0) {
        rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOffDelay));
    }
    m_current_rpm = 0;
    rv += "M65 P0" % commentSpaceLine("Welding Start -Low*******");
    return rv;
}

QString TormachWriter::writeCoordinates(Point destination) {
    QString rv;

    // always specify X and Y
    rv += m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4);

    // write vertical coordinate along the correct axis (Z or W) according to printer settings
    // only output Z/W coordinate if there was a change in Z/W
    Distance z_offset = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);

    Distance target_z = destination.z() + z_offset;
    if (qAbs(target_z - m_last_z) > 10) {
        if (target_z > -1 && target_z < 1) // fix for small value creating negative 0 g-code output
        {
            target_z = 0;
        }
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z = target_z;
    }
    return rv;
}
} // namespace ORNL
