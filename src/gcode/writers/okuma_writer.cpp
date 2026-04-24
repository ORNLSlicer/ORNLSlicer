#include "gcode/writers/okuma_writer.h"

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
OkumaWriter::OkumaWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString OkumaWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
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
    rv += "( --- header.txt --- )" % m_newline;
    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        rv += "( -------------------- laser_toolchange.txt --- )" % m_newline % "T100 M6" % m_newline %
              "( -------------------- )" % m_newline % "( -------------------- MOVED FROM LASER TECH FILE --- )" %
              m_newline % "(DISK_SPEED      XX  = DISK_SPEED)" % m_newline % "(CARRIER_GAS     5   = CARRIER_GAS)" %
              m_newline % "(SHIELDING	10	= hyperMILL_AM_SHIELDING_GAS)" % m_newline % "M369    (MIST COLLECTOR ON)" %
              m_newline % "M130                (OK TO RUN WITH NO SPINDLE RPM)" % m_newline %
              "M1510               (LASER HEAD DOWN)" % m_newline % "/G115 SPS=3.5  (SPOT SIZE SET TO VARIABLE)" %
              m_newline % "/M1507             (FOCAL CONTROL ON. ALLOWS FOCAL POINT CHANGE)" % m_newline %
              "/LHD=VC100         (ACTUAL SPOT SIZE CHANGE. VC100 IS SET BY G115 SPS=)" % m_newline %
              "/LPWW=2000         (SET VARIABLE FOR LASER POWER, WATTS)" % m_newline %
              "/M1500             (CHANGE FOCAL POINT)" % m_newline % "/M1508             (FOCAL CONTROL OFF)" %
              m_newline % "DW = 0.01 (Dwell time between border and layer)" % m_newline %
              "/M1501             (LASER READY)" % m_newline % "/LPW=0             (LASER POWER IS ZERO)" % m_newline %
              "/M1541             (HOPPER-OFF: HOPPER_OFF)" % m_newline % "/M1503     (LASER ON)" % m_newline %
              "( -------------------- MOVED FROM LASER TECH FILE --- )" % m_newline %
              "( --- files_x\\job_start.txt --- )" % m_newline % "( OPERATION 2 )" % m_newline %
              "( T100 Additive Manufacturing )" % m_newline % "( --- )" % m_newline % "M510 (CAS OFF)" % m_newline %
              "G00 G17 G21 G40 G80 G90" % m_newline % m_newline % "( --- sP.txt ---)" % m_newline % "G0 Z=VPSLZ" %
              m_newline % "G15 H1" % m_newline % "M11" % m_newline % "M27" % m_newline %
              "( --- rotate_rotary_axes_additive.txt ---)" % m_newline % "G611 HL=1" % m_newline %
              "G90 G00 A0. C0.        (USER DEFINED)" % m_newline % "X0. Y0.                (USER DEFINED)" %
              m_newline % "C0" % m_newline % "A0" % m_newline % "G612" % m_newline % "( --- )" % m_newline % "()" %
              m_newline % "G169 HL=1 X4.082 Y-52.5458 Z136. A0 C0 " % m_newline % "( --- 5X_begin.txt --- )" %
              m_newline % "G130 (TURN OFF SUPERNURBS)" % m_newline % "G0 X4.082 Y-52.5458 Z136.0" % m_newline %
              "G0 Z0.0503 A0 C0 " % m_newline;
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

QString OkumaWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
    m_layer_start = true;
    QString rv;
    rv += "( -------------------- laser_on.txt --- )" % m_newline;
    return rv;
}

QString OkumaWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString OkumaWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString OkumaWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
}

QString OkumaWriter::writeBeforePath(RegionType type) {
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

QString OkumaWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
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

QString OkumaWriter::writeLine(const Point& start_point, const Point& target_point,
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

    // turn off extruder with an M5 before the line, rather than in-line with S0
    // if (rpm == 0 && m_extruders_on[0] == true)
    //{
    //     rv += writeExtruderOff();
    // }

    rv += m_G1;
    // Forces first motion of layer to issue speed (needed for spiralize mode so that feedrate is scaled properly)
    if (m_layer_start) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));

        m_layer_start = false;
    }

    // Update feedrate and extruder speed if needed
    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
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

QString OkumaWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
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

QString OkumaWriter::writeAfterPath(RegionType type) {
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

QString OkumaWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString OkumaWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString OkumaWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString OkumaWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString OkumaWriter::writeShutdown() {
    QString rv;

    rv += "( --- 5X_end.txt --- )" % m_newline;
    rv += "G170 " % m_newline;
    rv += "G0 Z=VPSLZ" % m_newline;
    rv += "()" % m_newline;
    rv += "( --- files_x\\job_end.txt --- )" % m_newline;
    rv += "M511 (CAS ON)" % m_newline;
    rv += "( --- laser_end.txt --- )" % m_newline;
    rv += "G90 G0 A0. C0." % m_newline;
    rv += "/M1504" % m_newline;
    rv += "/M1542" % m_newline;
    rv += "/M1532" % m_newline;
    rv += "/M1522" % m_newline;
    rv += "/M1512" % m_newline;
    rv += "/M1502               (LASER READY OFF)" % m_newline;
    rv += "/M1509               (LASER HEAD UP)" % m_newline;
    rv += "M2" % m_newline;
    rv += "( --- )" % m_newline;
    return rv;
}

QString OkumaWriter::writePurge(int RPM, int duration, int delay) { return {}; }

QString OkumaWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString OkumaWriter::writeExtruderOn(RegionType type, int rpm) {
    QString rv;
    m_extruders_on[0] = true;
    rv += "( -------------------- laser_on.txt --- )" % m_newline;
    rv += "/LPW=LPWW        (LASER POWER)" % m_newline;
    rv += "( -------------------- )" % m_newline;
    return rv;
}

QString OkumaWriter::writeExtruderOff() {
    QString rv;
    m_extruders_on[0] = false;
    rv += "( -------------------- laser_off.txt --- )" % m_newline;
    rv += "/LPW=0               (LASER POWER)" % m_newline;
    rv += "(/)" % m_newline;
    rv += "( -------------------- )" % m_newline;
    return rv;
}

QString OkumaWriter::writeCoordinates(Point destination) {
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
