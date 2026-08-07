#include "gcode/writers/wolf_writer.h"

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
WolfWriter::WolfWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString WolfWriter::writeSettingsHeader(GcodeSyntax syntax) {
    QString text = "";
    text += WriterBase::writeSettingsHeader(syntax);

    text += m_newline;
    return text;
}

QString WolfWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                      int num_layers) {
    m_current_z = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);
    m_current_rpm = 0;
    m_current_robot = 1;
    m_machine_type = m_sb->setting<MachineType>(PRS::MachineSetup::kMachineType);
    m_deposition_active = false;
    m_first_travel = true;
    m_first_print = true;
    m_layer_start = true;
    m_min_z = 0.0f;
    QString rv;

    if (m_sb->setting<QString>(PRS::GCode::kStartCode) != "")
        rv += m_sb->setting<QString>(PRS::GCode::kStartCode);

    rv += m_newline;

    rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

    return rv;
}

QString WolfWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
    m_layer_start = true;
    m_bead_count = 0;
    QString rv;
    return rv;
}

QString WolfWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString WolfWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString WolfWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    m_current_type = type;
    return rv;
}

QString WolfWriter::writeBeforePath(RegionType type) {
    QString rv;
    m_current_type = type;
    if (!m_spiral_layer || m_first_print) {
        if (type == RegionType::kPerimeter) {
            rv += "M1T2D4F0" % commentLine("PERIMETER START");
            m_wolf_path_type = 1;
            if (!m_sb->setting<QString>(PS::GCode::kPerimeterStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kPerimeterStart) % m_newline;
        }
        else if (type == RegionType::kInset) {
            rv += "M2T2D4F0" % commentLine("INSET START");
            m_wolf_path_type = 2;
            if (!m_sb->setting<QString>(PS::GCode::kInsetStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kInsetStart) % m_newline;
        }
        else if (type == RegionType::kSkeleton) {
            rv += "M8T2D4F0" % commentLine("SKELETON START");
            m_wolf_path_type = 8;
            if (!m_sb->setting<QString>(PS::GCode::kSkeletonStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSkeletonStart) % m_newline;
        }
        else if (type == RegionType::kSkin) {
            rv += "M5T2D4F0" % commentLine("SKIN START");
            m_wolf_path_type = 5;
            if (!m_sb->setting<QString>(PS::GCode::kSkinStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSkinStart) % m_newline;
        }
        else if (type == RegionType::kInfill) {
            rv += "M5T2D4F0" % commentLine("INFILL START");
            m_wolf_path_type = 5;
            if (!m_sb->setting<QString>(PS::GCode::kInfillStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kInfillStart) % m_newline;
        }
        else if (type == RegionType::kSupport) {
            if (!m_sb->setting<QString>(PS::GCode::kSupportStart).isEmpty())
                rv += m_sb->setting<QString>(PS::GCode::kSupportStart) % m_newline;
        }
        else {}
    }
    m_bead_count++;
    rv += commentLine("BEAD NUMBER: " % QString::number(m_bead_count));
    return rv;
}

QString WolfWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                QSharedPointer<SettingsBase> params) {
    QString rv;

    Point new_start_location;
    RegionType rType = params->setting<RegionType>(SS::kRegionType);

    // travel_lift vector in direction normal to the layer
    // with length = lift height as defined in settings
    QVector3D travel_lift = getTravelLift();

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

    // write the lift
    if (travel_lift_required && !m_first_travel &&
        (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        Point lift_destination = new_start_location + travel_lift; // lift destination is above start location

        rv += m_G1 % m_f %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              writeCoordinates(lift_destination) % commentSpaceLine("TRAVEL LIFT Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // write the travel
    Point travel_destination = target_location;
    if (m_first_travel)
        travel_destination.z(qAbs(m_sb->setting<Distance>(PRS::Dimensions::kZOffset)() +
                                  m_sb->setting<Distance>(PS::Travel::kLiftHeight)() +
                                  m_sb->setting<Distance>(PS::Layer::kLayerHeight)()));
    else if (travel_lift_required)
        travel_destination = travel_destination + travel_lift; // travel destination is above the target point

    rv += m_G1 % m_f % QString::number(m_sb->setting<Velocity>(PS::Travel::kSpeed).to(m_meta.m_velocity_unit)) %
          writeCoordinates(travel_destination) % commentSpaceLine("TRAVEL");
    setFeedrate(m_sb->setting<Velocity>(PS::Travel::kSpeed));

    if (m_first_travel)         // if this is the first travel
        m_first_travel = false; // update for next one

    // write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G1 % m_f %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              writeCoordinates(target_location) % commentSpaceLine("TRAVEL LOWER Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    return rv;
}

QString WolfWriter::writeLine(const Point& start_point, const Point& target_point,
                              const QSharedPointer<SettingsBase> params) {
    // Get the settings
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    int rpm = params->setting<int>(SS::kExtruderSpeed);
    int material_number = params->setting<int>(SS::kMaterialNumber);
    RegionType region_type = params->setting<RegionType>(SS::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    float output_rpm = rpm * m_sb->setting<float>(PRS::MachineSpeed::kGearRatio);

    QString rv;

    if ((path_modifiers == PathModifiers::kAngledTipWipe || path_modifiers == PathModifiers::kForwardTipWipe ||
         path_modifiers == PathModifiers::kPerimeterTipWipe || path_modifiers == PathModifiers::kReverseTipWipe) &&
        m_wolf_path_type != 6) {
        rv += "M6" % commentSpaceLine("TIP WIPE START");
    }

    if (!m_deposition_active && rpm > 0) {
        rv += writeExtruderOn(region_type, rpm, 0);
        setFeedrate(0);
    }

    rv += m_G1;
    // Forces first motion of layer to issue speed (needed for spiralize mode so that feedrate is scaled properly)
    if (m_layer_start) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
        m_current_rpm = rpm;
        m_layer_start = false;
    }

    // Update feedrate if needed
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
        rv += commentSpaceLine(toString(region_type));

    m_first_print = false;

    return rv;
}

QString WolfWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                             const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    // Return value
    QString rv;
    return rv;
}

QString WolfWriter::writeAfterPath(RegionType type) {
    QString rv;
    if (!m_spiral_layer) {
        rv += writeExtruderOff(0); // update to turn off the extruder
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

QString WolfWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString WolfWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString WolfWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString WolfWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString WolfWriter::writeShutdown() {
    QString rv;
    rv += writeFinalTravelLift(
        [&](const Point& destination) {
            const Velocity z_speed = m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed);
            setFeedrate(z_speed);
            return m_G1 % m_f % QString::number(z_speed.to(m_meta.m_velocity_unit)) % writeCoordinates(destination);
        },
        "TRAVEL FINAL LIFT Z");
    return rv;
}

QString WolfWriter::writePurge(int RPM, int duration, int delay) {
    QString rv;
    return rv;
}

QString WolfWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString WolfWriter::writeExtruderOn(RegionType region_type, int rpm, int extruder_number) {
    QString rv;
    m_deposition_active = true;

    rv += "M101" % commentSpaceLine("TURN PUMP ON");

    // Retrieve the appropriate dwell time for the region
    Time dwell_time = 0;
    switch (region_type) {
        case RegionType::kPerimeter:
            dwell_time = m_sb->setting<Time>(MS::Extruder::kOnDelayPerimeter);
            break;
        case RegionType::kInset:
            dwell_time = m_sb->setting<Time>(MS::Extruder::kOnDelayInset);
            break;
        case RegionType::kSkeleton:
            dwell_time = m_sb->setting<Time>(MS::Extruder::kOnDelaySkeleton);
            break;
        case RegionType::kSkin:
            dwell_time = m_sb->setting<Time>(MS::Extruder::kOnDelaySkin);
            break;
        case RegionType::kInfill:
            dwell_time = m_sb->setting<Time>(MS::Extruder::kOnDelayInfill);
            break;
        default:
            break;
    }

    // Write the appropriate dwell time for the region
    if (dwell_time > 0) {
        rv += writeDwell(dwell_time);
    }

    return rv;
}

QString WolfWriter::writeExtruderOff(int extruder_number) {
    QString rv;
    m_deposition_active = false;

    // Retrieve relevant settings
    Time off_delay = m_sb->setting<Time>(MS::Extruder::kOffDelay);
    if (off_delay > 0) {
        rv += writeDwell(off_delay);
    }

    rv += "M103" % commentSpaceLine("TURN PUMP OFF");
    m_current_rpm = 0;

    return rv;
}

QString WolfWriter::writeCoordinates(Point destination) {
    QString rv;
    // always specify X, Y, I, J, K, and L
    rv += m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4) %
          " I0.0000 J0.0000 K1.0000 L0.0000";

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
