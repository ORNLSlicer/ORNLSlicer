#include "gcode/writers/rpbf_writer.h"

#include <QStringBuilder>
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
RPBFWriter::RPBFWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb) : WriterBase(meta, sb) {
    m_power_prefex = "$$POWER/";
    m_speed_prefix = "$$SPEED/";
    m_focus_prefix = "$$FOCUS/";
    m_spot_size_prefix = "$$SPOTSIZE/";
    m_polyline_prefix = "$$POLYLINE/";
    m_hatch_prefix = "$$HATCHES/";
    m_variable_delim = ",";
    m_placeholder = "0,0,0";
}

QString RPBFWriter::writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                                      int num_layers) {
    QString rv;

    rv += m_newline;
    rv += commentLine("LAYER COUNT: " % QString::number(num_layers));

    return rv;
}

QString RPBFWriter::writeBeforeLayer(float new_min_z, QSharedPointer<SettingsBase> sb) {
    return commentLine("SECTOR SPLIT");
}

QString RPBFWriter::writeBeforePart(QVector3D normal) { return QString(); }

QString RPBFWriter::writeBeforeIsland() { return QString(); }

QString RPBFWriter::writeBeforeRegion(RegionType type, int pathSize) {
    QString rv;
    if (type == RegionType::kPerimeter) {
        rv += m_power_prefex % QString::number(m_sb->setting<Power>(PS::Perimeter::kPower)(), 'f', 0) % m_newline %
              m_speed_prefix %
              QString::number(m_sb->setting<Velocity>(PS::Perimeter::kSpeed).to(m_meta.m_velocity_unit), 'f', 0) %
              m_newline % m_focus_prefix %
              QString::number(m_sb->setting<Distance>(PS::Perimeter::kFocus).to(mm), 'f', 3) % m_newline %
              m_spot_size_prefix % QString::number(m_sb->setting<int>(PS::Perimeter::kSpotSize), 'f', 0) % m_newline %
              m_polyline_prefix % QString::number(1) % m_variable_delim % QString::number(pathSize);
    }
    else if (type == RegionType::kInfill) {
        rv += m_power_prefex % QString::number(m_sb->setting<Power>(PS::Infill::kPower)(), 'f', 0) % m_newline %
              m_speed_prefix %
              QString::number(m_sb->setting<Velocity>(PS::Infill::kSpeed).to(m_meta.m_velocity_unit), 'f', 0) %
              m_newline % m_focus_prefix % QString::number(m_sb->setting<Distance>(PS::Infill::kFocus).to(mm), 'f', 3) %
              m_newline % m_spot_size_prefix % QString::number(m_sb->setting<int>(PS::Infill::kSpotSize), 'f', 0) %
              m_newline % m_hatch_prefix % QString::number(1) % m_variable_delim % QString::number(pathSize);
    }
    return rv;
}

QString RPBFWriter::writeBeforePath(RegionType type) { return QString(); }

QString RPBFWriter::writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                                QSharedPointer<SettingsBase> params) {
    return QString();
}

QString RPBFWriter::writeLine(const Point& start_point, const Point& target_point,
                              const QSharedPointer<SettingsBase> params) {
    QString rv;
    rv += m_variable_delim % QString::number(Distance(start_point.x()).to(m_meta.m_distance_unit), 'f', 0) %
          m_variable_delim % QString::number(Distance(start_point.y()).to(m_meta.m_distance_unit), 'f', 0) %
          m_variable_delim % QString::number(Distance(target_point.x()).to(m_meta.m_distance_unit), 'f', 0) %
          m_variable_delim % QString::number(Distance(target_point.y()).to(m_meta.m_distance_unit), 'f', 0) %
          m_variable_delim % m_placeholder;

    return rv;
}

QString RPBFWriter::writeAfterPath(RegionType type) {
    return commentSpaceLine(toString(type)); // % m_newline;
}

QString RPBFWriter::writeAfterRegion(RegionType type) { return commentLine("SECTOR SPLIT"); }

QString RPBFWriter::writeAfterIsland() { return QString(); }

QString RPBFWriter::writeAfterPart() { return QString(); }

QString RPBFWriter::writeAfterLayer() { return QString(); }

QString RPBFWriter::writeShutdown() { return QString(); }

QString RPBFWriter::writeDwell(Time time) { return QString(); }
} // namespace ORNL
