#include "step/layer/regions/anchor.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segment_base.h"
#include "geometry/segments/line.h"
#include "geometry/settings_polygon.h"
#include "managers/sync/sync_manager.h"
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
Anchor::Anchor(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, settings_polygons) {
    // NOP
}

QString Anchor::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kAnchor);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kAnchor);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kAnchor);
    }
    gcode += writer->writeAfterRegion(RegionType::kAnchor);
    return gcode;
}

void Anchor::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();

    Distance beadWidth = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
    int rings = m_sb->setting<int>(PS::Perimeter::kCount);

    PolygonList path_line = m_geometry.offset(-beadWidth / 2);

    int ring_nr = 0;
    while (!path_line.isEmpty() && ring_nr < rings) {
        for (Polygon poly : path_line) {
            Polyline line = poly.toPolyline();
            line.pop_back();
            m_computed_geometry.push_back(line);
        }

        path_line = path_line.offset(-beadWidth, -beadWidth / 2);
        ring_nr++;
    }
}

void Anchor::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                      QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) {
    PolylineOrderOptimizer poo(current_location, layerNumber);

    PathOrderOptimization pathOrderOptimization =
        static_cast<PathOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPathOrder));
    if (pathOrderOptimization == PathOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPathXLocation),
                            getSb()->setting<double>(PS::Optimizations::kCustomPathYLocation));

        poo.setStartOverride(startOverride);
    }

    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPointOrder));

    if (pointOrderOptimization == PointOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPointXLocation),
                            getSb()->setting<double>(PS::Optimizations::kCustomPointYLocation));

        poo.setStartPointOverride(startOverride);
    }

    poo.setPointParameters(pointOrderOptimization, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                           getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                           getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                           getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                           getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius),
                           getSb()->setting<bool>(PS::Optimizations::kEnablePointOrderSegmentBreaking));

    m_paths.clear();

    poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kAnchor,
                              static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline();
        Path newPath = createPath(result);

        if (newPath.size() > 0) {
            QVector<Path> temp_path;
            calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3), temp_path);
            PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                  m_sb->setting<Velocity>(PS::Travel::kSpeed));
            current_location = newPath.back()->end();
            m_paths.push_back(newPath);
        }
    }
}

void Anchor::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    // NOP
}

Path Anchor::createPath(Polyline line) {
    for (int i = 0, end = line.size(); i < end; ++i) {
        Path new_path;

        Distance default_width = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
        Distance default_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
        Velocity default_speed = m_sb->setting<Velocity>(PS::Perimeter::kSpeed);
        Acceleration default_acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kPerimeter);
        AngularVelocity default_extruder_speed = m_sb->setting<AngularVelocity>(PS::Perimeter::kExtruderSpeed);
        int material_number = m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum);

        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line[i], line[(i + 1) % end]);
        segment->getSb()->setSetting(SS::kWidth, default_width);
        segment->getSb()->setSetting(SS::kHeight, default_height);
        segment->getSb()->setSetting(SS::kSpeed, default_speed);
        segment->getSb()->setSetting(SS::kAccel, default_acceleration);
        segment->getSb()->setSetting(SS::kExtruderSpeed, default_extruder_speed);
        segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        segment->getSb()->setSetting(SS::kRegionType, RegionType::kPerimeter);

        new_path.append(segment);

        if (new_path.calculateLength() > m_sb->setting<Distance>(PS::Perimeter::kMinPathLength)) {
            return new_path;
        }
        else {
            return Path();
        }
    }

    return Path();
}
} // namespace ORNL
