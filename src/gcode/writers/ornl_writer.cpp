#include "gcode/writers/ornl_writer.h"

#include "utilities/enums.h"
#include "utilities/mathutils.h"

#include <QStringBuilder>

namespace ORNL {
ORNLWriter::ORNLWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString ORNLWriter::writeSettingsHeader(GcodeSyntax syntax) {
    QString text = "";
    text += commentLine("Slicing Parameters");
    WriterBase::writeSettingsHeader(syntax);

    text += m_newline;
    return text;
}

QString ORNLWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                      int num_layers) {
    m_current_z = m_sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kZOffset);
    m_current_rpm = 0;
	m_current_robot = 1;
    m_machine_type = m_sb->setting<MachineType>(Constants::PrinterSettings::MachineSetup::kMachineType);
    for (int ext = 0, end = m_extruders_on.size(); ext < end; ++ext) // all extruders off initially
        m_extruders_on[ext] = false;
    m_first_travel = true;
    m_first_print = true;
    m_layer_start = true;
    m_min_z = 0.0f;
    QString rv;
    if (m_sb->setting<int>(Constants::PrinterSettings::GCode::kEnableStartupCode)) {
        rv += commentLine("SAFETY BLOCK - ESTABLISH OPERATIONAL MODES");
        rv += "G1 F120 " % commentLine("SET INITIAL FEEDRATE");
        if (m_sb->setting<int>(Constants::PrinterSettings::GCode::kEnableWaitForUser)) {
            rv += "M0" % commentSpaceLine("WAIT FOR USER");
        }
        rv += writeDwell(0.25);
    }

    if (m_sb->setting<int>(Constants::PrinterSettings::GCode::kEnableBoundingBox)) {
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

    if (m_sb->setting<int>(Constants::PrinterSettings::GCode::kEnableMaterialLoad)) {
        rv += writePurge(m_sb->setting<int>(Constants::MaterialSettings::Purge::kInitialScrewRPM),
                         m_sb->setting<int>(Constants::MaterialSettings::Purge::kInitialDuration),
                         m_sb->setting<int>(Constants::MaterialSettings::Purge::kInitialTipWipeDelay));
        if (m_sb->setting<int>(Constants::PrinterSettings::GCode::kEnableWaitForUser)) {
            rv += "M0" % commentSpaceLine("WAIT FOR USER");
        }
    }

    if (m_sb->setting<QString>(Constants::PrinterSettings::GCode::kStartCode) != "")
        rv += m_sb->setting<QString>(Constants::PrinterSettings::GCode::kStartCode);

    rv += m_newline;

    rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

    return rv;
}

QString ORNLWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    m_spiral_layer = sb->setting<bool>(Constants::ProfileSettings::SpecialModes::kEnableSpiralize);
    m_layer_start = true;
    QString rv;
    rv += "M01 " % commentLine("OPTIONAL STOP - LAYER CHANGE");
    return rv;
}

QString ORNLWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
}

QString ORNLWriter::writeBeforeIsland() {
    QString rv;
    return rv;
}

QString ORNLWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    m_current_type = type;
    if ((!m_spiral_layer || m_first_print) && m_machine_type == MachineType::kWire_Arc)
    {
        if (type == RegionType::kPerimeter)
        {
            rv += "M1001" % commentSpaceLine("ROBOT 1 - MIG");
			m_current_robot = 1;
        }
        else if (type == RegionType::kInset)
        {
            rv += "M1001" % commentSpaceLine("ROBOT 1 - MIG");
			m_current_robot = 1;
        }
        else if (type == RegionType::kSkeleton)
        {
            rv += "M1001" % commentSpaceLine("ROBOT 1 - MIG");
			m_current_robot = 1;
        }
        else if (type == RegionType::kSkin)
        {
            rv += "M1002" % commentSpaceLine("ROBOT 2 - ELECTRO-SLAG");
			m_current_robot = 2;
        }
        else if (type == RegionType::kInfill)
        {
            rv += "M1002" % commentSpaceLine("ROBOT 2 - ELECTRO-SLAG");
			m_current_robot = 2;
        }
        else if (type == RegionType::kSupport)
        {
            rv += "M1001" % commentSpaceLine("ROBOT 1 - MIG");
			m_current_robot = 1;
        }
        else {}
    }
    return rv;
}

