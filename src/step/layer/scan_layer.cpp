#include "step/layer/scan_layer.h"

#include <limits>

#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qdir.h>
#include <qfiledevice.h>
#include <qhashfunctions.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/segments/travel.h"
#include "optimizers/path_order_optimizer.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/region_base.h"
#include "step/step.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {

ScanLayer::ScanLayer(int layer, const QSharedPointer<SettingsBase>& sb) : m_layer_num(layer), Step(sb) {
    m_type = StepType::kScan;
    m_first_connect = false;
}

QString ScanLayer::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    int index = 0;
    for (QSharedPointer<IslandBase> island : m_islands) {
        gcode += writer->writeBeforeScan(
            island->getGeometry().min(), island->getGeometry().max(), m_layer_num, index,
            static_cast<Axis>(island->getSb()->setting<int>(PS::LaserScanner::kOrientationAxis)),
            island->getSb()->setting<Angle>(PS::LaserScanner::kOrientationAngle));

        gcode += island->writeGCode(writer);

        if (m_layer_num == 0) {
            gcode += writer->writeAfterScan(
                0, island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScannerStepDistance),
                island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScanLineResolution));
        }
        else {
            gcode +=
                writer->writeAfterScan(island->getSb()->setting<Distance>(PS::Layer::kBeadWidth),
                                       island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScannerStepDistance),
                                       island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScanLineResolution));
        }
        ++index;
    }

    QString filename = m_path.absoluteFilePath("scan_output-" % QString::number(m_layer_num) % ".dat");
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);

        Point widthHeight = m_geometry.max() - m_geometry.min();
        Distance stepDistance = getSb()->setting<Distance>(PS::LaserScanner::kLaserScannerStepDistance);
        Distance lineResolution = getSb()->setting<Distance>(PS::LaserScanner::kLaserScanLineResolution);

        stream << "P||" % QString::number(Distance(m_geometry.min().x()).to(mm), 'f', 4) % '|' %
                      QString::number(Distance(m_geometry.min().y()).to(mm), 'f', 4) % '|' %
                      QString::number(Distance(widthHeight.x()).to(mm), 'f', 4) % '|' %
                      QString::number(Distance(widthHeight.y()).to(mm), 'f', 4) % '|' %
                      QString::number(stepDistance.to(mm), 'f', 4) % '|' %
                      QString::number(lineResolution.to(mm), 'f', 4) % '\n';

        QVector<PolygonList> split_geometry = m_geometry.splitIntoParts();

        for (PolygonList island : split_geometry) {
            for (Polygon poly : island) {
                stream << "O||";
                QString actual_sep("||");
                QString sep;
                for (Point pt : poly) {
                    stream << sep % QString::number(Distance(pt.x()).to(mm), 'f', 4) % '|' %
                                  QString::number(Distance(pt.y()).to(mm), 'f', 4);
                    sep = actual_sep;
                }
                stream << '\n';
            }
        }
    }
    file.close();

    return gcode;
}

void ScanLayer::compute() {
    for (QSharedPointer<IslandBase> island : m_islands) {
        island->compute(m_layer_num, m_sync);
    }
}

void ScanLayer::connectPaths(Point& start, int& start_index, QVector<QSharedPointer<RegionBase>>& previousRegions) {
    for (QSharedPointer<IslandBase> island : m_islands) {
        Point newEnd = start;
        for (QSharedPointer<RegionBase> region : island->getRegions()) {
            region->getPaths().first().removeTravels();
            QSharedPointer<TravelSegment> newSegment;
            if (m_first_connect) {
                if (m_layer_num == 0) {
                    Distance z_offset = island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScannerHeight) -
                                        island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScannerHeightOffset);

                    start.z(-z_offset);
                }
                newSegment = QSharedPointer<TravelSegment>::create(start, region->getPaths().first().front()->start());
                m_first_connect = false;
            }
            else {
                newSegment = QSharedPointer<TravelSegment>::create(start, region->getPaths().first().front()->start());
            }

            newSegment->getSb()->setSetting(SS::kRegionType, RegionType::kLaserScan);
            newSegment->getSb()->setSetting(SS::kSpeed, m_sb->setting<Velocity>(PS::Travel::kSpeed));
            region->getPaths().first().prepend(newSegment);
            newEnd = region->getPaths().last().back()->end();
        }
        start = newEnd;
    }
}

void ScanLayer::calculateModifiers(Point& point) {
    // NOP
}

Point ScanLayer::getEndLocation() {

    for (auto& island : m_islands) {
        auto regions = island->getRegions();
        for (int region_index = regions.size() - 1; region_index >= 0; region_index--) {
            auto paths = regions[region_index]->getPaths();
            for (int path_index = paths.size() - 1; path_index >= 0; path_index--) {
                auto path = paths[path_index];
                if (path.size() > 0)
                    return path.back()->end();
            }
        }
    }
    return Point(0, 0, 0);
}

void ScanLayer::unorient() {
    if (!this->isDirty()) {
        for (QSharedPointer<IslandBase> island : m_islands) {

            Point m_half_shift = m_shift_amount;
            Distance scan_height = island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScannerHeight) -
                                   island->getSb()->setting<Distance>(PS::LaserScanner::kLaserScannerHeightOffset);

            m_half_shift.x(m_half_shift.x() - m_sb->setting<double>(PRS::Dimensions::kXOffset));
            m_half_shift.y(m_half_shift.y() - m_sb->setting<double>(PRS::Dimensions::kYOffset));
            m_half_shift.z(m_shift_amount.z() + scan_height);

            // rotate and then shift every island in the layer
            QQuaternion rotation = MathUtils::CreateQuaternion(QVector3D(0, 0, 1), m_slicing_plane.normal());
            island->transform(rotation.inverted(), m_half_shift * -1);

            // unapply current origin shift
            Point m_origin_shift = Point(.0, .0, .0) - m_shift_amount;
            m_origin_shift.z(.0);
            island->transform(QQuaternion(), m_origin_shift * -1);
        }
    }
}

void ScanLayer::reorient() {
    // unapply origin shift
    Point m_origin_shift = Point(.0, .0, .0) - m_shift_amount;
    m_origin_shift.z(.0);
    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(QQuaternion(), m_origin_shift);
    }

    // raise the layer by half the layer height, because cross-sections are taken at the center of a layer
    // but the path for the extruder should be at a full layer height
    Point m_half_shift = m_shift_amount;
    Distance scan_height = m_sb->setting<Distance>(PS::LaserScanner::kLaserScannerHeight) -
                           m_sb->setting<Distance>(PS::LaserScanner::kLaserScannerHeightOffset);

    m_half_shift.x(m_half_shift.x() - m_sb->setting<double>(PRS::Dimensions::kXOffset));
    m_half_shift.y(m_half_shift.y() - m_sb->setting<double>(PRS::Dimensions::kYOffset));
    m_half_shift.z(m_shift_amount.z() + scan_height);

    // rotate and then shift every island in the layer
    QQuaternion rotation = MathUtils::CreateQuaternion(QVector3D(0, 0, 1), m_slicing_plane.normal());
    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(rotation, m_half_shift);
    }
}

float ScanLayer::getMinZ() {
    float min_z = std::numeric_limits<float>::max();
    for (QSharedPointer<IslandBase> island : m_islands) {
        float island_min = island->getMinZ();
        if (island_min < min_z)
            min_z = island_min;
    }
    return min_z;
}

void ScanLayer::setFirst() { m_first_connect = true; }
} // namespace ORNL
