#include "gcode/writers/marlin_pellet_writer.h"

#include <QStringBuilder>
#include <qcontainerfwd.h>
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
MarlinPelletWriter::MarlinPelletWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {
    int num_extruders = m_sb->setting<int>(ES::MultiNozzle::kNozzleCount);
    m_extruders_active.resize(num_extruders); // sets vector size
}

QString MarlinPelletWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x,
                                              Distance maximum_y, int num_layers) {
    m_current_z = m_sb->setting<Distance>(PRS::Dimensions::kZOffset);
    m_current_rpm = 0;
    m_current_bead_area = 0;
    for (int i = 0, end = m_extruders_on.size(); i < end; ++i) // all extruders initially off, and inactive
    {
        m_extruders_on[i] = false;
        m_extruders_active[i] = false;
    }
    m_extruders_active[0] = true; // extruder 0 is active by default
    m_material_number = -1;
    m_first_print = true;
    m_first_travel = true;
    m_layer_start = true;
    m_min_z = 0.0f;
    QString rv;
    if (m_sb->setting<int>(PRS::GCode::kEnableStartupCode)) {
        rv += "G29" % commentSpaceLine("ENABLE BED COMPENSATION");
        rv += "T0\n";
        rv += m_newline;
        if (m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight))
            rv += "M102 S1" % commentSpaceLine("USE BEAD AREA MODE");
        else
            rv += "M102 S0" % commentSpaceLine("USE RPM MODE");
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

    rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

    return rv;
}

QString MarlinPelletWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(PS::SpecialModes::kEnableSpiralize);
    m_layer_start = true;
    QString rv;
    return rv;
}

QString MarlinPelletWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString MarlinPelletWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString MarlinPelletWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
}

QString MarlinPelletWriter::writeBeforePath(RegionType type) {
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

QString MarlinPelletWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                        QSharedPointer<SettingsBase> params) {
    QString rv;
    Velocity speed = params->setting<Velocity>(SS::kSpeed);

    m_current_bead_area = 0;

    bool any_extr_on = false;
    for (int i = 0, end = m_extruders_on.size(); i < end; ++i)
        any_extr_on = any_extr_on || m_extruders_on[i];

    if (any_extr_on)
        rv += writeExtruderOff();

    Point new_start_location;

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

    // travel_lift vector in direction normal to the layer
    // with length = lift height as defined in settings
    QVector3D travel_lift = getTravelLift();

    // write the lift
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        Point lift_destination = new_start_location + travel_lift; // lift destination is above start location
        rv += m_G1 % " F" %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              writeCoordinates(lift_destination) % commentSpaceLine("TRAVEL LIFT Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    // write the travel
    Point travel_destination = target_location;
    if (travel_lift_required)
        travel_destination = travel_destination + travel_lift; // travel destination is above the target point

    rv += m_G1 % " F" % QString::number(speed.to(m_meta.m_velocity_unit)) % writeCoordinates(travel_destination) %
          commentSpaceLine("TRAVEL");
    setFeedrate(speed);

    // write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G1 % " F" %
              QString::number(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed).to(m_meta.m_velocity_unit)) %
              writeCoordinates(target_location) % commentSpaceLine("TRAVEL LOWER Z");
        setFeedrate(m_sb->setting<Velocity>(PRS::MachineSpeed::kZSpeed));
    }

    if (m_first_travel)         // if this is the first travel
        m_first_travel = false; // update for next one

    return rv;
}

