#include "step/layer/regions/raft.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/pattern_generator.h"
#include "geometry/point.h"
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
Raft::Raft(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, settings_polygons) {
    // NOP
}

QString Raft::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kRaft);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kRaft);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kRaft);
    }
    gcode += writer->writeAfterRegion(RegionType::kRaft);
    return gcode;
}

void Raft::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum));

    Distance nozzle_offset = this->getSb()->setting<Distance>(MS::PlatformAdhesion::kRaftBeadWidth);

    m_computed_geometry.append(
        PatternGenerator::GenerateLines(m_geometry.offset(-nozzle_offset / 2), nozzle_offset, Angle(), false));
}

void Raft::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
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
    poo.setInfillParameters(InfillPatterns::kLines, m_geometry, getSb()->setting<Distance>(PS::Infill::kMinPathLength),
                            getSb()->setting<Distance>(PS::Travel::kMinLength));

    poo.setPointParameters(pointOrderOptimization, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                           getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                           getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                           getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                           getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius),
                           getSb()->setting<bool>(PS::Optimizations::kEnablePointOrderSegmentBreaking));

    poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kInfill,
                              static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

    QVector<Polyline> previouslyLinkedLines;
    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline(previouslyLinkedLines);
        if (result.size() > 0) {
            Path newPath = createPath(result);
            if (newPath.size() > 0) {
                PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                      m_sb->setting<Velocity>(PS::Travel::kSpeed));
                current_location = newPath.back()->end();
                previouslyLinkedLines.push_back(result);
                m_paths.push_back(newPath);
            }
        }
    }
}

void Raft::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    // NOP
}

Path Raft::createPath(Polyline line) {
    // Populate the paths.
    Distance default_width = this->getSb()->setting<Distance>(MS::PlatformAdhesion::kRaftBeadWidth);
    Distance default_height = this->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity default_speed = m_sb->setting<Velocity>(PS::Layer::kSpeed);
    Acceleration default_acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kDefault);
    AngularVelocity default_extruder_speed = m_sb->setting<AngularVelocity>(PS::Layer::kExtruderSpeed);
    int material_number = m_sb->setting<int>(MS::MultiMaterial::kInfillNum);

    Path newPath;
    for (int i = 0, end = line.size() - 1; i < end; ++i) {
        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(line[i], line[i + 1]);

        segment->getSb()->setSetting(SS::kWidth, default_width);
        segment->getSb()->setSetting(SS::kHeight, default_height);
        segment->getSb()->setSetting(SS::kSpeed, default_speed);
        segment->getSb()->setSetting(SS::kAccel, default_acceleration);
        segment->getSb()->setSetting(SS::kExtruderSpeed, default_extruder_speed);
        segment->getSb()->setSetting(SS::kMaterialNumber, material_number);
        segment->getSb()->setSetting(SS::kRegionType, RegionType::kRaft);

        newPath.append(segment);
    }

    if (newPath.calculateLength() > m_sb->setting<Distance>(PS::Layer::kMinExtrudeLength)) {
        return newPath;
    }
    else {
        return Path();
    }
}
} // namespace ORNL
