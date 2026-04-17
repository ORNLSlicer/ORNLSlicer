#include "gcode/writers/reprap_writer.h"

#include <QStringBuilder>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qnumeric.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/segments/arc.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
RepRapWriter::RepRapWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString RepRapWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                        int num_layers) {
    m_current_z = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);
    m_filament_location = 0.0;
    for (int i = 0, end = m_extruders_on.size(); i < end; ++i) // all extruders initially off
        m_extruders_on[i] = false;
    m_first_print = true;
    m_first_travel = true;
    m_layer_start = true;
    m_min_z = 0.0f;
    QString rv;

    rv += commentLine("Layer height: " %
                      QString::number(m_sb->setting<Distance>(PS::Layer::kLayerHeight).to(m_meta.m_distance_unit)));

    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        rv += "M140 S" % QString::number(m_sb->setting<Temperature>(MS::Temperatures::kBed).to(degC)) %
              commentSpaceLine("SET BED TEMPERATURE");
        if (m_sb->setting<int>(MS::Temperatures::kBed) > 0) {
            rv += "M190 S" % QString::number(m_sb->setting<Temperature>(MS::Temperatures::kBed).to(degC)) %
                  commentSpaceLine("SET BED TEMPERATURE AND WAIT");
        }

        rv += "M104 S" % QString::number(m_sb->setting<Temperature>(MS::Temperatures::kExtruder0).to(degC)) % " T0" %
              commentSpaceLine("SET EXTRUDER 0 TEMPERATURE");
        if (m_sb->setting<int>(MS::Temperatures::kBed) > 0) {
            rv += "M109 S" % QString::number(m_sb->setting<Temperature>(MS::Temperatures::kExtruder0).to(degC)) %
                  " T0" % commentSpaceLine("SET EXTRUDER 0 TEMPERATURE AND WAIT");
        }

        //            rv += "G28" % commentSpaceLine("TRAVEL HOME ALL AXES");

        if (m_sb->setting<int>(MS::Filament::kRelative) > 0) {
            rv += "M83" % commentSpaceLine("USE RELATIVE EXTRUSION DISTANCES");
        }
        else {
            rv += "M82" % commentSpaceLine("USE ABSOLUTE EXTRUSION DISTANCES");
        }

        rv += "G92 E0" % commentSpaceLine("RESET FILAMENT AXIS TO 0");

        rv += m_newline;
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
              QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX");

        m_start_point = Point(minimum_x, minimum_y, 0);
    }

    if (m_sb->setting<QString>(PRS::GCode::kStartCode) != "")
        rv += m_sb->setting<QString>(PRS::GCode::kStartCode);

    rv += m_newline;

    rv += commentLine(m_meta.m_layer_count_delimiter % ":" % QString::number(num_layers));

    return rv;
}

QString RepRapWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
    m_layer_start = true;
    QString rv;

    if (m_sb->setting<bool>(MS::Retraction::kEnable) && m_sb->setting<bool>(MS::Retraction::kLayerChange) &&
        new_min_z > sb->setting<Distance>(PS::Layer::kLayerHeight)) {
        rv += writeRetraction();
    }
    return rv;
}

QString RepRapWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString RepRapWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString RepRapWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
}

QString RepRapWriter::writeBeforePath(RegionType type) {
    QString rv;
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
    if ((m_filament_location > 0) && !m_sb->setting<bool>(MS::Filament::kRelative)) {
        rv += "G92 E0" % commentSpaceLine("RESET FILAMENT TO 0");
        m_filament_location = 0.0;
    }

    return rv;
}

QString RepRapWriter::writeLayerChange(uint layer_number) {
    QString rv;

    rv += commentLine(QString("BEGINNING LAYER: ") % QString::number(layer_number + 1));

    // Cooling fan
    if (layer_number >= 2 && m_sb->setting<bool>(MS::Cooling::kEnable))
        rv += "M106 S" % QString::number((m_sb->setting<int>(MS::Cooling::kMaxSpeed) / 100) * 255) %
              commentSpaceLine("ENABLE FAN");

    return rv;
}