QString ORNLWriter::writeBeforePath(RegionType type) {
    QString rv;
    m_current_type = type;
    if (!m_spiral_layer || m_first_print) {
        if (type == RegionType::kPerimeter) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kPerimeterStart).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kPerimeterStart) % m_newline;
        }
        else if (type == RegionType::kInset) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInsetStart).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInsetStart) % m_newline;
        }
        else if (type == RegionType::kSkeleton) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkeletonStart).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkeletonStart) % m_newline;
        }
        else if (type == RegionType::kSkin) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkinStart).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkinStart) % m_newline;
        }
        else if (type == RegionType::kInfill) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInfillStart).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInfillStart) % m_newline;
        }
        else if (type == RegionType::kSupport) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSupportStart).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSupportStart) % m_newline;
        }
        else {}
    }
    return rv;
}

QString ORNLWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                QSharedPointer<SettingsBase> params) {
    QString rv;

    Point new_start_location;
    RegionType rType = params->setting<RegionType>(Constants::SegmentSettings::kRegionType);

    // travel_lift vector in direction normal to the layer
    // with length = lift height as defined in settings
    QVector3D travel_lift = getTravelLift();

    // Use updated start location if this is the first travel
    if (m_first_travel)
        new_start_location = m_start_point;
    else
        new_start_location = start_location;

    Distance liftDist;
    liftDist = m_sb->setting<Distance>(Constants::ProfileSettings::Travel::kLiftHeight);

    bool travel_lift_required = liftDist > 0; // && !m_first_travel; //do not write a lift on first travel

    // Don't lift for short travel moves
    if (start_location.distance(target_location) <
        m_sb->setting<Distance>(Constants::ProfileSettings::Travel::kMinTravelForLift)) {
        travel_lift_required = false;
    }   

    // write the lift
    if (travel_lift_required && !m_first_travel &&
        (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly)) {
        Point lift_destination = new_start_location + travel_lift; // lift destination is above start location

        rv += m_G0 % writeCoordinates(lift_destination) % commentSpaceLine("TRAVEL LIFT Z");
        setFeedrate(m_sb->setting<Velocity>(Constants::PrinterSettings::MachineSpeed::kZSpeed));
    }

    // write the travel
    Point travel_destination = target_location;
    if (m_first_travel)
        travel_destination.z(qAbs(m_sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kZOffset)() +
                                  m_sb->setting<Distance>(Constants::ProfileSettings::Travel::kLiftHeight)() +
                                  m_sb->setting<Distance>(Constants::ProfileSettings::Layer::kLayerHeight)()));
    else if (travel_lift_required)
        travel_destination = travel_destination + travel_lift; // travel destination is above the target point

    rv += m_G0 % writeCoordinates(travel_destination) % commentSpaceLine("TRAVEL");
    setFeedrate(m_sb->setting<Velocity>(Constants::ProfileSettings::Travel::kSpeed));

    if (m_first_travel)         // if this is the first travel
        m_first_travel = false; // update for next one

    // write the travel lower (undo the lift)
    if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly)) {
        rv += m_G0 % writeCoordinates(target_location) % commentSpaceLine("TRAVEL LOWER Z");
        setFeedrate(m_sb->setting<Velocity>(Constants::PrinterSettings::MachineSpeed::kZSpeed));
    }

    return rv;
}