QString MarlinPelletWriter::writeLine(const Point& start_point, const Point& target_point,
                                      const QSharedPointer<SettingsBase> params) {
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    int rpm = params->setting<int>(SS::kExtruderSpeed);
    int material_number = params->setting<int>(SS::kMaterialNumber);
    RegionType region_type = params->setting<RegionType>(SS::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    float output_rpm = rpm * m_sb->setting<float>(PRS::MachineSpeed::kGearRatio);
    Distance width = params->setting<Distance>(SS::kWidth);
    Distance height = params->setting<Distance>(SS::kHeight);
    Area bead_area = (width - height) * height +
                     (pi() * (height / 2) * (height / 2)); // Rectangle with two half circles used as cross-section

    QString rv;

    // change tools if necessary
    QString tool_change = setTools(params->setting<QVector<int>>(SS::kExtruders));
    if (tool_change.length() > 0) // if the tools need to change
    {
        rv += writeExtruderOff(); // write extruder off before tool changes
        rv += tool_change;
    }

    // Update the material number if necessary
    if (material_number != m_material_number && m_sb->setting<int>(MS::MultiMaterial::kEnable) &&
        !m_sb->setting<bool>(ES::MultiNozzle::kEnableMultiNozzleMultiMaterial)) {
        rv += "T" % QString::number(material_number) % commentSpaceLine("SET ACTIVE EXTRUDER");
        m_material_number = material_number;
    }

    // determine if writeExtruderOn is necessary
    bool requiresWriteExtruderOn = false;
    for (int i = 0, end = m_extruders_active.size(); i < end; ++i) {
        // if an extruder is active and off, need to write extruders on
        // (should happen after tool changes or travels)
        if (m_extruders_active[i] && !m_extruders_on[i])
            requiresWriteExtruderOn = true;
    }

    if (requiresWriteExtruderOn && rpm > 0) // && !m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight))
    {
        rv += writeExtruderOn(region_type, rpm, width, height, bead_area);
        // m_current_rpm = rpm;
    }

    // RPM and Bead Area updates must happen via m3/m5 and cannot be processed in-line with G1
    if (rpm != m_current_rpm && rpm != 0 && !m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight)) {
        rv += m_M3 % m_s % QString::number(output_rpm) % commentSpaceLine("UPDATE EXTRUDER RPM");
        m_current_rpm = rpm;
    }
    else if (rpm != m_current_rpm && rpm == 0) // && !m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight))
    {
        rv += writeExtruderOff();
    }
    else if (m_current_bead_area != bead_area && m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight) && rpm > 0) {
        rv +=
            m_M3 % m_s %
            QString::number((Distance(width).to(m_meta.m_distance_unit) - Distance(height).to(m_meta.m_distance_unit)) *
                                Distance(height).to(m_meta.m_distance_unit) +
                            (pi() * (Distance(height).to(m_meta.m_distance_unit) / 2) *
                             (Distance(height).to(m_meta.m_distance_unit) / 2))) %
            commentSpaceLine("UPDATE BEAD AREA");
        m_current_bead_area = bead_area;
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

    // add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone)
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    else
        rv += commentSpaceLine(toString(region_type));

    m_first_print = false;

    return rv;
}

