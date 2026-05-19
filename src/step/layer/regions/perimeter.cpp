#include "step/layer/regions/perimeter.h"

#include <algorithm>
#include <cstddef>

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "algorithms/knn.h"
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
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
Perimeter::Perimeter(const QSharedPointer<SettingsBase>& sb, const int index,
                     const QVector<SettingsPolygon>& settings_polygons, PolygonList uncut_geometry)
    : RegionBase(sb, index, settings_polygons, uncut_geometry) {}

QString Perimeter::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;
    gcode += writer->writeBeforeRegion(RegionType::kPerimeter);
    for (Path path : m_paths) {
        gcode += writer->writeBeforePath(RegionType::kPerimeter);
        for (QSharedPointer<SegmentBase> segment : path.getSegments()) {
            gcode += segment->writeGCode(writer);
        }
        gcode += writer->writeAfterPath(RegionType::kPerimeter);
    }
    gcode += writer->writeAfterRegion(RegionType::kPerimeter);
    return gcode;
}

void Perimeter::compute(uint layer_num) {
    m_paths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum));
    Distance beadWidth = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
    int rings = m_sb->setting<int>(PS::Perimeter::kCount);

    PolygonList path_line = m_geometry.offset(-beadWidth / 2);

    QVector<Point> previousPoints;
    QVector<Point> currentPoints;
    for (Polygon& poly : m_geometry) {
        for (Point& p : poly) {
            previousPoints.push_back(p);
        }
    }

    int ring_nr = 0;
    while (!path_line.isEmpty() && ring_nr < rings) {
        for (Polygon& poly : path_line) {
            for (Point& p : poly) {
                kNN neighbor(previousPoints, QVector<Point> {p}, 1);
                neighbor.execute();

                int closest = neighbor.getNearestIndices().first();
                p.setNormals(previousPoints[closest].getNormals());
                currentPoints.push_back(p);
            }
        }
        previousPoints = currentPoints;
        currentPoints.clear();

        for (Polygon poly : path_line) {
            Polyline line = poly.toPolyline();
            line.pop_back();
            m_computed_geometry.push_back(line);
        }

        ring_nr++;

        m_geometry = path_line.offset(-beadWidth / 2, -beadWidth / 2);
        path_line = path_line.offset(-beadWidth, -beadWidth / 2);
    }
}

void Perimeter::optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) {
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

    if (m_sb->setting<bool>(PS::SpecialModes::kEnableSpiralize)) {
        if (m_computed_geometry.size() > 0) {
            poo.setGeometryToEvaluate(
                m_computed_geometry, RegionType::kPerimeter,
                static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

            Polyline result = poo.linkSpiralPolyline2D(m_was_last_region_spiral,
                                                       m_sb->setting<Distance>(PS::Layer::Layer::kLayerHeight),
                                                       pointOrderOptimization);

            // Exit early if no perimeter path can be made
            if (result.size() < 3) {
                return;
            }

            // Create path from polyline
            Path newPath = createPath(result);
            newPath.setCCW(result.orientation()); // Set orientation of path
            newPath.getSegments().removeLast();   // Remove last segment of path to enable spiral path linking

            // Exit early if perimeter path is too short
            if (newPath.calculateLength() < m_sb->setting<Distance>(PS::Perimeter::kMinPathLength)) {
                return;
            }

            if (!m_was_last_region_spiral) {
                PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                      m_sb->setting<Velocity>(PS::Travel::kSpeed));
            }

            // Update current location and add path to list of paths
            current_location = newPath.back()->end();
            m_paths.push_back(newPath);
        }
    }
    else {
        if (static_cast<PrintDirection>(m_sb->setting<int>(PS::Ordering::kPerimeterReverseDirection)) !=
            PrintDirection::kReverse_off)
            for (Polyline& line : m_computed_geometry) {
                line = line.reverse();
            }

        poo.setGeometryToEvaluate(
            m_computed_geometry, RegionType::kPerimeter,
            static_cast<PathOrderOptimization>(m_sb->setting<int>(PS::Optimizations::kPathOrder)));

        while (poo.getCurrentPolylineCount() > 0) {
            Polyline result = poo.linkNextPolyline();

            // Exit early if no perimeter path can be made
            if (result.size() < 3) {
                continue;
            }

            // Create path from polyline
            Path newPath = createPath(result);
            newPath.setCCW(result.orientation());

            // Exit early if perimeter path is too short
            if (newPath.calculateLength() < m_sb->setting<Distance>(PS::Perimeter::kMinPathLength)) {
                continue;
            }

            if (newPath.size() > 0) {
                bool shouldRotate = m_sb->setting<bool>(PRS::MachineSetup::kSupportsE2);
                bool shouldTilt = m_sb->setting<bool>(PRS::MachineSetup::kSupportsE1);

                if (shouldTilt || shouldRotate) {
                    Point rotation_origin = Point(m_sb->setting<Distance>(ES::RotationOrigin::kXOffset),
                                                  m_sb->setting<Distance>(ES::RotationOrigin::kYOffset));

                    PathModifierGenerator::GenerateRotationAndTilt(newPath, rotation_origin, shouldRotate,
                                                                   shouldNextPathBeCCW, shouldTilt);
                }
                calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3));
                PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                      m_sb->setting<Velocity>(PS::Travel::kSpeed));

                // Update current location and add path to list of paths
                current_location = newPath.back()->end();
                m_paths.push_back(newPath);
            }
        }
    }
}