QString RepRapWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                  QSharedPointer<SettingsBase> params) {
    QString rv;
    Velocity speed = params->setting<Velocity>(SS::kSpeed);

    Point new_start_location;
    setTools(QVector<int>()); // turn off all extruders

    // Update Acceleration
    if (m_sb->setting<bool>(PRS::Acceleration::kEnableDynamic)) {
        rv += "M204 T" %
              QString::number(m_sb->setting<Acceleration>(PRS::Acceleration::kDefault).to(m_meta.m_acceleration_unit)) %
              commentSpaceLine("UPDATE ACCELERATION");
    }

    // Use updated start location if this is the first travel
    if (m_first_travel)
        new_start_location = m_start_point;
    else
        new_start_location = start_location;

    Distance liftDist = m_sb->setting<Distance>(PS::Travel::kLiftHeight);

    bool travel_lift_required = liftDist > 0; // && !m_first_travel; //do not write a lift on first travel

    // Don't lift for short travel moves
    if (start_location.distance(target_location) < m_sb->setting<Distance>(PS::Travel::kMinTravelForLift)) {
        travel_lift_required = false;
    }

    // Retract on long travel moves
    if (start_location.distance(target_location) > m_sb->setting<Distance>(MS::Retraction::kMinTravel) &&
        (params->setting<bool>(SS::kIsRegionStartSegment) || !m_sb->setting<bool>(MS::Retraction::kOpenSpacesOnly))) {
        rv += writeRetraction();
    }

    // travel_lift vector in direction normal to the layer
    // with length = lift height as defined in settings
    QVector3D travel_lift = getTravelLift();

    // write the lift
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        Point lift_destination = new_start_location + travel_lift; // lift destination is above start location
        rv += m_G1 % m_f %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              writeCoordinates(lift_destination) % commentSpaceLine("TRAVEL LIFT Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // write the travel
    Point travel_destination = target_location;
    if (travel_lift_required)
        travel_destination = travel_destination + travel_lift; // travel destination is above the target point

    rv += m_G1 % m_f % QString::number(speed.to(m_meta.m_velocity_unit)) % writeCoordinates(travel_destination) %
          commentSpaceLine("TRAVEL");
    setFeedrate(speed);

    // write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G1 % m_f %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              writeCoordinates(target_location) % commentSpaceLine("TRAVEL LOWER Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    m_first_travel = false;
    return rv;
}

QString RepRapWriter::writeLine(const Point& start_point, const Point& target_point,
                                const QSharedPointer<SettingsBase> params) {
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    RegionType region_type = params->setting<RegionType>(SS::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);

    QString rv;

    // check if any extruders need priming
    bool needs_prime = false;
    for (int ext : params->setting<QVector<int>>(SS::kExtruders))
        needs_prime = !m_extruders_on[ext] || needs_prime;

    // set the tools/extruders before priming so that correct extruders get primed
    rv += setTools(params->setting<QVector<int>>(SS::kExtruders));

    // If first printing segment, prime extruder, or at least undo any retraction, and update acceleration
    // First segment of the path is signified by extruder being off and the modifier isn't one of five ending modifiers
    if (needs_prime && path_modifiers != PathModifiers::kSlowDown && path_modifiers != PathModifiers::kForwardTipWipe &&
        path_modifiers != PathModifiers::kReverseTipWipe && path_modifiers != PathModifiers::kCoasting &&
        path_modifiers != PathModifiers::kSpiralLift) {
        if (m_sb->setting<bool>(PRS::Acceleration::kEnableDynamic)) {
            if (region_type == RegionType::kPerimeter) {
                rv += "M204 P" %
                      QString::number(
                          m_sb->setting<Acceleration>(PRS::Acceleration::kPerimeter).to(m_meta.m_acceleration_unit)) %
                      commentSpaceLine("UPDATE ACCELERATION");
            }
            else if (region_type == RegionType::kInset) {
                rv += "M204 P" %
                      QString::number(
                          m_sb->setting<Acceleration>(PRS::Acceleration::kInset).to(m_meta.m_acceleration_unit)) %
                      commentSpaceLine("UPDATE ACCELERATION");
            }
            else if (region_type == RegionType::kSkeleton) {
                rv += "M204 P" %
                      QString::number(
                          m_sb->setting<Acceleration>(PRS::Acceleration::kSkeleton).to(m_meta.m_acceleration_unit)) %
                      commentSpaceLine("UPDATE ACCELERATION");
            }
            else if (region_type == RegionType::kSkin) {
                rv += "M204 P" %
                      QString::number(
                          m_sb->setting<Acceleration>(PRS::Acceleration::kSkin).to(m_meta.m_acceleration_unit)) %
                      commentSpaceLine("UPDATE ACCELERATION");
            }
            else if (region_type == RegionType::kInfill) {
                rv += "M204 P" %
                      QString::number(
                          m_sb->setting<Acceleration>(PRS::Acceleration::kInfill).to(m_meta.m_acceleration_unit)) %
                      commentSpaceLine("UPDATE ACCELERATION");
            }
            else if (region_type == RegionType::kSupport) {
                rv += "M204 P" %
                      QString::number(
                          m_sb->setting<Acceleration>(PRS::Acceleration::kSupport).to(m_meta.m_acceleration_unit)) %
                      commentSpaceLine("UPDATE ACCELERATION");
            }
            else {
                rv += "M204 P" %
                      QString::number(
                          m_sb->setting<Acceleration>(PRS::Acceleration::kDefault).to(m_meta.m_acceleration_unit)) %
                      commentSpaceLine("UPDATE ACCELERATION");
            }
        }

        if (m_filament_location < 0)
            rv += writePrime();
    }

    rv += m_G1;
    // update feedrate if needed
    if (getFeedrate() != speed || m_layer_start) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
        m_layer_start = false;
    }

    // writes XYZ to destination
    rv += writeCoordinates(target_point);

    // calculate and write E value if path is an extrusion path
    if (path_modifiers != PathModifiers::kCoasting && path_modifiers != PathModifiers::kForwardTipWipe &&
        path_modifiers != PathModifiers::kPerimeterTipWipe && path_modifiers != PathModifiers::kReverseTipWipe &&
        path_modifiers != PathModifiers::kSpiralLift) {

        // Set extrusion multiplier, or use default value of 1.0
        double current_multiplier;
        if (region_type == RegionType::kPerimeter)
            current_multiplier = m_sb->setting<double>(PS::Perimeter::kExtrusionMultiplier);
        else if (region_type == RegionType::kInset)
            current_multiplier = m_sb->setting<double>(PS::Inset::kExtrusionMultiplier);
        else if (region_type == RegionType::kSkeleton)
            current_multiplier = m_sb->setting<double>(PS::Skeleton::kExtrusionMultiplier);
        else if (region_type == RegionType::kSkin)
            current_multiplier = m_sb->setting<double>(PS::Skin::kExtrusionMultiplier);
        else if (region_type == RegionType::kInfill)
            current_multiplier = m_sb->setting<double>(PS::Infill::kExtrusionMultiplier);
        else
            current_multiplier = m_sb->setting<double>(PS::Perimeter::kExtrusionMultiplier);

        Distance segment_length = start_point.distance(target_point);
        Distance width = params->setting<Distance>(SS::kWidth);
        Distance height = params->setting<Distance>(SS::kHeight);
        Distance filament_diameter = m_sb->setting<Distance>(MS::Filament::kDiameter);
        Distance segment_filament_length =
            (segment_length * width * height) / ((filament_diameter / 2) * (filament_diameter / 2) * 3.14159);
        segment_filament_length *= current_multiplier;

        // if using relative E, write out segment filament length and be done
        if (m_sb->setting<bool>(MS::Filament::kRelative)) {
            m_filament_location = segment_filament_length;
            rv += m_e % QString::number(Distance(segment_filament_length).to(m_meta.m_distance_unit), 'f', 4);
        }
        // if using absolute E, update total length and write value
        else {
            m_filament_location += segment_filament_length;
            rv += m_e % QString::number(Distance(m_filament_location).to(m_meta.m_distance_unit), 'f', 4);
        }
    }

    // add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    m_first_print = false;

    return rv;
}

