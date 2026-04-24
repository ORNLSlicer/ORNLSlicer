#include "step/layer/regions/perimeter_sector.h"

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "geometry/segments/line.h"
#include "geometry/settings_polygon.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
PerimeterSector::PerimeterSector(const QSharedPointer<SettingsBase>& sb, const int index,
                                 const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, index, settings_polygons) {
    // NOP
}

QString PerimeterSector::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;

    if (m_paths.size() > 0) {
        for (Path path : m_paths) {
            gcode += writer->writeBeforeRegion(RegionType::kPerimeter, path.size());
            for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
                gcode += segment->writeGCode(writer);
            }
            gcode += writer->writeAfterPath(RegionType::kPerimeter);
        }
    }
    else
        gcode += writer->writeEmptyStep();

    return gcode;
}

void PerimeterSector::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();

    Point center =
        Point(m_sb->setting<double>(PRS::Dimensions::kXOffset), m_sb->setting<double>(PRS::Dimensions::kYOffset));

    if (m_computed_geometry.size() > 1) {
        for (int i = 0; i < m_computed_geometry.size(); ++i) {
            if (m_computed_geometry[i].last() == m_computed_geometry[(i + 1) % m_computed_geometry.size()].first()) {
                Polyline copyMinusStart = m_computed_geometry[(i + 1) % m_computed_geometry.size()];
                copyMinusStart.removeAt(0);
                for (Point p : copyMinusStart)
                    m_computed_geometry[i].append(p);
                m_computed_geometry.removeAt((i + 1) % m_computed_geometry.size());
            }
        }
    }

    QVector<QPair<Distance, Polyline>> circularOrder;
    for (Polyline& line : m_computed_geometry) {
        Angle ang1 = MathUtils::internalAngle(line.first(), center, m_start_vec);
        Angle ang2 = MathUtils::internalAngle(line.last(), center, m_start_vec);

        if (ang2 < ang1)
            line = line.reverse();
    }

    for (Polyline line : m_computed_geometry) {
        QPair<Distance, Polyline> pair;
        pair.first = center.distance(line.first());
        pair.second = line;
        circularOrder.push_back(pair);
    }

    //        bool shouldReverse =
    //        static_cast<PrintDirection>(m_sb->setting<int>(PS::Ordering::kPerimeterReverseOrder))
    //                == PrintDirection::kReverse_All_Layers;

    //        std::sort(std::begin(circularOrder), std::end(circularOrder),
    //                  [&](const auto& a, const auto& b)
    //        {
    //            if(shouldReverse)
    //                return a.first < b.first;
    //            else
    //                return a.first > b.first;
    //        });

    m_computed_geometry.clear();
    for (QPair<Distance, Polyline> eachPair : circularOrder)
        m_computed_geometry.push_back(eachPair.second);

    if (static_cast<PrintDirection>(m_sb->setting<int>(PS::Ordering::kPerimeterReverseDirection)) !=
        PrintDirection::kReverse_off) {
        for (Polyline& line : m_computed_geometry) {
            line = line.reverse();
        }
    }

    for (Polyline line : m_computed_geometry)
        m_paths.push_back(createPath(line));
}

void PerimeterSector::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                               QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) {
    // NOP
}

void PerimeterSector::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    // NOP
}

Path PerimeterSector::createPath(Polyline line) {
    Distance width = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
    Distance height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity speed = m_sb->setting<Velocity>(PS::Perimeter::kSpeed);
    Acceleration acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kPerimeter);
    AngularVelocity extruder_speed = m_sb->setting<AngularVelocity>(PS::Perimeter::kExtruderSpeed);

    Path newPath;
    for (int i = 0, end = line.size() - 1; i < end; ++i) {

        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line[i], line[i + 1]);

        segment->getSb()->setSetting(SS::kWidth, width);
        segment->getSb()->setSetting(SS::kHeight, height);
        segment->getSb()->setSetting(SS::kSpeed, speed);
        segment->getSb()->setSetting(SS::kAccel, acceleration);
        segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
        segment->getSb()->setSetting(SS::kRegionType, RegionType::kPerimeter);

        newPath.append(segment);
    }
    return newPath;
}

void PerimeterSector::setComputedGeometry(QVector<Polyline> perimeters) { m_computed_geometry = perimeters; }

void PerimeterSector::setStartVector(Point p) { m_start_vec = p; }
} // namespace ORNL