QString ORNLWriter::writeLine(const Point& start_point, const Point& target_point,
                              const QSharedPointer<SettingsBase> params) {
    // Get the settings
    Velocity speed = params->setting<Velocity>(Constants::SegmentSettings::kSpeed);
    int rpm = params->setting<int>(Constants::SegmentSettings::kExtruderSpeed);
    int material_number = params->setting<int>(Constants::SegmentSettings::kMaterialNumber);
    RegionType region_type = params->setting<RegionType>(Constants::SegmentSettings::kRegionType);
    PathModifiers path_modifiers = params->setting<PathModifiers>(Constants::SegmentSettings::kPathModifiers);
    float output_rpm = rpm * m_sb->setting<float>(Constants::PrinterSettings::MachineSpeed::kGearRatio);

    QString rv;

    for (int extruder : params->setting<QVector<int>>(Constants::SegmentSettings::kExtruders)) {
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
    if (m_layer_start && m_machine_type != MachineType::kWire_Arc) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
        if (m_machine_type != MachineType::kWire_Arc) {
            rv += m_s % QString::number(output_rpm);
        }

        m_current_rpm = rpm;

        m_layer_start = false;
    }

    // Update feedrate and extruder speed if needed - wire arc always requires speed
    if (getFeedrate() != speed || m_machine_type == MachineType::kWire_Arc) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }

    if (rpm != m_current_rpm && m_machine_type != MachineType::kWire_Arc) {
        rv += m_s % QString::number(output_rpm);
        m_current_rpm = rpm;
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

QString ORNLWriter::writeArc(const Point& start_point, const Point& end_point, const Point& center_point,
                             const Angle& angle, const bool& ccw, const QSharedPointer<SettingsBase> params) {
    // Return value
    QString rv;

    // Get the settings
    Velocity speed = params->setting<Velocity>(Constants::SegmentSettings::kSpeed);
    int rpm = params->setting<int>(Constants::SegmentSettings::kExtruderSpeed);
    int material_number = params->setting<int>(Constants::SegmentSettings::kMaterialNumber);
    auto region_type = params->setting<RegionType>(Constants::SegmentSettings::kRegionType);
    auto path_modifiers = params->setting<PathModifiers>(Constants::SegmentSettings::kPathModifiers);
    float output_rpm = rpm * m_sb->setting<float>(Constants::PrinterSettings::MachineSpeed::kGearRatio);
    Distance z_offset = m_sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kZOffset);

    for (int extruder : params->setting<QVector<int>>(Constants::SegmentSettings::kExtruders)) {
        // turn on the extruder if it isn't already on
        if (m_extruders_on[0] == false && rpm > 0) {
            // only check first extruder
            rv += writeExtruderOn(region_type, rpm, extruder);
        }
    }

    rv += ((ccw) ? m_G3 : m_G2);

    if (getFeedrate() != speed) {
        setFeedrate(speed);
        rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit));
    }

    if (rpm != m_current_rpm && m_machine_type != MachineType::kWire_Arc) {
        rv += m_s % QString::number(rpm);
        m_current_rpm = rpm;
    }

    rv += m_i % QString::number(Distance(center_point.x() - start_point.x()).to(m_meta.m_distance_unit), 'f', 4) % m_j %
          QString::number(Distance(center_point.y() - start_point.y()).to(m_meta.m_distance_unit), 'f', 4) % m_x %
          QString::number(Distance(end_point.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(end_point.y()).to(m_meta.m_distance_unit), 'f', 4);

    // write vertical coordinate along the correct axis (Z or W) according to printer settings
    // only output Z/W coordinate if there was a change in Z/W
    Distance target_z = end_point.z() + z_offset;
    if (qAbs(target_z - m_last_z) > 10) {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z = target_z;
    }

    // Add comment for gcode parser
    if (path_modifiers != PathModifiers::kNone) {
        rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
    }
    else {
        rv += commentSpaceLine(toString(region_type));
    }

    return rv;
}

