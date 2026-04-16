#include "step/layer/regions/brim.h"

#include <algorithm>

#include <qcontainerfwd.h>
#include <qmath.h>
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
Brim::Brim(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, settings_polygons) {
    // NOP
}

QString Brim::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kBrim);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kBrim);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kBrim);
    }
    gcode += writer->writeAfterRegion(RegionType::kBrim);
    return gcode;
}

void Brim::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum));

    Distance brimWidth = m_sb->setting<Distance>(MS::PlatformAdhesion::kBrimWidth);
    Distance beadWidth = m_sb->setting<Distance>(MS::PlatformAdhesion::kBrimBeadWidth);
    int m_rings = qCeil(brimWidth() / beadWidth());

    // m_geometry is set to the outer most loop, which is printed first
    // printing inwards if there are more than 1 loops
    for (int ring_nr = 0; ring_nr < m_rings; ring_nr++) {
        Distance addOffset = beadWidth;
        if (ring_nr == 0)
            addOffset = 0;
        PolygonList path_line = m_geometry.offset(-addOffset);

        if (!path_line.size()) {
            break;
        }
        m_geometry = path_line;

        for (Polygon poly : path_line) {
            Polyline line = poly.toPolyline();
            line.pop_back();
            m_computed_geometry.push_back(line);
        }
    }
}

void Brim::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
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
                           getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius));

    m_paths.clear();

    poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kInset,
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

void Brim::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    // NOP
}

Path Brim::createPath(Polyline line) {
    Path new_path;

    Distance default_width = m_sb->setting<Distance>(MS::PlatformAdhesion::kBrimBeadWidth);
    Distance default_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity default_speed = m_sb->setting<Velocity>(PS::Layer::kSpeed);
    Acceleration default_acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kDefault);
    AngularVelocity default_extruder_speed = m_sb->setting<AngularVelocity>(PS::Layer::kExtruderSpeed);
    int material_number = m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum);

    for (int i = 0, end_cond = line.size(); i < end_cond; ++i) {
        Point start = line[i];
        Point end = line[(i + 1) % end_cond];

        bool is_settings_region = false;

        QVector<Point> intersections;
        for (auto settings_poly : m_settings_polygons) {
            QSharedPointer<SettingsBase> updatedBase = QSharedPointer<SettingsBase>::create(*m_sb);
            updatedBase->populate(settings_poly.getSettings());
            // Determine if the start and end points are in a settings region
            if (settings_poly.inside(start)) {
                start.setSettings(updatedBase);
                is_settings_region = true;
            }
            else {
                start.setSettings(m_sb);
            }

            if (settings_poly.inside(end))
                end.setSettings(updatedBase);

            // Find if/ where this line intersects with a settings polygon
            QVector<Point> poly_intersect = settings_poly.clipLine(start, end);
            intersections.append(poly_intersect);
        }

        // Divide lines into subsections
        if (intersections.size() > 1) {
            // Sort points in order to start
            std::sort(intersections.begin(), intersections.end(),
                      [start](auto lhs, auto rhs) { return start.distance(lhs) < start.distance(rhs); });

            for (Point& point : intersections) {
                // If no settings change, skip this point
                if (point.getSettings()->json() == m_sb->json())
                    continue;

                QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(start, point);

                segment->getSb()->setSetting(SS::kWidth, is_settings_region ? start.getSettings()->setting<Distance>(
                                                                                  MS::PlatformAdhesion::kBrimBeadWidth)
                                                                            : default_width);
                segment->getSb()->setSetting(
                    SS::kHeight, is_settings_region ? start.getSettings()->setting<Distance>(PS::Layer::kLayerHeight)
                                                    : default_height);
                segment->getSb()->setSetting(SS::kSpeed, is_settings_region
                                                             ? start.getSettings()->setting<Velocity>(PS::Layer::kSpeed)
                                                             : default_speed);
                segment->getSb()->setSetting(
                    SS::kAccel, is_settings_region
                                    ? start.getSettings()->setting<Acceleration>(PRS::Acceleration::kDefault)
                                    : default_acceleration);
                segment->getSb()->setSetting(
                    SS::kExtruderSpeed, is_settings_region
                                            ? start.getSettings()->setting<AngularVelocity>(PS::Layer::kExtruderSpeed)
                                            : default_extruder_speed);
                segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
                segment->getSb()->setSetting(SS::kRegionType, RegionType::kBrim);

                new_path.append(segment);
                is_settings_region = !is_settings_region;
                start = point;
            }
        }

        // Add final segment
        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(start, end);
        segment->getSb()->setSetting(SS::kWidth, is_settings_region ? start.getSettings()->setting<Distance>(
                                                                          MS::PlatformAdhesion::kBrimBeadWidth)
                                                                    : default_width);
        segment->getSb()->setSetting(SS::kHeight, is_settings_region
                                                      ? start.getSettings()->setting<Distance>(PS::Layer::kLayerHeight)
                                                      : default_height);
        segment->getSb()->setSetting(
            SS::kSpeed, is_settings_region ? start.getSettings()->setting<Velocity>(PS::Layer::kSpeed) : default_speed);
        segment->getSb()->setSetting(
            SS::kAccel, is_settings_region ? start.getSettings()->setting<Acceleration>(PRS::Acceleration::kDefault)
                                           : default_acceleration);
        segment->getSb()->setSetting(SS::kExtruderSpeed,
                                     is_settings_region
                                         ? start.getSettings()->setting<AngularVelocity>(PS::Layer::kExtruderSpeed)
                                         : default_extruder_speed);
        segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        segment->getSb()->setSetting(SS::kRegionType, RegionType::kBrim);

        new_path.append(segment);
    }

    if (new_path.calculateLength() > m_sb->setting<Distance>(PS::Layer::kMinExtrudeLength)) {
        return new_path;
    }
    else {
        return Path();
    }
}
} // namespace ORNL
