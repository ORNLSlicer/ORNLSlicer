#include "step/layer/regions/perimeter.h"

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
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
void appendPathLines(PolygonList& path_lines, QVector<Polyline>& computed_geometry) {
    for (Polygon& poly : path_lines) {
        Polyline line = poly.toPolyline();
        line.pop_back();
        computed_geometry.push_back(line);
    }
}

PolygonList selectedBoundaryOffsetGeometry(const PolygonList& external_boundaries,
                                           const PolygonList& internal_boundaries, PerimeterBoundarySelection selection,
                                           Distance offset_distance) {
    if (selection == PerimeterBoundarySelection::kInternal) {
        return external_boundaries - internal_boundaries.offset(offset_distance);
    }

    PolygonList offset_external_geometry = external_boundaries.offset(-offset_distance);
    PolygonList offset_geometry = offset_external_geometry - internal_boundaries;
    offset_geometry.lost_geometry = offset_external_geometry.lost_geometry;
    return offset_geometry;
}

Distance closedPolylineLength(const Polyline& line) {
    if (line.size() < 2) {
        return 0;
    }

    return line.length() + line.back().distance(line.front());
}

Point pointAlongSegment(const Point& start, const Point& end, double ratio) {
    return Point(start.x() + ((end.x() - start.x()) * ratio), start.y() + ((end.y() - start.y()) * ratio),
                 start.z() + ((end.z() - start.z()) * ratio));
}

Point stopPointOnClosingSegment(const Polyline& line, Distance distance_before_start) {
    if (line.isEmpty()) {
        return Point();
    }

    const Point segment_start = line.back();
    const Point segment_end = line.front();
    const Distance segment_length = segment_start.distance(segment_end);

    if (segment_length <= distance_before_start) {
        return segment_start;
    }

    const double ratio = (segment_length() - distance_before_start()) / segment_length();
    return pointAlongSegment(segment_start, segment_end, ratio);
}

bool hasSmoothClosingSegment(const Polyline& line) {
    if (line.size() < 3) {
        return false;
    }

    return MathUtils::nearCollinear(line[line.size() - 2], line.back(), line.front(), 45 * deg);
}

Polyline buildSpiralPerimeterPolyline(const QVector<Polyline>& ordered_perimeters, Distance final_stop_distance) {
    Polyline spiral;

    for (int perimeter_index = 0, end = ordered_perimeters.size(); perimeter_index < end; ++perimeter_index) {
        const Polyline& perimeter = ordered_perimeters[perimeter_index];
        if (perimeter.size() < 3) {
            continue;
        }

        spiral += perimeter;

        if (perimeter_index + 1 < end) {
            const Point next_start = ordered_perimeters[perimeter_index + 1].front();
            Point connector_start;
            if (hasSmoothClosingSegment(perimeter)) {
                connector_start = stopPointOnClosingSegment(perimeter, final_stop_distance);
            }
            else {
                auto [projected_connector_start, distance] =
                    MathUtils::nearestPointOnSegment(perimeter.back(), perimeter.front(), next_start);
                Q_UNUSED(distance)
                connector_start = projected_connector_start;
            }

            if (spiral.back() != connector_start) {
                spiral += connector_start;
            }
        }
        else {
            const Point final_stop = stopPointOnClosingSegment(perimeter, final_stop_distance);

            if (spiral.back() != final_stop) {
                spiral += final_stop;
            }
        }
    }

    return spiral;
}
} // namespace

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
    int perimeter_count = m_sb->setting<int>(PS::Perimeter::kCount);
    const PerimeterBoundarySelection boundary_selection =
        static_cast<PerimeterBoundarySelection>(m_sb->setting<int>(PS::Perimeter::kBoundarySelection));

    const PolygonList original_geometry = m_geometry;
    if (boundary_selection == PerimeterBoundarySelection::kAll) {
        PolygonList path_lines = m_geometry.offset(-beadWidth / 2);

        for (int perimeter_number = 0; !path_lines.isEmpty() && perimeter_number < perimeter_count;
             ++perimeter_number) {
            appendPathLines(path_lines, m_computed_geometry);

            m_geometry = path_lines.offset(-beadWidth / 2, -beadWidth / 2);
            path_lines = path_lines.offset(-beadWidth, -beadWidth / 2);
        }
        return;
    }

    // Selective modes offset the full part mask and then extract the requested boundary type. Offsetting a hole or an
    // outline as a standalone polygon would send paths into void space or through holes because the opposite boundary
    // would no longer clip the offset.
    const PolygonList external_boundaries = original_geometry.externalPolygonBoundaries();
    const PolygonList internal_boundaries = original_geometry.internalPolygonBoundaries();

    for (int perimeter_number = 0; perimeter_number < perimeter_count; ++perimeter_number) {
        const Distance path_offset = beadWidth * perimeter_number + beadWidth / 2;
        PolygonList offset_geometry =
            selectedBoundaryOffsetGeometry(external_boundaries, internal_boundaries, boundary_selection, path_offset);
        PolygonList path_lines = boundary_selection == PerimeterBoundarySelection::kInternal
                                     ? offset_geometry.internalPolygonBoundaries()
                                     : offset_geometry.externalPolygonBoundaries();
        if (path_lines.isEmpty()) {
            break;
        }

        appendPathLines(path_lines, m_computed_geometry);

        const Distance remaining_offset = beadWidth * (perimeter_number + 1);
        m_geometry = selectedBoundaryOffsetGeometry(external_boundaries, internal_boundaries, boundary_selection,
                                                    remaining_offset);
    }
}