QString ORNLWriter::writeAfterPath(RegionType type) {
    QString rv;
    if (!m_spiral_layer) {
        rv += writeExtruderOff(0); // update to turn off appropriate extruders
        if (type == RegionType::kPerimeter) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kPerimeterEnd).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kPerimeterEnd) % m_newline;
        }
        else if (type == RegionType::kInset) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInsetEnd).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInsetEnd) % m_newline;
        }
        else if (type == RegionType::kSkeleton) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkeletonEnd).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkeletonEnd) % m_newline;
        }
        else if (type == RegionType::kSkin) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkinEnd).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSkinEnd) % m_newline;
        }
        else if (type == RegionType::kInfill) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInfillEnd).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kInfillEnd) % m_newline;
        }
        else if (type == RegionType::kSupport) {
            if (!m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSupportEnd).isEmpty())
                rv += m_sb->setting<QString>(Constants::ProfileSettings::GCode::kSupportEnd) % m_newline;
        }
    }
    return rv;
}

QString ORNLWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
}

QString ORNLWriter::writeAfterIsland() {
    QString rv;
    return rv;
}

QString ORNLWriter::writeAfterPart() {
    QString rv;
    return rv;
}

QString ORNLWriter::writeAfterLayer() {
    QString rv;
    rv += m_sb->setting<QString>(Constants::PrinterSettings::GCode::kLayerCodeChange) % m_newline;
    return rv;
}

QString ORNLWriter::writeShutdown() {
    QString rv;
    if (m_sb->setting<MachineType>(Constants::PrinterSettings::MachineSetup::kMachineType) == MachineType::kPellet) {
        rv += m_M5 % commentSpaceLine("TURN EXTRUDER OFF END OF PRINT");
    }

    rv += m_sb->setting<QString>(Constants::PrinterSettings::GCode::kEndCode) % m_newline % "M02" %
          commentSpaceLine("END OF G-CODE");
    return rv;
}

QString ORNLWriter::writePurge(int RPM, int duration, int delay) {
    QString rv;
    return rv;
}

QString ORNLWriter::writeDwell(Time time) {
    if (time > 0)
        return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
    else
        return {};
}

QString ORNLWriter::writeExtruderOn(RegionType region_type, int rpm, int extruder_number) {
    QString rv;
    m_extruders_on[extruder_number] = true;
    float output_rpm;

    if (m_machine_type == MachineType::kWire_Arc)
    {
        if(m_current_type == RegionType::kInfill || m_current_type == RegionType::kSkin)
        {
            rv += "M200" % commentSpaceLine("STRIP CLAD ON");
            rv += "M202" % commentSpaceLine("STRIP FEED ON");
            rv += "M204" % commentSpaceLine("FLUX FEED ON");
        }
        else
        {
            rv += "M100" % commentSpaceLine("WIRE ARC WELDER ON");
            rv += "M102" % commentSpaceLine("WIRE FEED ON");
            rv += "M104" % commentSpaceLine("WIRE SHIELDING ON");
        }
    }
    else
    {
        // Retrieve relevant settings
        int initial_speed = m_sb->setting<int>(Constants::MaterialSettings::Extruder::kInitialSpeed);
        float gear_ratio = m_sb->setting<float>(Constants::PrinterSettings::MachineSpeed::kGearRatio);
        bool force_min_layer_time = m_sb->setting<bool>(Constants::MaterialSettings::Cooling::kForceMinLayerTime);
        ForceMinimumLayerTime force_min_layer_time_method =
            m_sb->setting<ForceMinimumLayerTime>(Constants::MaterialSettings::Cooling::kForceMinLayerTimeMethod);

        if (initial_speed > 0) {
            output_rpm = gear_ratio * initial_speed;

            // Only update the current rpm if not using feedrate scaling. An updated rpm value here could prevent the S
            // parameter from being issued during the first G1 motion of the path and thus the extruder rate won't
            // properly scale
            if (!(force_min_layer_time && force_min_layer_time_method == ForceMinimumLayerTime::kSlow_Feedrate)) {
                m_current_rpm = initial_speed;
            }

            rv += m_M3 % m_s % QString::number(output_rpm) % commentSpaceLine("TURN EXTRUDER ON");

            // Retrieve the appropriate dwell time for the region
            Time dwell_time = 0;
            switch (region_type) {
                case RegionType::kPerimeter:
                    dwell_time = m_sb->setting<Time>(Constants::MaterialSettings::Extruder::kOnDelayPerimeter);
                    break;
                case RegionType::kInset:
                    dwell_time = m_sb->setting<Time>(Constants::MaterialSettings::Extruder::kOnDelayInset);
                    break;
                case RegionType::kSkeleton:
                    dwell_time = m_sb->setting<Time>(Constants::MaterialSettings::Extruder::kOnDelaySkeleton);
                    break;
                case RegionType::kSkin:
                    dwell_time = m_sb->setting<Time>(Constants::MaterialSettings::Extruder::kOnDelaySkin);
                    break;
                case RegionType::kInfill:
                    dwell_time = m_sb->setting<Time>(Constants::MaterialSettings::Extruder::kOnDelayInfill);
                    break;
                default:
                    break;
            }

            // Write the appropriate dwell time for the region
            if (dwell_time > 0) {
                rv += writeDwell(dwell_time);
            }
        }
        else {
            output_rpm = gear_ratio * rpm;
            rv += m_M3 % m_s % QString::number(output_rpm) % commentSpaceLine("TURN EXTRUDER ON");

            // Only update the current rpm if not using feedrate scaling. An updated rpm value here could prevent the S
            // parameter from being issued during the first G1 motion of the path and thus the extruder rate won't
            // properly scale
            if (!(force_min_layer_time && force_min_layer_time_method == ForceMinimumLayerTime::kSlow_Feedrate)) {
                m_current_rpm = rpm;
            }
        }
    }

    return rv;
}

