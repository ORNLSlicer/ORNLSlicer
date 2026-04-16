#include "gcode/writers/adamantine_writer.h"

#include <QStringBuilder>
#include <qhashfunctions.h>
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
AdamantineWriter::AdamantineWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {}

QString AdamantineWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x,
                                            Distance maximum_y, int num_layers) {
    counter = 1;
    QString rv;
    rv = "Mode" % m_space % "x" % m_space % "y" % m_space % "z" % m_space % "pmod" % m_space % "param\n";
    return rv;
} // writeInitialSetup

QString AdamantineWriter::writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) {
    QString rv;
    return rv;
} // writeBeforeLayer

QString AdamantineWriter::writeBeforePart(QVector3D normal) {
    QString rv;
    return rv;
} // writeBeforePart

QString AdamantineWriter::writeBeforeIsland() {
    QString rv;
    return rv;
} // writeBeforeIsland

QString AdamantineWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    return rv;
} // writeBeforeRegion

QString AdamantineWriter::writeBeforePath(RegionType type) {
    m_region_type = type;
    QString rv;
    return rv;
} // writeBeforePath

QString AdamantineWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                      QSharedPointer<SettingsBase> params) {
    counter++;
    QString rv;
    // write the travel
    // Adamantine structure:
    //   column  1: 1 for spot mode, 0 for line mode
    //   columns 2-4: x,y,z coordinates in meters
    //   column  5: nominal power. In example, spot mode is 0, line mode is 1. Should seek clarification.
    rv += "1" % m_space % QString::number(Distance(target_location.x()).to(m_meta.m_distance_unit), 'f', 4) % m_space %
          QString::number(Distance(target_location.y()).to(m_meta.m_distance_unit), 'f', 5) % m_space %
          QString::number(Distance(target_location.z()).to(m_meta.m_distance_unit), 'f', 8) % m_space % "0" % m_space;

    if (m_sb->setting<int>(MS::Extruder::kInitialSpeed) > 0) {
        if (m_region_type == RegionType::kInset) {
            if (m_sb->setting<Time>(MS::Extruder::kOnDelayInset) >= 0)
                rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayInset)) % "\n";
        }
        else if (m_region_type == RegionType::kSkin) {
            if (m_sb->setting<Time>(MS::Extruder::kOnDelaySkin) >= 0)
                rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelaySkin)) % "\n";
        }
        else if (m_region_type == RegionType::kInfill) {
            if (m_sb->setting<Time>(MS::Extruder::kOnDelayInfill) >= 0)
                rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayInfill)) % "\n";
        }
        else if (m_region_type == RegionType::kSkeleton) {
            if (m_sb->setting<Time>(MS::Extruder::kOnDelaySkeleton) >= 0)
                rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelaySkeleton)) % "\n";
        }
        else {
            if (m_sb->setting<Time>(MS::Extruder::kOnDelayPerimeter) >= 0)
                rv += writeDwell(m_sb->setting<Time>(MS::Extruder::kOnDelayPerimeter)) % "\n";
        }
    }
    // else
    return rv;

} // writeTravel

QString AdamantineWriter::writeLine(const Point& start_point, const Point& target_point,
                                    const QSharedPointer<SettingsBase> params) {
    counter++;
    QString rv;
    Velocity speed = params->setting<Velocity>(SS::kSpeed);
    // write the line
    // Adamantine structure:
    //   column  1: 1 for spot mode, 0 for line mode
    //   columns 2-4: x,y,z coordinates in meters
    //   column  5: nominal power. In example, spot mode is 0, line mode is 1. Should seek clarification.
    rv += "0" % m_space % QString::number(Distance(target_point.x()).to(m_meta.m_distance_unit), 'f', 4) % m_space %
          QString::number(Distance(target_point.y()).to(m_meta.m_distance_unit), 'f', 5) % m_space %
          QString::number(Distance(target_point.z()).to(m_meta.m_distance_unit), 'f', 8) % m_space % "1" % m_space %
          QString::number(speed.to(m_meta.m_velocity_unit), 'f', 5) % "\n";
    return rv;

} // writeLine

QString AdamantineWriter::writeAfterPath(RegionType type) {
    QString rv;
    return rv;
} // writeAfterPath

QString AdamantineWriter::writeAfterRegion(RegionType type) {
    QString rv;
    return rv;
} // writeAfterRegion

QString AdamantineWriter::writeAfterIsland() {
    QString rv;
    return rv;
} // writeAfterIsland

QString AdamantineWriter::writeAfterPart() {
    QString rv;
    rv = "Number of path segments\n" % QString::number(counter);
    return rv;
} // writeAfterPart

QString AdamantineWriter::writeAfterLayer() {
    QString rv;
    return rv;
} // writeAfterLayer

QString AdamantineWriter::writeShutdown() {
    QString rv;
    return rv;
} // writeShutdown

QString AdamantineWriter::writeDwell(Time time) {
    if (time >= 0)
        return QString::number(time.to(m_meta.m_time_unit), 'f', 4);
    else
        return {};
} // writeDwell
} // namespace ORNL
