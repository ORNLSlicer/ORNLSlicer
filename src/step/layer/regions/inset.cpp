#include "step/layer/regions/inset.h"

#include <algorithm>
#include <cstddef>

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

#ifdef HAVE_SINGLE_PATH
    #include <single_path/single_path.h>
Q_DECLARE_METATYPE(QList<SinglePath::Bridge>);
#endif

namespace ORNL {
Inset::Inset(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, index, settings_polygons) {
    // NOP
}

QString Inset::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kInset);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kInset);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kInset);
    }
    gcode += writer->writeAfterRegion(RegionType::kInset);
    return gcode;
}

void Inset::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    m_paths.clear();
    m_outer_most_path_set.clear();
    m_inner_most_path_set.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kInsetNum));

    Distance beadWidth = m_sb->setting<Distance>(PS::Inset::kBeadWidth);
    int rings = m_sb->setting<int>(PS::Inset::kCount);

    PolygonList path_line = m_geometry.offset(-beadWidth / 2);

    Distance overlap = m_sb->setting<Distance>(PS::Inset::kOverlap);
    if (overlap > 0) {
        path_line = path_line.offset(overlap);
    }

    int ring_nr = 0;
    while (!path_line.isEmpty() && ring_nr < rings) {
        for (Polygon poly : path_line) {
            Polyline line = poly.toPolyline();
            line.pop_back();
            m_computed_geometry.push_back(line);
        }

        ring_nr++;

        m_geometry = path_line.offset(-beadWidth / 2., -beadWidth / 2.);
        path_line = path_line.offset(-beadWidth, -beadWidth / 2.);
    }
}

void Inset::optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
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

    if (static_cast<PrintDirection>(m_sb->setting<int>(PS::Ordering::kInsetReverseDirection)) !=
        PrintDirection::kReverse_off) {
        for (Polyline& line : m_computed_geometry) {
            line = line.reverse();
        }
    }

    poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kInset,
                              static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline();

        // Exit early if no inset path can be made
        if (result.size() < 3) {
            continue;
        }

        // Create path from polyline
        Path newPath = createPath(result);
        newPath.setCCW(result.orientation());

        // Exit early if inset path is too short
        if (newPath.calculateLength() < m_sb->setting<Distance>(PS::Inset::kMinPathLength)) {
            continue;
        }

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

Path Inset::createPath(Polyline line) {
    // ---------- No Settings Regions ----------
    if (m_settings_polygons.isEmpty()) {
        Path path;

        for (size_t i = 0; i < line.size(); ++i) {
            LSegmentPtr segment = LSegmentPtr::create(line[i], line[(i + 1) % line.size()]);
            populateSegmentSettings(segment->getSb(), m_sb);
            path.append(segment);
        }
    }

    // ---------- Settings Regions ----------
    return createPathWithLocalizedSettings(line);
}

#ifdef HAVE_SINGLE_PATH
void Inset::setSinglePathGeometry(QVector<SinglePath::PolygonList> sp_geometry) {
    m_single_path_geometry = sp_geometry;
}