QString RepRapWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                               const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    QString rv;

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    int rpm = params->setting<int>(SS::kExtruderSpeed);
    int material_number = params->setting<int>(SS::kMaterialNumber);
    auto region_type = params->setting<RegionType>(SS::kRegionType);
    auto path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);

    rv += ((ccw) ? m_G3 : m_G2);

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

    // calculate and write E value if path is an extrusion path
    if (path_modifiers != PathModifiers::kCoasting && path_modifiers != PathModifiers::kForwardTipWipe &&
        path_modifiers != PathModifiers::kPerimeterTipWipe && path_modifiers != PathModifiers::kReverseTipWipe &&
        path_modifiers != PathModifiers::kSpiralLift) {

        // Set extrusion multiplier, or use default value of 1.0
        double current_multiplier;
        if (region_type == RegionType::kPerimeter)
            current_multiplier = m_sb->setting<double>(PS::Perimeter::kExtrusionMultiplier);
        else if (region_type == RegionType::kInset)
            current_multiplier = m_sb->setting<double>(PS::Inset::kExtrusionMultiplier);
        else if (region_type == RegionType::kSkeleton)
            current_multiplier = m_sb->setting<double>(PS::Skeleton::kExtrusionMultiplier);
        else if (region_type == RegionType::kSkin)
            current_multiplier = m_sb->setting<double>(PS::Skin::kExtrusionMultiplier);
        else if (region_type == RegionType::kInfill)
            current_multiplier = m_sb->setting<double>(PS::Infill::kExtrusionMultiplier);
        else
            current_multiplier = m_sb->setting<double>(PS::Perimeter::kExtrusionMultiplier);

        Distance segment_length = ArcSegment(start_point, end_point, center_point, ccw).length();
        Distance width = params->setting<Distance>(SS::kWidth);
        Distance height = params->setting<Distance>(SS::kHeight);
        Distance filament_diameter = m_sb->setting<Distance>(MS::Filament::kDiameter);
        Distance segment_filament_length =
            (segment_length * width * height) / ((filament_diameter / 2) * (filament_diameter / 2) * 3.14159);
        segment_filament_length *= current_multiplier;

        // if using relative E, write out segment filament length and be done
        if (m_sb->setting<bool>(MS::Filament::kRelative)) {
            m_filament_location = segment_filament_length;
            rv += m_e % QString::number(segment_filament_length.to(m_meta.m_distance_unit));
        }
        // if using absolute E, update total length and write value
        else {
            m_filament_location += segment_filament_length;
            rv += m_e % QString::number(m_filament_location.to(m_meta.m_distance_unit));
        }
    }

    // Add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    return rv;
}