QString MarlinPelletWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                                     const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    QString rv;

    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    int rpm = params->setting<int>(SS::kExtruderSpeed);
    int material_number = params->setting<int>(SS::kMaterialNumber);
    auto region_type = params->setting<RegionType>(SS::kRegionType);
    auto path_modifiers = params->setting<PathModifiers>(SS::kPathModifiers);
    Distance width = params->setting<Distance>(SS::kWidth);
    Distance height = params->setting<Distance>(SS::kHeight);
    Area bead_area = (width - height) * height +
                     (pi() * (height / 2) * (height / 2)); // Rectangle with two half circles used as cross-section

    // change tools if necessary
    QString tool_change = setTools(params->setting<QVector<int>>(SS::kExtruders));
    if (tool_change.length() > 0) // if the tools need to change
    {
        rv += writeExtruderOff(); // write extruder off before tool changes
        rv += tool_change;
    }

    // Update the material number if necessary
    if (material_number != m_material_number && m_sb->setting<int>(MS::MultiMaterial::kEnable) &&
        !m_sb->setting<bool>(ES::MultiNozzle::kEnableMultiNozzleMultiMaterial)) {
        rv += "T" % QString::number(material_number) % commentSpaceLine("SET ACTIVE EXTRUDER");
        m_material_number = material_number;
    }

    // determine if writeExtruderOn is necessary
    bool requiresWriteExtruderOn = false;
    for (int i = 0, end = m_extruders_active.size(); i < end; ++i) {
        // if an extruder is active and off, need to write extruders on
        // (should happen after tool changes or travels)
        if (m_extruders_active[i] && !m_extruders_on[i])
            requiresWriteExtruderOn = true;
    }

    if (!requiresWriteExtruderOn && rpm > 0) {
        rv += writeExtruderOn(region_type, rpm, width, height, bead_area);
    }

    rv += ((ccw) ? m_G3 : m_G2);

    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }
    if (rpm != m_current_rpm) {
        rv += m_s % QString::number(rpm);
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

QString MarlinPelletWriter::writeAfterPath(RegionType type) {
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

QString MarlinPelletWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString MarlinPelletWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString MarlinPelletWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString MarlinPelletWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(PRS::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString MarlinPelletWriter::writeShutdown() {
    QString rv;
    rv += "M5" % commentSpaceLine("TURN EXTRUDER OFF END OF PRINT") % "M104 S0 T0" %
          commentSpaceLine("TURN EXTRUDER 1 OFF");
    if (m_sb->setting<bool>(ES::MultiNozzle::kEnableMultiNozzleMultiMaterial)) {
        rv += "M104 S0 T1" % commentSpaceLine("TURN EXTRUDER 2 OFF");
    }

    rv +=
        "M140 S0" % commentSpaceLine("TURN HEATED PLATEN OFF") % "M141 S0" % commentSpaceLine("TURN PRINT CHAMBER OFF");

    rv += m_sb->setting<QString>(PRS::GCode::kEndCode) % m_newline;
    return rv;
}

QString MarlinPelletWriter::writePurge(int RPM, int duration, int delay) { return {}; }

QString MarlinPelletWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString MarlinPelletWriter::writeExtruderOn(RegionType type, int rpm, Distance width, Distance height, Area bead_area) {
    // extruder on/off commands only affect extruder marked active by a tool change
    for (int i = 0, end = m_extruders_active.size(); i < end; ++i) {
        if (m_extruders_active[i])
            m_extruders_on[i] = true;
    }
    QString rv;

    if (!m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight)) {
        float output_rpm;

        output_rpm =
            m_sb->setting<float>(PRS::MachineSpeed::kGearRatio) * m_sb->setting<int>(MS::Extruder::kInitialSpeed);

        if (m_sb->setting<int>(MS::Extruder::kInitialSpeed) > 0) {
            output_rpm =
                m_sb->setting<float>(PRS::MachineSpeed::kGearRatio) * m_sb->setting<int>(MS::Extruder::kInitialSpeed);

            // Only update the current rpm if not using feedrate scaling. An updated rpm value here could prevent the S
            // parameter from being issued during the first G1 motion of the path and thus the extruder rate won't
            // properly scale
            if (!(m_sb->setting<int>(MS::Cooling::kForceMinLayerTime) &&
                  m_sb->setting<int>(MS::Cooling::kForceMinLayerTimeMethod) ==
                      (int)ForceMinimumLayerTime::kSlow_Feedrate))
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
            // parameter from being issued during the first G1 motion of the path and thus the extruder rate won't
            // properly scale
            if (!(m_sb->setting<int>(MS::Cooling::kForceMinLayerTime) &&
                  m_sb->setting<int>(MS::Cooling::kForceMinLayerTimeMethod) ==
                      (int)ForceMinimumLayerTime::kSlow_Feedrate))
                m_current_rpm = rpm;
        }

        if (m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight)) {
            rv += "M102 S1" % commentSpaceLine("RE-ENABLE BEAD AREA TO BEGIN MOTION");
        }

        // output_rpm = m_sb->setting< float >(PRS::MachineSpeed::kGearRatio) * rpm;
        // rv += m_M3 % m_s % QString::number(output_rpm) % commentSpaceLine("UPDATE EXTRUDER RPM");
    }
    else {
        rv +=
            m_M3 % m_s %
            QString::number((Distance(width).to(m_meta.m_distance_unit) - Distance(height).to(m_meta.m_distance_unit)) *
                                Distance(height).to(m_meta.m_distance_unit) +
                            (pi() * (Distance(height).to(m_meta.m_distance_unit) / 2) *
                             (Distance(height).to(m_meta.m_distance_unit) / 2)));
        rv += commentSpaceLine("SET BEAD AREA");
        m_current_bead_area = bead_area;
        m_current_rpm = rpm;
    }

    return rv;
}

QString MarlinPelletWriter::writeExtruderOff() {
    // extruder on/off commands only affect extruder marked active by a tool change
    for (int i = 0, end = m_extruders_active.size(); i < end; ++i) {
        if (m_extruders_active[i])
            m_extruders_on[i] = false;
    }

    QString rv;

    if (m_sb->setting<Time>(MS::Extruder::kOffDelay) > 0) {
        rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOffDelay));
    }
    rv += m_M5 % commentSpaceLine("TURN EXTRUDER OFF");

    m_current_rpm = 0;
    m_current_bead_area = 0;

    return rv;
}

QString MarlinPelletWriter::writeCoordinates(Point destination) {
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

QString MarlinPelletWriter::setTools(QVector<int> extruders) {

    QString rv = "";
    if (m_extruders_active.size() > 2) // assumes that simultaneous extruders are ext0 and ext1
    {
        if (m_extruders_active[0] && m_extruders_active[1] &&
            extruders.length() < 2) // currently both on, need single ext
        {
            // turn off both
            rv += "M605 S0" % m_newline;
            m_extruders_active[0] = false;
            m_extruders_active[1] = false;
        }

        if (extruders.length() > 1 &&
            !(m_extruders_active[0] && m_extruders_active[1])) // need both exts. on & not already on
        {
            // turn on both
            rv += "M605 S2" % m_newline;
            m_extruders_active[0] = true;
            m_extruders_active[1] = true;
        }
    }

    // zero or one ext. need to be on
    for (int i = 0, end = m_extruders_active.size(); i < end; ++i) {
        if (!m_extruders_active[i] && extruders.contains(i)) // ext isn't active, needs to be
        {
            // write tool change to zero
            rv += "T" % QString::number(i) % m_newline;
            m_extruders_active[i] = true;
        }
        else if (m_extruders_active[i] && !extruders.contains(i)) // ext is active, needs to not be
        {
            m_extruders_active[i] = false;
        }
        // else - the extruder was already correctly marked active/inactive
    }
    return rv;
}
} // namespace ORNL