void Inset::createSinglePaths() {
    Distance perim_width = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
    Distance perim_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity perim_speed = m_sb->setting<Velocity>(PS::Perimeter::kSpeed);
    Acceleration perim_acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kPerimeter);
    AngularVelocity perim_extruder_speed = m_sb->setting<AngularVelocity>(PS::Perimeter::kExtruderSpeed);

    Distance inset_width = m_sb->setting<Distance>(PS::Inset::kBeadWidth);
    Distance inset_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    Velocity inset_speed = m_sb->setting<Velocity>(PS::Inset::kSpeed);
    Acceleration inset_acceleration = m_sb->setting<Acceleration>(PRS::Acceleration::kInset);
    AngularVelocity inset_extruder_speed = m_sb->setting<AngularVelocity>(PS::Inset::kExtruderSpeed);

    for (SinglePath::PolygonList polygonList : m_single_path_geometry) {
        for (SinglePath::Polygon polygon : polygonList) {
            Path new_path;
            for (int i = 0; i < polygon.size() - 1; i++) {
                SinglePath::Point start = polygon[i];
                SinglePath::Point end = polygon[i + 1];

                QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(start, end);

                if (start.getRegionType() != end.getRegionType()) // This bridge jumps regions
                {
                    segment->getSb()->setSetting(SS::kWidth, perim_width);
                    segment->getSb()->setSetting(SS::kHeight, perim_height);
                    segment->getSb()->setSetting(SS::kSpeed, perim_speed);
                    segment->getSb()->setSetting(SS::kAccel, perim_acceleration);
                    segment->getSb()->setSetting(SS::kExtruderSpeed, perim_extruder_speed);
                    segment->getSb()->setSetting(SS::kRegionType, RegionType::kPerimeter);
                }
                else {
                    segment->getSb()->setSetting(
                        SS::kWidth,
                        (start.getRegionType() == SinglePath::RegionType::kPerimeter) ? perim_width : inset_width);
                    segment->getSb()->setSetting(
                        SS::kHeight,
                        (start.getRegionType() == SinglePath::RegionType::kPerimeter) ? perim_height : inset_height);
                    segment->getSb()->setSetting(
                        SS::kSpeed,
                        (start.getRegionType() == SinglePath::RegionType::kPerimeter) ? perim_speed : inset_speed);
                    segment->getSb()->setSetting(
                        SS::kAccel, (start.getRegionType() == SinglePath::RegionType::kPerimeter) ? perim_acceleration
                                                                                                  : inset_acceleration);
                    segment->getSb()->setSetting(SS::kExtruderSpeed,
                                                 (start.getRegionType() == SinglePath::RegionType::kPerimeter)
                                                     ? perim_extruder_speed
                                                     : inset_extruder_speed);
                    segment->getSb()->setSetting(SS::kRegionType,
                                                 (start.getRegionType() == SinglePath::RegionType::kPerimeter)
                                                     ? RegionType::kPerimeter
                                                     : RegionType::kInset);
                }
                new_path.append(segment);
            }

            //! \note Close Polygon
            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(polygon.last(), polygon.first());
            segment->getSb()->setSetting(
                SS::kWidth,
                (polygon.last().getRegionType() == SinglePath::RegionType::kPerimeter) ? perim_width : inset_width);
            segment->getSb()->setSetting(
                SS::kHeight,
                (polygon.last().getRegionType() == SinglePath::RegionType::kPerimeter) ? perim_height : inset_height);
            segment->getSb()->setSetting(
                SS::kSpeed,
                (polygon.last().getRegionType() == SinglePath::RegionType::kPerimeter) ? perim_speed : inset_speed);
            segment->getSb()->setSetting(SS::kAccel,
                                         (polygon.last().getRegionType() == SinglePath::RegionType::kPerimeter)
                                             ? perim_acceleration
                                             : inset_acceleration);
            segment->getSb()->setSetting(SS::kExtruderSpeed,
                                         (polygon.last().getRegionType() == SinglePath::RegionType::kPerimeter)
                                             ? perim_extruder_speed
                                             : inset_extruder_speed);
            segment->getSb()->setSetting(SS::kRegionType,
                                         (polygon.last().getRegionType() == SinglePath::RegionType::kPerimeter)
                                             ? RegionType::kPerimeter
                                             : RegionType::kInset);

            new_path.append(segment);
            if (new_path.calculateLength() > m_sb->setting<Distance>(PS::Perimeter::kMinPathLength))
                m_paths.append(new_path);
        }
    }
}
#endif

QVector<Path>& Inset::getOuterMostPathSet() { return m_outer_most_path_set; }

QVector<Path>& Inset::getInnerMostPathSet() { return m_inner_most_path_set; }

QVector<Polyline> Inset::getComputedGeometry() { return m_computed_geometry; }