Path Perimeter::createPath(Polyline line) {
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

QVector<Polyline> Perimeter::getComputedGeometry() { return m_computed_geometry; }

void Perimeter::calculateModifiers(Path& path, bool supportsG3) {
    if (m_sb->setting<bool>(ES::Ramping::kTrajectoryAngleEnabled)) {
        PathModifierGenerator::GenerateTrajectorySlowdown(path, m_sb);
    }

    if (m_sb->setting<bool>(MS::Slowdown::kPerimeterEnable)) {
        PathModifierGenerator::GenerateSlowdown(path, m_sb->setting<Distance>(MS::Slowdown::kPerimeterDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kPerimeterLiftDistance),
                                                m_sb->setting<Distance>(MS::Slowdown::kPerimeterCutoffDistance),
                                                m_sb->setting<Velocity>(MS::Slowdown::kPerimeterSpeed),
                                                m_sb->setting<AngularVelocity>(MS::Slowdown::kPerimeterExtruderSpeed),
                                                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                                                m_sb->setting<double>(MS::Slowdown::kSlowDownAreaModifier));
    }
    if (m_sb->setting<bool>(MS::TipWipe::kPerimeterEnable)) {
        if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kPerimeterDirection)) ==
                TipWipeDirection::kForward ||
            static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kPerimeterDirection)) ==
                TipWipeDirection::kOptimal)
            PathModifierGenerator::GenerateTipWipe(path, PathModifiers::kForwardTipWipe,
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterDistance),
                                                   m_sb->setting<Velocity>(MS::TipWipe::kPerimeterSpeed),
                                                   m_sb->setting<Angle>(MS::TipWipe::kPerimeterAngle),
                                                   m_sb->setting<AngularVelocity>(MS::TipWipe::kPerimeterExtruderSpeed),
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterLiftHeight),
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterCutoffDistance));
        else if (static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kPerimeterDirection)) ==
                 TipWipeDirection::kAngled) {
            PathModifierGenerator::GenerateTipWipe(path, PathModifiers::kAngledTipWipe,
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterDistance),
                                                   m_sb->setting<Velocity>(MS::TipWipe::kPerimeterSpeed),
                                                   m_sb->setting<Angle>(MS::TipWipe::kPerimeterAngle),
                                                   m_sb->setting<AngularVelocity>(MS::TipWipe::kPerimeterExtruderSpeed),
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterLiftHeight),
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterCutoffDistance));
        }
        else
            PathModifierGenerator::GenerateTipWipe(path, PathModifiers::kReverseTipWipe,
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterDistance),
                                                   m_sb->setting<Velocity>(MS::TipWipe::kPerimeterSpeed),
                                                   m_sb->setting<Angle>(MS::TipWipe::kPerimeterAngle),
                                                   m_sb->setting<AngularVelocity>(MS::TipWipe::kPerimeterExtruderSpeed),
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterLiftHeight),
                                                   m_sb->setting<Distance>(MS::TipWipe::kPerimeterCutoffDistance));
    }
    if (m_sb->setting<bool>(MS::SpiralLift::kPerimeterEnable)) {
        PathModifierGenerator::GenerateSpiralLift(path, m_sb->setting<Distance>(MS::SpiralLift::kLiftRadius),
                                                  m_sb->setting<Distance>(MS::SpiralLift::kLiftHeight),
                                                  m_sb->setting<int>(MS::SpiralLift::kLiftPoints),
                                                  m_sb->setting<Velocity>(MS::SpiralLift::kLiftSpeed), supportsG3);
    }
    if (m_sb->setting<bool>(MS::Startup::kPerimeterEnable)) {
        if (m_sb->setting<bool>(MS::Startup::kPerimeterRampUpEnable)) {
            PathModifierGenerator::GenerateInitialStartupWithRampUp(
                path, m_sb->setting<Distance>(MS::Startup::kPerimeterDistance),
                m_sb->setting<Velocity>(MS::Startup::kPerimeterSpeed), m_sb->setting<Velocity>(PS::Perimeter::kSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kPerimeterExtruderSpeed),
                m_sb->setting<AngularVelocity>(PS::Perimeter::kExtruderSpeed),
                m_sb->setting<int>(MS::Startup::kPerimeterSteps),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
        else {
            PathModifierGenerator::GenerateInitialStartup(
                path, m_sb->setting<Distance>(MS::Startup::kPerimeterDistance),
                m_sb->setting<Velocity>(MS::Startup::kPerimeterSpeed),
                m_sb->setting<AngularVelocity>(MS::Startup::kPerimeterExtruderSpeed),
                m_sb->setting<bool>(PS::SpecialModes::kEnableWidthHeight),
                m_sb->setting<double>(MS::Startup::kStartUpAreaModifier));
        }
    }
    if (m_sb->setting<bool>(PS::Perimeter::kEnableFlyingStart)) {
        PathModifierGenerator::GenerateFlyingStart(path, m_sb->setting<Distance>(PS::Perimeter::kFlyingStartDistance),
                                                   m_sb->setting<Velocity>(PS::Perimeter::kFlyingStartSpeed));
    }
}

Path Perimeter::createPathWithLocalizedSettings(const Polyline& line) {
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

void Perimeter::populateSegmentSettings(QSharedPointer<SettingsBase> segment_sb,
                                        const QSharedPointer<SettingsBase>& parent_sb) {
    // Populate segment settings with the provided settings base
    segment_sb->populate(parent_sb);

    segment_sb->setSetting(SS::kWidth, parent_sb->setting<Distance>(PS::Perimeter::kBeadWidth));
    segment_sb->setSetting(SS::kHeight, parent_sb->setting<Distance>(PS::Layer::kLayerHeight));
    segment_sb->setSetting(SS::kSpeed, parent_sb->setting<Velocity>(PS::Perimeter::kSpeed));
    segment_sb->setSetting(SS::kAccel, parent_sb->setting<Acceleration>(PRS::Acceleration::kPerimeter));
    segment_sb->setSetting(SS::kExtruderSpeed, parent_sb->setting<AngularVelocity>(PS::Perimeter::kExtruderSpeed));
    segment_sb->setSetting(SS::kMaterialNumber, parent_sb->setting<int>(MS::MultiMaterial::kPerimeterNum));
    segment_sb->setSetting(SS::kRegionType, RegionType::kPerimeter);
}
} // namespace ORNL
