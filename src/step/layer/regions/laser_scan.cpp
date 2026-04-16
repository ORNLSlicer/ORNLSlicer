#include "step/layer/regions/laser_scan.h"

#include <qcontainerfwd.h>
#include <qmath.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/polyline.h"
#include "geometry/segment_base.h"
#include "geometry/segments/scan.h"
#include "geometry/settings_polygon.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
LaserScan::LaserScan(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, settings_polygons) {
    // NOP
}

QString LaserScan::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kLaserScan);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
    }
    return gcode;
}

void LaserScan::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();

    Distance x_offset = m_sb->setting<Distance>(PS::LaserScanner::kLaserScannerXOffset);
    Distance y_offset = m_sb->setting<Distance>(PS::LaserScanner::kLaserScannerYOffset);
    Distance scan_width = m_sb->setting<Distance>(PS::LaserScanner::kLaserScannerWidth);

    Distance buffer_distance = 0;
    if (m_sb->setting<bool>(PS::LaserScanner::kEnableScannerBuffer))
        buffer_distance = m_sb->setting<Distance>(PS::LaserScanner::kBufferDistance);

    QVector<Polyline> scan_lines;

    // Determine which axis the laser scanner will travel
    if (static_cast<Axis>(m_sb->setting<int>(PS::LaserScanner::kLaserScannerAxis)) == Axis::kX) {
        // Determine number of scan lines
        float y_distance = m_geometry.max().y() - m_geometry.min().y();
        int number_of_scan_lines = qCeil(y_distance / scan_width());

        // Determine starting point of first scan line
        Point start = m_geometry.min();
        start.x(start.x() - x_offset - buffer_distance);
        start.y(start.y() - y_offset + (scan_width / 2));

        // Determine ending point of first scan line
        Point end = m_geometry.min();
        end.x(m_geometry.max().x() - x_offset + buffer_distance);
        end.y(end.y() - y_offset + (scan_width / 2));

        // Add first scan line to scan_lines
        scan_lines += Polyline({start, end});

        // Add all necessary scan lines as well as scan line tavels to scan_lines
        while (scan_lines.size() < number_of_scan_lines * 2 - 1) {
            Polyline scan_line = scan_lines.last();

            scan_line.first().y(scan_line.first().y() + scan_width);
            scan_line.last().y(scan_line.last().y() + scan_width);

            scan_lines += Polyline({scan_lines.last().last(), scan_line.last()});

            scan_lines += scan_line.reverse();
        }
    }
    else // Scanner Axis = Y
    {
        // Determine number of scan lines
        float x_distance = m_geometry.max().x() - m_geometry.min().x();
        int number_of_scan_lines = qCeil(x_distance / scan_width());

        // Determine starting point of first scan line
        Point start = m_geometry.min();
        start.x(start.x() - x_offset + (scan_width / 2));
        start.y(start.y() - y_offset - buffer_distance);

        // Determine ending point of first scan line
        Point end = m_geometry.min();
        end.x(end.x() - x_offset + (scan_width / 2));
        end.y(m_geometry.max().y() - y_offset + buffer_distance);

        // Add first scan line to scan_lines
        scan_lines += Polyline({start, end});

        // Add all necessary scan lines as well as scan line tavels to scan_lines
        while (scan_lines.size() < number_of_scan_lines * 2 - 1) {
            Polyline scan_line = scan_lines.last();

            scan_line.first().x(scan_line.first().x() + scan_width);
            scan_line.last().x(scan_line.last().x() + scan_width);

            scan_lines += Polyline({scan_lines.last().last(), scan_line.last()});

            scan_lines += scan_line.reverse();
        }
    }

    m_computed_geometry = scan_lines;

    Path finalPath;
    for (Polyline line : m_computed_geometry)
        finalPath.append(createPath(line));

    m_paths.append(finalPath);
}

void LaserScan::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                         QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) {
    // NOP - handled by ScanLayer
}

void LaserScan::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    // NOP
}

Path LaserScan::createPath(Polyline line) {
    Path newPath;

    QSharedPointer<ScanSegment> segment = QSharedPointer<ScanSegment>::create(line.first(), line.last());
    segment->getSb()->setSetting(SS::kRegionType, RegionType::kLaserScan);

    if (m_paths.size() % 2 == 0) {
        segment->setDataCollection(true);
        segment->getSb()->setSetting(SS::kSpeed, this->getSb()->setting<Velocity>(PS::LaserScanner::kSpeed));
    }
    else {
        segment->setDataCollection(false);
        segment->getSb()->setSetting(SS::kSpeed, this->getSb()->setting<Velocity>(PS::Travel::kSpeed));
    }

    return newPath;
}
} // namespace ORNL