void Inset::calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) {
    if (m_sb->setting<bool>(ES::Ramping::kTrajectoryAngleEnabled)) {
        PathModifierGenerator::GenerateTrajectorySlowdown(path, m_sb);
    }

    // add the modifiers
    if (m_sb->setting<bool>(MS::Slowdown::kInsetEnable)) {
        PathModifierGenerator::GenerateSlowdown(path, m_sb->setting<Distance>(MS::Slowdown::kInsetDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kInsetLiftDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kInsetCutoffDistance),
                                                m_sb->setting<Velocity>(MS::Slowdown::kInsetSpeed),
                                                m_sb->setting<AngularVelocity>(MS::Slowdown::kInsetExtruderSpeed),
                                                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                                                m_sb->setting<double>(MS::Slowdown::kSlowDownAreaModifier));
    }
    if (m_sb->setting<bool>(MS::TipWipe::kInsetEnable)) {
        if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInsetDirection)) ==
                TipWipeDirection::kForward ||
            static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInsetDirection)) ==
                TipWipeDirection::kOptimal)
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInsetDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kInsetSpeed), m_sb->setting<Angle>(MS::TipWipe::kInsetAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kInsetExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kInsetLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kInsetCutoffDistance));
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInsetDirection)) ==
                 TipWipeDirection::kAngled) {
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kAngledTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInsetDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kInsetSpeed), m_sb->setting<Angle>(MS::TipWipe::kInsetAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kInsetExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kInsetLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kInsetCutoffDistance));
        }
        else
            PathModifierGenerator::GenerateTipWipe(
                path, PathModifiers::kReverseTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInsetDistance),
                m_sb->setting<Velocity>(MS::TipWipe::kInsetSpeed), m_sb->setting<Angle>(MS::TipWipe::kInsetAngle),
                m_sb->setting<AngularVelocity>(MS::TipWipe::kInsetExtruderSpeed),
                m_sb->setting<Distance>(MS::TipWipe::kInsetLiftHeight),
                m_sb->setting<Distance>(MS::TipWipe::kInsetCutoffDistance));
    }
    if (m_sb->setting<bool>(MS::SpiralLift::kInsetEnable)) {
        PathModifierGenerator::GenerateSpiralLift(path, m_sb->setting<Distance>(MS::SpiralLift::kLiftRadius),
                                                  m_sb->setting<Distance>(MS::SpiralLift::kLiftHeight),
                                                  m_sb->setting<int>(MS::SpiralLift::kLiftPoints),
                                                  m_sb->setting<Velocity>(MS::SpiralLift::kLiftSpeed), supportsG3);
    }
    if (m_sb->setting<bool>(MS::Startup::kInsetEnable)) {
        if (m_sb->setting<bool>(MS::Startup::kInsetRampUpEnable)) {
            PathModifierGenerator::GenerateInitialStartupWithRampUp(
                path, m_sb->setting<Distance>(MS::Startup::kInsetDistance),
                m_sb->setting<Velocity>(MS::Startup::kInsetSpeed), m_sb->setting<Velocity>(PS::Inset::kSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kInsetExtruderSpeed),
                m_sb->setting<AngularVelocity>(PS::Inset::kExtruderSpeed), m_sb->setting<int>(MS::Startup::kInsetSteps),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
        else {
            PathModifierGenerator::GenerateInitialStartup(
                path, m_sb->setting<Distance>(MS::Startup::kInsetDistance),
                m_sb->setting<Velocity>(MS::Startup::kInsetSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kInsetExtruderSpeed),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
    }
}

Path Inset::createPathWithLocalizedSettings(const Polyline& line) {
    Path path;

    // Iterate through each segment of the polyline
    for (size_t i = 0; i < line.size(); ++i) {
        const Point& start = line[i];
        const Point& end = line[(i + 1) % line.size()];

        // Clip the segment against the settings polygons
        QVector<Point> cuts;
        for (const SettingsPolygon& polygon : m_settings_polygons) {
            cuts += polygon.clipLine(start, end);
        }

        // Sort cuts based on their distance from the start point
        std::sort(cuts.begin(), cuts.end(),
                  [start](const Point& a, const Point& b) { return start.distance(a) < start.distance(b); });

        // Create an ordered list of points including start, cuts, and end
        QVector<Point> points;
        points << start << cuts << end;

        // Assemble subsegments from the points and apply regional settings
        for (size_t j = 0; j + 1 < points.size(); ++j) {
            const Point& p0 = points[j];
            const Point& p1 = points[j + 1];
            const Point mid = (p0 + p1) * 0.5;

            // Assign the subsegment default settings from the main settings base
            QSharedPointer<SettingsBase> parent_sb = QSharedPointer<SettingsBase>::create(*m_sb);

            // Populate the subsegment settings with local settings
            for (const SettingsPolygon& polygon : m_settings_polygons) {
                if (polygon.inside(mid)) {
                    parent_sb->populate(polygon.getSettings());
                    break;
                }
            }

            LSegmentPtr segment = LSegmentPtr::create(p0, p1);
            populateSegmentSettings(segment->getSb(), parent_sb);
            path.append(segment);
        }
    }
    return path;
}

void Inset::populateSegmentSettings(QSharedPointer<SettingsBase> segment_sb,
                                    const QSharedPointer<SettingsBase>& parent_sb) {
    // Populate segment settings with the provided settings base
    segment_sb->populate(parent_sb);

    segment_sb->setSetting(SS::kWidth, parent_sb->setting<Distance>(PS::Inset::kBeadWidth));
    segment_sb->setSetting(SS::kHeight, parent_sb->setting<Distance>(PS::Layer::kLayerHeight));
    segment_sb->setSetting(SS::kSpeed, parent_sb->setting<Velocity>(PS::Inset::kSpeed));
    segment_sb->setSetting(SS::kAccel, parent_sb->setting<Acceleration>(PRS::Acceleration::kInset));
    segment_sb->setSetting(SS::kExtruderSpeed, parent_sb->setting<AngularVelocity>(PS::Inset::kExtruderSpeed));
    segment_sb->setSetting(SS::kMaterialNumber, parent_sb->setting<int>(MS::MultiMaterial::kInsetNum));
    segment_sb->setSetting(SS::kRegionType, RegionType::kInset);
}
} // namespace ORNL
