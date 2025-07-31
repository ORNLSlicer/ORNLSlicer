#include <QStringBuilder>

#include "gcode/writers/haas_writer.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL
{
    HaasWriter::HaasWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {

    }

    QString HaasWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y, int num_layers)
    {
        m_current_z = m_sb->setting< Distance >(Constants::PrinterSettings::Dimensions::kZOffset);
        m_current_w = m_sb->setting< Distance >(Constants::PrinterSettings::Dimensions::kWMax);
        m_current_rpm = 0;
        m_extruders_on[0] = false;
        m_first_travel = true;
        m_first_print = true;
        m_layer_start = true;
        m_min_z = 0.0f;
        m_material_number = -1;
        m_tool_number = m_sb->setting< int >(Constants::PrinterSettings::MachineSetup::kToolNumber);
        Distance tool_offset = 100000;
        QString rv;
        if (m_sb->setting< int >(Constants::PrinterSettings::GCode::kEnableStartupCode))
        {
            rv += "M06 T" % QString::number(m_tool_number) % m_newline %
                    "G90G00G54X0.Y0." % m_newline;
            //rv += "G43H2Z100." % m_newline;
            rv += "G43H" % QString::number(m_tool_number) % "Z" %
                QString::number(tool_offset.to(m_meta.m_distance_unit), 'f', 4);
        }

        if(m_sb->setting< int >(Constants::PrinterSettings::GCode::kEnableBoundingBox))
        {
            rv += "G0 Z0" % commentSpaceLine("RAISE Z TO DEMO BOUNDING BOX")
                % m_G0 % m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" % QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX")
                % m_G0 % m_x % QString::number(maximum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" % QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX")
                % m_G0 % m_x % QString::number(maximum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" % QString::number(maximum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX")
                % m_G0 % m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" % QString::number(maximum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX")
                % m_G0 % m_x % QString::number(minimum_x.to(m_meta.m_distance_unit), 'f', 4) % " Y" % QString::number(minimum_y.to(m_meta.m_distance_unit), 'f', 4) % commentSpaceLine("BOUNDING BOX")
                % "M0" % commentSpaceLine("WAIT FOR USER");

            m_start_point = Point(minimum_x, minimum_y, 0);
        }

        if(m_sb->setting< QString >(Constants::PrinterSettings::GCode::kStartCode) != "")
            rv += m_sb->setting< QString >(Constants::PrinterSettings::GCode::kStartCode);

        rv += m_newline;

        rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

        return rv;
    }

    QString HaasWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb)
    {
        m_spiral_layer = sb->setting<bool>(Constants::ProfileSettings::SpecialModes::kEnableSpiralize);
        m_layer_start = true;
        QString rv;
        return rv;
    }

    QString HaasWriter::writeBeforePart(QVector3D normal)
    {
        QString rv;
        return rv;
    }

    QString HaasWriter::writeBeforeIsland()
    {
        QString rv;
        return rv;
    }

    QString HaasWriter::writeBeforeRegion(RegionType type, int pathSize)
    {
        QString rv;
        return rv;
    }

    QString HaasWriter::writeBeforePath(RegionType type)
    {
        QString rv;
        if(!m_spiral_layer || m_first_print)
        {
            if (type == RegionType::kPerimeter)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kPerimeterStart).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kPerimeterStart) % m_newline;
            }
            else if (type == RegionType::kInset)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInsetStart).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInsetStart) % m_newline;
            }
            else if(type == RegionType::kSkeleton)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkeletonStart).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkeletonStart) % m_newline;
            }
            else if (type == RegionType::kSkin)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkinStart).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkinStart) % m_newline;
            }
            else if (type == RegionType::kInfill)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInfillStart).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInfillStart) % m_newline;
            }
            else if (type == RegionType::kSupport)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSupportStart).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSupportStart) % m_newline;
            }
        }
        return rv;
    }

    QString HaasWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                    QSharedPointer<SettingsBase> params)
    {
        QString rv;

        Point new_start_location;

        //Use updated start location if this is the first travel
        if(m_first_travel)
            new_start_location = m_start_point;
        else
            new_start_location = start_location;

        Distance liftDist;
        liftDist = m_sb->setting< Distance >(Constants::ProfileSettings::Travel::kLiftHeight);

        bool travel_lift_required = liftDist > 0;// && !m_first_travel; //do not write a lift on first travel

        //Don't lift for short travel moves
        if(start_location.distance(target_location) < m_sb->setting< Distance >(Constants::ProfileSettings::Travel::kMinTravelForLift))
        {
            travel_lift_required = false;
        }

        //travel_lift vector in direction normal to the layer
        //with length = lift height as defined in settings
        QVector3D travel_lift = getTravelLift();

        //write the lift
        if (travel_lift_required && !m_first_travel && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftUpOnly))
        {
            Point lift_destination = new_start_location + travel_lift; //lift destination is above start location
            rv += m_G0 % writeCoordinates(lift_destination) % commentSpaceLine("TRAVEL LIFT Z");
            setFeedrate(m_sb->setting< Velocity >(Constants::PrinterSettings::MachineSpeed::kZSpeed));
        }

        //write the travel
        Point travel_destination = target_location;
        if(m_first_travel)
            travel_destination.z(qAbs(m_sb->setting< Distance >(Constants::PrinterSettings::Dimensions::kZOffset)()));
        else if (travel_lift_required)
            travel_destination = travel_destination + travel_lift; //travel destination is above the target point

        rv += m_G0 % writeCoordinates(travel_destination) % commentSpaceLine("TRAVEL");
        setFeedrate(m_sb->setting< Velocity >(Constants::ProfileSettings::Travel::kSpeed));

        //write the travel lower (undo the lift)
        if (travel_lift_required && (lType == TravelLiftType::kBoth || lType == TravelLiftType::kLiftLowerOnly))
        {
            rv += m_G0 % writeCoordinates(target_location) % commentSpaceLine("TRAVEL LOWER Z");
            setFeedrate(m_sb->setting< Velocity >(Constants::PrinterSettings::MachineSpeed::kZSpeed));
        }

        if (m_first_travel) //if this is the first travel
            m_first_travel = false; //update for next one

        return rv;
    }

    QString HaasWriter::writeLine(const Point& start_point, const Point& target_point, const QSharedPointer<SettingsBase> params)
    {
        Velocity speed = params->setting<Velocity>(Constants::SegmentSettings::kSpeed);
        int rpm = params->setting<int>(Constants::SegmentSettings::kExtruderSpeed);
        RegionType region_type = params->setting<RegionType>(Constants::SegmentSettings::kRegionType);
        PathModifiers path_modifiers = params->setting<PathModifiers>(Constants::SegmentSettings::kPathModifiers);
        float output_rpm = rpm * m_sb->setting< float >(Constants::PrinterSettings::MachineSpeed::kGearRatio);

        QString rv;

        //turn on the extruder if it isn't already on
        if (m_extruders_on[0] == false && rpm > 0)
        {
            rv += writeExtruderOn(region_type, rpm);
        }

        //turn off extruder with an M5 before the line, rather than in-line with S0
        if (rpm == 0 && m_extruders_on[0] == true)
        {
            rv += writeExtruderOff();
        }

        rv += m_G1;
        // Forces first motion of layer to issue speed (needed for spiralize mode so that feedrate is scaled properly)
        if (m_layer_start)
        {
            setFeedrate(speed);
            rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit), 'f', 4);

            rv += m_s % QString::number(output_rpm, 'f', 4);
            m_current_rpm = rpm;

            m_layer_start = false;
        }

        // Update feedrate and extruder speed if needed
        if (getFeedrate() != speed)
        {
            setFeedrate(speed);
            rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit), 'f', 4);
        }

        if (rpm != m_current_rpm)
        {
            rv += m_s % QString::number(output_rpm, 'f', 4);
            m_current_rpm = rpm;
        }

        //writes WXYZ to destination
        rv += writeCoordinates(target_point);

        //add comment for gcode parser
        if (path_modifiers != PathModifiers::kNone)
            rv += commentSpaceLine(toString(region_type) % m_space % toString(path_modifiers));
        else
            rv += commentSpaceLine(toString(region_type));

        m_first_print = false;

        return rv;
    }

    QString HaasWriter::writeArc(const Point &start_point,
                                           const Point &end_point,
                                           const Point &center_point,
                                           const Angle &angle,
                                           const bool &ccw,
                                           const QSharedPointer<SettingsBase> params)
    {
        QString rv;

        Velocity speed = params->setting<Velocity>(Constants::SegmentSettings::kSpeed);
        int rpm = params->setting<int>(Constants::SegmentSettings::kExtruderSpeed);
        int material_number = params->setting<int>(Constants::SegmentSettings::kMaterialNumber);
        auto region_type = params->setting<RegionType>(Constants::SegmentSettings::kRegionType);
        auto path_modifiers = params->setting<PathModifiers>(Constants::SegmentSettings::kPathModifiers);
        float output_rpm = rpm * m_sb->setting< float >(Constants::PrinterSettings::MachineSpeed::kGearRatio);

        // Turn on the extruder if it isn't already on
        if (!m_extruders_on[0] && rpm > 0)
        {
            rv += writeExtruderOn(region_type, rpm);
        }

        rv += ((ccw) ? m_G3 : m_G2);

        if (getFeedrate() != speed)
        {
            setFeedrate(speed);
            rv += m_f % QString::number(speed.to(m_meta.m_velocity_unit), 'f', 4);
        }
        if (rpm != m_current_rpm)
        {
            rv += m_s % QString::number(output_rpm, 'f', 4);
            m_current_rpm = rpm;
        }

        rv += m_i % QString::number(Distance(center_point.x() - start_point.x()).to(m_meta.m_distance_unit), 'f', 4) %
              m_j % QString::number(Distance(center_point.y() - start_point.y()).to(m_meta.m_distance_unit), 'f', 4) %
              m_x % QString::number(Distance(end_point.x()).to(m_meta.m_distance_unit), 'f', 4) %
              m_y % QString::number(Distance(end_point.y()).to(m_meta.m_distance_unit), 'f', 4);

        // write vertical coordinate along the correct axis (Z or W) according to printer settings
        // only output Z/W coordinate if there was a change in Z/W
        Distance z_offset = m_sb->setting< Distance >(Constants::PrinterSettings::Dimensions::kZOffset);

        Distance target_z = end_point.z() + z_offset;
        if(qAbs(target_z - m_last_z) > 10)
        {
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

    QString HaasWriter::writeAfterPath(RegionType type)
    {
        QString rv;
        if(!m_spiral_layer)
        {
            rv += writeExtruderOff();
            if (type == RegionType::kPerimeter)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kPerimeterEnd).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kPerimeterEnd) % m_newline;
            }
            else if (type == RegionType::kInset)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInsetEnd).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInsetEnd) % m_newline;
            }
            else if (type == RegionType::kSkeleton)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkeletonEnd).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkeletonEnd) % m_newline;
            }
            else if (type == RegionType::kSkin)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkinEnd).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSkinEnd) % m_newline;
            }
            else if (type == RegionType::kInfill)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInfillEnd).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kInfillEnd) % m_newline;
            }
            else if (type == RegionType::kSupport)
            {
                if (!m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSupportEnd).isEmpty())
                    rv += m_sb->setting< QString >(Constants::ProfileSettings::GCode::kSupportEnd) % m_newline;
            }
        }
        return rv;
    }

    QString HaasWriter::writeAfterRegion(RegionType type)
    {
        QString rv;
        return rv;
    }

    QString HaasWriter::writeAfterIsland()
    {
        QString rv;
        return rv;
    }

    QString HaasWriter::writeAfterPart()
    {
        QString rv;
        return rv;
    }

    QString HaasWriter::writeAfterLayer()
    {
        QString rv;
        rv += m_sb->setting< QString >(Constants::PrinterSettings::GCode::kLayerCodeChange) % m_newline;
        return rv;
    }

    QString HaasWriter::writeShutdown()
    {
        QString rv;

        rv += "G91G28Z0.0" % m_newline;
        rv += "G91G28Y0.0" % m_newline;

        rv += m_sb->setting< QString >(Constants::PrinterSettings::GCode::kEndCode) % m_newline %
              "M19" % commentSpaceLine("ORIENT SPINDLE") % "M30" %
                commentSpaceLine("END OF G-CODE") % "%" % m_newline;
        return rv;
    }

    QString HaasWriter::writePurge(int RPM, int duration, int delay)
    {
        return "M69 F" % QString::number(RPM) % m_p % QString::number(duration) % m_s % QString::number(delay) % commentSpaceLine("PURGE");
    }

    QString HaasWriter::writeDwell(Time time)
    {
        if (time > 0)
            return m_G4 % m_p % QString::number(time.to(m_meta.m_time_unit), 'f', 4) % commentSpaceLine("DWELL");
        else
            return {};
    }

    QString HaasWriter::writeExtruderOn(RegionType type, int rpm)
    {
        QString rv;
        m_extruders_on[0] = true;
        float output_rpm;

        if (m_sb->setting< int >(Constants::MaterialSettings::Extruder::kInitialSpeed) > 0)
        {
            output_rpm = m_sb->setting< float >(Constants::PrinterSettings::MachineSpeed::kGearRatio) * m_sb->setting< int >(Constants::MaterialSettings::Extruder::kInitialSpeed);

            // Only update the current rpm if not using feedrate scaling. An updated rpm value here could prevent the S parameter
            // from being issued during the first G1 motion of the path and thus the extruder rate won't properly scale
            if(!(m_sb->setting< int >(Constants::MaterialSettings::Cooling::kForceMinLayerTime) &&
                 m_sb->setting< int >(Constants::MaterialSettings::Cooling::kForceMinLayerTimeMethod) == (int)ForceMinimumLayerTime::kSlow_Feedrate))
                m_current_rpm = m_sb->setting< int >(Constants::MaterialSettings::Extruder::kInitialSpeed);

            rv += m_M3 % m_s % QString::number(output_rpm, 'f', 4) % commentSpaceLine("TURN EXTRUDER ON");

            if (type == RegionType::kInset)
            {
                if(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelayInset) > 0)
                    rv += writeDwell(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelayInset));
            }
            else if (type == RegionType::kSkin)
            {
                if(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelaySkin) > 0)
                    rv += writeDwell(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelaySkin));
            }
            else if (type == RegionType::kInfill)
            {
                if(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelayInfill) > 0)
                    rv += writeDwell(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelayInfill));
            }
            else if (type == RegionType::kSkeleton)
            {
                if(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelaySkeleton) > 0)
                    rv += writeDwell(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelaySkeleton));
            }
            else
            {
                if(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelayPerimeter) > 0)
                    rv += writeDwell(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOnDelayPerimeter));
            }
        }
        else
        {
            output_rpm = m_sb->setting< float >(Constants::PrinterSettings::MachineSpeed::kGearRatio) * rpm;
            rv += m_M3 % m_s % QString::number(output_rpm, 'f', 4) % commentSpaceLine("TURN EXTRUDER ON");
            // Only update the current rpm if not using feedrate scaling. An updated rpm value here could prevent the S parameter
            // from being issued during the first G1 motion of the path and thus the extruder rate won't properly scale
            if(!(m_sb->setting< int >(Constants::MaterialSettings::Cooling::kForceMinLayerTime) &&
                 m_sb->setting< int >(Constants::MaterialSettings::Cooling::kForceMinLayerTimeMethod) == (int)ForceMinimumLayerTime::kSlow_Feedrate))
                m_current_rpm = rpm;
        }

        return rv;
    }

    QString HaasWriter::writeExtruderOff()
    {
        QString rv;
        m_extruders_on[0] = false;
        if(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOffDelay) > 0)
        {
            rv += writeDwell(m_sb->setting< Time >(Constants::MaterialSettings::Extruder::kOffDelay));
        }
        rv += m_M5 % commentSpaceLine("TURN EXTRUDER OFF");
        m_current_rpm = 0;
        return rv;
    }

    QString HaasWriter::writeCoordinates(Point destination)
    {
        QString rv;

        //always specify X and Y
        rv += m_x % QString::number(Distance(destination.x()).to(m_meta.m_distance_unit), 'f', 4) %
              m_y % QString::number(Distance(destination.y()).to(m_meta.m_distance_unit), 'f', 4);

        //write vertical coordinate along the correct axis (Z or W) according to printer settings
        //only output Z/W coordinate if there was a change in Z/W
        Distance z_offset = m_sb->setting< Distance >(Constants::PrinterSettings::Dimensions::kZOffset);

        Distance target_z = destination.z() + z_offset;
        if(qAbs(target_z - m_last_z) > 10)
        {
            rv += m_z % QString::number(Distance(target_z).to(m_meta.m_distance_unit), 'f', 4);
            m_current_z = target_z;
            m_last_z = target_z;
        }
        return rv;
    }

}  // namespace ORNL