QString ORNLWriter::writeExtruderOff(int extruder_number) {
    QString rv;
    m_extruders_on[extruder_number] = false;

    // Retrieve relevant settings
    Time off_delay = m_sb->setting<Time>(Constants::MaterialSettings::Extruder::kOffDelay);

    if (m_machine_type == MachineType::kWire_Arc)
    {
        if(m_current_type == RegionType::kInfill || m_current_type == RegionType::kSkin)
        {
            rv += "M201" % commentSpaceLine("STRIP CLAD OFF");
            rv += "M203" % commentSpaceLine("STRIP FEED OFF");
            rv += "M205" % commentSpaceLine("FLUX FEED OFF");
        }
        else
        {
            rv += "M101" % commentSpaceLine("WIRE ARC WELDER OFF");
            rv += "M103" % commentSpaceLine("WIRE FEED OFF");
            rv += "M105" % commentSpaceLine("WIRE SHIELDING OFF");
        }
    }
    else if (off_delay > 0)
    {
        rv += writeDwell(off_delay);
    }

    m_current_rpm = 0;

    return rv;
}

QString ORNLWriter::writeCoordinates(Point destination) {
    QString rv;

    // always specify X and Y
    rv += m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) % m_y %
          QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4);

    // write vertical coordinate along the correct axis (Z or W) according to printer settings
    // only output Z/W coordinate if there was a change in Z/W
    Distance z_offset = m_sb->setting<Distance>(Constants::PrinterSettings::Dimensions::kZOffset);

    Distance target_z = destination.z() + z_offset;
    /*if (qAbs(target_z - m_last_z) > 10) {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z = target_z;
    }*/

    // always specify Z for Wire Arc
    if (m_machine_type == MachineType::kWire_Arc)
    {
        rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
        m_current_z = target_z;
        m_last_z = target_z;
    }
    else
    {
        if (qAbs(target_z - m_last_z) > 10) {
            rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
            m_current_z = target_z;
            m_last_z = target_z;
        }
    }
	
	if (m_machine_type == MachineType::kWire_Arc && m_current_robot == 1)
	{
		rv += " RX-180 RY0 RZ-135 PA0 PC0";
	}
	else if(m_machine_type == MachineType::kWire_Arc && m_current_robot == 2)
	{
		rv += " RX180 RY0 RZ90 PA0 PC0";
	}
	
    return rv;
}
} // namespace ORNL