void Perimeter::optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) {
    Q_UNUSED(shouldNextPathBeCCW)

    PathOrderOptimization pathOrderOptimization =
        static_cast<PathOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPathOrder));

    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPointOrder));

    auto configureOptimizer = [&](PolylineOrderOptimizer& optimizer, PointOrderOptimization point_order) {
        if (pathOrderOptimization == PathOrderOptimization::kCustomPoint) {
            Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPathXLocation),
                                getSb()->setting<double>(PS::Optimizations::kCustomPathYLocation));

            optimizer.setStartOverride(startOverride);
        }

        if (point_order == PointOrderOptimization::kCustomPoint) {
            Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomPointXLocation),
                                getSb()->setting<double>(PS::Optimizations::kCustomPointYLocation));

            optimizer.setStartPointOverride(startOverride);
        }

        optimizer.setPointParameters(point_order, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                                     getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                                     getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                                     getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                                     getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius),
                                     getSb()->setting<bool>(PS::Optimizations::kEnablePointOrderSegmentBreaking));
    };

    PolylineOrderOptimizer poo(current_location, layerNumber);
    configureOptimizer(poo, pointOrderOptimization);

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

        if (m_sb->setting<bool>(PS::Perimeter::kEnableSpiralPerimeter)) {
            Point spiral_query_location = current_location;
            PolylineOrderOptimizer spiral_poo(spiral_query_location, layerNumber);
            configureOptimizer(spiral_poo, pointOrderOptimization);
            spiral_poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kPerimeter, pathOrderOptimization);

            QVector<Polyline> ordered_perimeters;
            const Distance min_path_length = m_sb->setting<Distance>(PS::Perimeter::kMinPathLength);

            while (spiral_poo.getCurrentPolylineCount() > 0) {
                if (!ordered_perimeters.isEmpty()) {
                    spiral_poo.setPointParameters(PointOrderOptimization::kNextClosest, false, 0, 0, false, 0, true);
                }

                Polyline result = spiral_poo.linkNextPolyline();

                if (result.size() < 3 || closedPolylineLength(result) < min_path_length) {
                    continue;
                }

                // Keep loop choice near the local seam to avoid jumping across the island on the next transition.
                spiral_query_location = result.front();
                ordered_perimeters.push_back(result);
            }

            if (ordered_perimeters.isEmpty()) {
                return;
            }

            Polyline result =
                buildSpiralPerimeterPolyline(ordered_perimeters, m_sb->setting<Distance>(PS::Perimeter::kBeadWidth));

            if (result.size() < 3) {
                return;
            }

            Path newPath = createPath(result);
            newPath.setCCW(ordered_perimeters.front().orientation());

            if (newPath.size() > 0) {
                newPath.getSegments().removeLast();
            }

            if (newPath.calculateLength() < min_path_length) {
                return;
            }

            if (newPath.size() > 0) {
                calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3));
                PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                      m_sb->setting<Velocity>(PS::Travel::kSpeed));

                current_location = newPath.back()->end();
                m_paths.push_back(newPath);
            }

            return;
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