QString RepRapWriter::writeAfterPath(RegionType type) {
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

QString RepRapWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString RepRapWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString RepRapWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString RepRapWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString RepRapWriter::writeShutdown() {
    QString rv;

    rv += m_sb->setting<QString>(PRS::GCode::kEndCode) % m_newline % "M104 S0" % commentSpaceLine("TURN EXTRUDER OFF") %
          "M140 S0" % commentSpaceLine("TURN BED OFF") % "M84" % commentSpaceLine("DISABLE MOTORS") % "M106 S0" %
          commentSpaceLine("DISABLE COOLING FAN");

    return rv;
}

QString RepRapWriter::writePurge(int RPM, int duration, int delay) { return {}; }

QString RepRapWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString RepRapWriter::writeCoordinates(Point destination) {
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

QString RepRapWriter::writeRetraction() {
    QString rv;

    if (m_filament_location >= 0 && m_sb->setting<bool>(MS::Retraction::kEnable)) {
        if ((m_filament_location != 0) && !m_sb->setting<bool>(MS::Filament::kRelative))
            rv += "G92 E0" % commentSpaceLine("RESET FILAMENT TO 0");
        m_filament_location = 0.0;

        rv += m_G1 % m_f % QString::number(m_sb->setting<Velocity>(MS::Retraction::kSpeed).to(m_meta.m_velocity_unit)) %
              m_e % QString::number(m_sb->setting<Distance>(MS::Retraction::kLength).to(m_meta.m_distance_unit) * -1) %
              commentSpaceLine("RETRACT FILAMENT");

        if (m_sb->setting<bool>(MS::Filament::kRelative))
            m_filament_location = -1 * m_sb->setting<Distance>(MS::Retraction::kLength);
        else
            m_filament_location -= m_sb->setting<Distance>(MS::Retraction::kLength);
    }

    return rv;
}

QString RepRapWriter::writePrime() {
    QString rv;

    if (m_sb->setting<bool>(MS::Filament::kRelative)) {
        m_filament_location = m_sb->setting<Distance>(MS::Retraction::kLength) +
                              m_sb->setting<Distance>(MS::Retraction::kPrimeAdditionalLength);
        rv += m_G1 % m_f %
              QString::number(m_sb->setting<Velocity>(MS::Retraction::kPrimeSpeed).to(m_meta.m_velocity_unit)) % m_e %
              QString::number(m_filament_location.to(m_meta.m_distance_unit)) % commentSpaceLine("PRIME FILAMENT");
    }
    else {
        m_filament_location += (m_sb->setting<Distance>(MS::Retraction::kLength) +
                                m_sb->setting<Distance>(MS::Retraction::kPrimeAdditionalLength));
        rv += m_G1 % m_f %
              QString::number(m_sb->setting<Velocity>(MS::Retraction::kPrimeSpeed).to(m_meta.m_velocity_unit)) % m_e %
              QString::number(m_filament_location.to(m_meta.m_distance_unit)) % commentSpaceLine("PRIME FILAMENT");
    }
    return rv;
}

QString RepRapWriter::setTools(QVector<int> extruders) {
    QString rv = "";
    return rv;
}
} // namespace ORNL
