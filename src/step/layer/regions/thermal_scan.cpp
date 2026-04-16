#include "step/layer/regions/thermal_scan.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "geometry/segments/scan.h"
#include "geometry/settings_polygon.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
ThermalScan::ThermalScan(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, settings_polygons) {
    // NOP
}

QString ThermalScan::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kThermalScan);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kThermalScan);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kThermalScan);
    }
    gcode += writer->writeAfterRegion(RegionType::kThermalScan);
    return gcode;
}

void ThermalScan::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();

    Distance x_offset = m_sb->setting<Distance>(PS::ThermalScanner::kThermalScannerXOffset);
    Distance y_offset = m_sb->setting<Distance>(PS::ThermalScanner::kThermalScannerYOffset);

    Point start, end = m_geometry.boundingRectCenter();
    end.x(end.x() - x_offset);
    end.y(end.y() - y_offset);

    m_computed_geometry = Polyline({start, end});

    m_paths.push_back(createPath(m_computed_geometry));
}

void ThermalScan::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                           QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) {
    // NOP?
}

void ThermalScan::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    // NOP
}

Path ThermalScan::createPath(Polyline line) {
    Path newPath;

    QSharedPointer<ScanSegment> segment = QSharedPointer<ScanSegment>::create(line.first(), line.last());
    segment->getSb()->setSetting(SS::kRegionType, RegionType::kThermalScan);

    newPath.append(segment);

    return newPath;
}
} // namespace ORNL
