#include "step/layer/regions/inset.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

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
#include "geometry/spiral_path.h"
#include "optimizers/polyline_order_optimizer.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
Polyline toOpenPolyline(Polygon poly) {
    Polyline line = poly.toPolyline();
    if (!line.isEmpty()) { line.pop_back(); }
    return line;
}

bool isValidInsetLine(const Polyline& line, Distance min_path_length) {
    return line.size() >= 3 && SpiralPath::closedPolylineLength(line) >= min_path_length;
}

QVector<Polygon> appendValidPathLines(const PolygonList& path_lines, QVector<Polyline>& computed_geometry,
                                      QVector<Distance>& computed_widths, Distance min_path_length, Distance bead_width,
                                      Distance min_segment_length, bool& skipped_path_line) {
    QVector<Polygon> valid_path_lines;

    for (const Polygon& poly : path_lines) {
        Polyline line = toOpenPolyline(poly).removeShortSegments(min_segment_length, true);
        if (!isValidInsetLine(line, min_path_length)) {
            skipped_path_line = true;
            continue;
        }

        computed_geometry.push_back(line);
        computed_widths.push_back(bead_width);
        valid_path_lines.push_back(poly);
    }

    return valid_path_lines;
}

QVector<Polygon> validPathLines(const PolygonList& path_lines, Distance min_path_length, Distance min_segment_length) {
    QVector<Polygon> valid_path_lines;

    for (const Polygon& poly : path_lines) {
        if (isValidInsetLine(toOpenPolyline(poly).removeShortSegments(min_segment_length, true), min_path_length)) {
            valid_path_lines.push_back(poly);
        }
    }

    return valid_path_lines;
}

Distance totalPathLineLength(const QVector<Polygon>& path_lines) {
    Distance total_length;
    for (const Polygon& poly : path_lines) { total_length += SpiralPath::closedPolylineLength(toOpenPolyline(poly)); }
    return total_length;
}

PolygonList pathLineFootprint(const Polygon& path_line, Distance bead_width, const PolygonList& clipping_geometry) {
    PolygonList outer_offset = path_line.offset(bead_width / 2);
    PolygonList inner_offset = path_line.offset(-bead_width / 2);
    PolygonList footprint    = outer_offset ^ inner_offset;

    return footprint & clipping_geometry;
}

bool subtractPathLineFootprints(PolygonList& geometry, const QVector<Polygon>& path_lines, Distance bead_width) {
    PolygonList path_line_footprint;
    for (const Polygon& poly : path_lines) { path_line_footprint += pathLineFootprint(poly, bead_width, geometry); }

    if (path_line_footprint.isEmpty()) { return false; }

    geometry -= path_line_footprint;
    return true;
}

PolygonList insetPathLines(const PolygonList& geometry, Distance bead_width, Distance overlap) {
    PolygonList path_line = geometry.offset(-bead_width / 2);
    if (overlap > 0) { path_line = path_line.offset(overlap); }

    return path_line;
}

double netAreaAbs(PolygonList geometry) {
    return std::fabs(geometry.netArea()());
}

double negligibleAreaTolerance(double reference_area, Distance nominal_width) {
    return std::max(reference_area * 1.0e-6, nominal_width() * nominal_width() * 1.0e-4);
}

bool hasInternalBoundaries(const PolygonList& geometry) {
    return !geometry.internalPolygonBoundaries().isEmpty();
}

Distance clampedAdaptiveWidth(Distance requested_width, Distance nominal_width, Distance min_width,
                              Distance max_width) {
    double requested = requested_width();
    if (!std::isfinite(requested) || requested <= 0) { requested = nominal_width(); }

    double lower = std::max(0.0, min_width());
    double upper = max_width() > 0 ? max_width() : std::numeric_limits<double>::max();
    if (upper < lower) { upper = lower; }

    return Distance(std::clamp(requested, lower, upper));
}

bool geometryClearedByFootprints(PolygonList geometry, const QVector<Polygon>& path_lines, Distance bead_width,
                                 double reference_area, Distance nominal_width) {
    if (!subtractPathLineFootprints(geometry, path_lines, bead_width)) { return false; }

    return geometry.isEmpty() || netAreaAbs(geometry) <= negligibleAreaTolerance(reference_area, nominal_width);
}

bool shellWidthCoversGeometry(PolygonList geometry, const QVector<Polygon>& path_lines, Distance bead_width,
                              Distance nominal_width) {
    const Distance path_length = totalPathLineLength(path_lines);
    if (path_length <= 0) { return false; }

    const Distance full_area_width = Distance(netAreaAbs(geometry) / path_length());
    return std::fabs(full_area_width() - bead_width()) <= std::max(nominal_width() * 1.0e-4, 1.0e-6);
}

std::optional<Distance> fullCoverageAdaptiveWidth(PolygonList geometry, Distance nominal_width, Distance overlap,
                                                  Distance min_path_length, Distance min_segment_length,
                                                  Distance min_width, Distance max_width) {
    const double initial_area = netAreaAbs(geometry);
    if (initial_area <= 0) { return std::nullopt; }
    const bool shell_geometry = hasInternalBoundaries(geometry);

    Distance candidate_width = nominal_width;
    QVector<Polygon> candidate_path_lines;
    for (int i = 0; i < 4; ++i) {
        candidate_path_lines =
            validPathLines(insetPathLines(geometry, candidate_width, overlap), min_path_length, min_segment_length);
        const Distance candidate_length = totalPathLineLength(candidate_path_lines);
        if (candidate_length <= 0) { return std::nullopt; }

        const Distance next_width =
            clampedAdaptiveWidth(Distance(initial_area / candidate_length()), nominal_width, min_width, max_width);
        if (std::fabs(next_width() - candidate_width()) <= std::max(nominal_width() * 1.0e-4, 1.0e-6)) {
            candidate_width = next_width;
            break;
        }

        candidate_width = next_width;
    }

    candidate_path_lines =
        validPathLines(insetPathLines(geometry, candidate_width, overlap), min_path_length, min_segment_length);
    if (candidate_path_lines.isEmpty()) { return std::nullopt; }

    if (geometryClearedByFootprints(geometry, candidate_path_lines, candidate_width, initial_area, nominal_width)) {
        return candidate_width;
    }
    if (shell_geometry && candidate_path_lines.size() > 1 &&
        shellWidthCoversGeometry(geometry, candidate_path_lines, candidate_width, nominal_width)) {
        return candidate_width;
    }

    return std::nullopt;
}

Distance adaptiveContourWidthForGeometry(PolygonList geometry, Distance nominal_width, int remaining_count,
                                         Distance overlap, Distance min_path_length, Distance min_segment_length,
                                         Distance min_width, Distance max_width) {
    if (std::optional<Distance> full_width = fullCoverageAdaptiveWidth(
            geometry, nominal_width, overlap, min_path_length, min_segment_length, min_width, max_width)) {
        return *full_width;
    }

    PolygonList preview_geometry = geometry;
    Distance preview_length;

    for (int i = 0; i < remaining_count && !preview_geometry.isEmpty(); ++i) {
        QVector<Polygon> preview_path_lines = validPathLines(insetPathLines(preview_geometry, nominal_width, overlap),
                                                             min_path_length, min_segment_length);
        if (preview_path_lines.isEmpty()) { break; }

        preview_length += totalPathLineLength(preview_path_lines);
        if (!subtractPathLineFootprints(preview_geometry, preview_path_lines, nominal_width)) { break; }
    }

    if (preview_length <= 0) { return nominal_width; }

    const bool more_nominal_paths_fit =
        !validPathLines(insetPathLines(preview_geometry, nominal_width, overlap), min_path_length, min_segment_length)
             .isEmpty();
    const double initial_area           = netAreaAbs(geometry);
    const double preview_remaining_area = netAreaAbs(preview_geometry);
    const double target_area =
        more_nominal_paths_fit ? std::max(0.0, initial_area - preview_remaining_area) : initial_area;

    return clampedAdaptiveWidth(Distance(target_area / preview_length()), nominal_width, min_width, max_width);
}

QVector<Distance> plannedAdaptiveContourWidths(PolygonList geometry, Distance nominal_width, int remaining_count,
                                               Distance overlap, Distance min_path_length, Distance min_segment_length,
                                               Distance min_width, Distance max_width) {
    QVector<Distance> widths;

    for (int i = 0; i < remaining_count && !geometry.isEmpty(); ++i) {
        const Distance path_width =
            adaptiveContourWidthForGeometry(geometry, nominal_width, remaining_count - i, overlap, min_path_length,
                                            min_segment_length, min_width, max_width);
        PolygonList path_lines            = insetPathLines(geometry, path_width, overlap);
        QVector<Polygon> valid_path_lines = validPathLines(path_lines, min_path_length, min_segment_length);
        if (valid_path_lines.isEmpty()) { break; }

        widths.push_back(path_width);

        const bool clear_shell_geometry =
            hasInternalBoundaries(geometry) &&
            shellWidthCoversGeometry(geometry, valid_path_lines, path_width, nominal_width);

        if (!subtractPathLineFootprints(geometry, valid_path_lines, path_width)) { break; }
        if (clear_shell_geometry) { geometry.clear(); }
    }

    return widths;
}

Distance adaptiveContourWidth(PolygonList geometry, Distance nominal_width, int remaining_count, Distance overlap,
                              Distance min_path_length, Distance min_segment_length, Distance min_width,
                              Distance max_width) {
    QVector<Distance> widths = plannedAdaptiveContourWidths(geometry, nominal_width, remaining_count, overlap,
                                                            min_path_length, min_segment_length, min_width, max_width);
    if (widths.isEmpty()) { return nominal_width; }

    return *std::max_element(widths.begin(), widths.end(),
                             [](const Distance& lhs, const Distance& rhs) { return lhs() < rhs(); });
}

double distanceXYToSegment(const Point& point, const Point& start, const Point& end) {
    const double dx     = end.x() - start.x();
    const double dy     = end.y() - start.y();
    const double len_sq = dx * dx + dy * dy;
    if (len_sq <= std::numeric_limits<double>::epsilon()) {
        return std::hypot(point.x() - start.x(), point.y() - start.y());
    }

    const double t = std::clamp(((point.x() - start.x()) * dx + (point.y() - start.y()) * dy) / len_sq, 0.0, 1.0);
    const double nearest_x = start.x() + t * dx;
    const double nearest_y = start.y() + t * dy;
    return std::hypot(point.x() - nearest_x, point.y() - nearest_y);
}

bool pointOnClosedPolylineXY(const Point& point, const Polyline& line, double tolerance) {
    if (line.size() < 2) { return false; }

    for (int i = 0; i < line.size(); ++i) {
        if (distanceXYToSegment(point, line[i], line[(i + 1) % line.size()]) <= tolerance) { return true; }
    }

    return false;
}

}  // namespace

Inset::Inset(const QSharedPointer<SettingsBase>& sb, const int index, const QVector<SettingsPolygon>& settings_polygons)
    : RegionBase(sb, index, settings_polygons, PolygonList(), RegionType::kInset) {
    // NOP
}

QString Inset::writeGCode(QSharedPointer<WriterBase> writer) { return writeRegionGCode(writer, RegionType::kInset); }

void Inset::compute(uint layer_num) {
    m_paths.clear();
    m_computed_geometry.clear();
    m_computed_widths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kInsetNum));

    Distance beadWidth = m_sb->setting<Distance>(PS::Inset::kBeadWidth);
    int rings          = m_sb->setting<int>(PS::Inset::kCount);

    Distance overlap                  = m_sb->setting<Distance>(PS::Inset::kOverlap);
    const Distance min_path_length    = m_sb->setting<Distance>(PS::Inset::kMinPathLength);
    const Distance min_segment_length = m_sb->setting<Distance>(PS::Inset::kMinSegmentLength);

    int ring_nr           = 0;
    PolygonList path_line = m_geometry.offset(-beadWidth / 2);
    if (overlap > 0) { path_line = path_line.offset(overlap); }

    if (m_sb->setting<bool>(PS::Inset::kAdaptive)) {
        const Distance min_adaptive_width = m_sb->setting<Distance>(PS::Inset::kAdaptiveMinWidth);
        const Distance max_adaptive_width = m_sb->setting<Distance>(PS::Inset::kAdaptiveMaxWidth);

        while (!m_geometry.isEmpty() && ring_nr < rings) {
            const int remaining_count = rings - ring_nr;
            const Distance path_width =
                adaptiveContourWidth(m_geometry, beadWidth, remaining_count, overlap, min_path_length,
                                     min_segment_length, min_adaptive_width, max_adaptive_width);
            path_line = insetPathLines(m_geometry, path_width, overlap);

            bool skipped_path_line = false;
            QVector<Polygon> valid_path_lines =
                appendValidPathLines(path_line, m_computed_geometry, m_computed_widths, min_path_length, path_width,
                                     min_segment_length, skipped_path_line);

            if (valid_path_lines.isEmpty()) { break; }

            ring_nr++;

            const bool clear_shell_geometry =
                hasInternalBoundaries(m_geometry) &&
                shellWidthCoversGeometry(m_geometry, valid_path_lines, path_width, beadWidth);

            if (!subtractPathLineFootprints(m_geometry, valid_path_lines, path_width)) { break; }
            if (clear_shell_geometry) { m_geometry.clear(); }
        }
        return;
    }

    while (!path_line.isEmpty() && ring_nr < rings) {
        bool skipped_path_line = false;
        QVector<Polygon> valid_path_lines =
            appendValidPathLines(path_line, m_computed_geometry, m_computed_widths, min_path_length, beadWidth,
                                 min_segment_length, skipped_path_line);

        if (valid_path_lines.isEmpty()) { break; }

        ring_nr++;

        if (skipped_path_line) {
            if (!subtractPathLineFootprints(m_geometry, valid_path_lines, beadWidth)) { break; }

            path_line = m_geometry.offset(-beadWidth / 2);
        }
        else {
            m_geometry = path_line.offset(-beadWidth / 2., -beadWidth / 2.);
            path_line  = path_line.offset(-beadWidth, -beadWidth / 2.);
        }
    }
}

void Inset::optimize(int layerNumber, Point& current_location, bool& shouldNextPathBeCCW) {
    PathOrderOptimization pathOrderOptimization = this->pathOrderOptimization();

    PointOrderOptimization pointOrderOptimization =
        static_cast<PointOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kPointOrder));

    auto configureOptimizer = [&](PolylineOrderOptimizer& optimizer, PointOrderOptimization point_order) {
        if (pathOrderOptimization == PathOrderOptimization::kCustomPoint) {
            Point startOverride = customPathOrderPoint();

            optimizer.setStartOverride(startOverride);
        }

        if (usesCustomPointLocation(point_order)) {
            Point startOverride = customPointOrderPoint();

            optimizer.setStartPointOverride(startOverride);
        }

        optimizer.setPointParameters(point_order, getSb()->setting<bool>(PS::Optimizations::kMinDistanceEnabled),
                                     getSb()->setting<Distance>(PS::Optimizations::kMinDistanceThreshold),
                                     getSb()->setting<Distance>(PS::Optimizations::kConsecutiveDistanceThreshold),
                                     getSb()->setting<bool>(PS::Optimizations::kLocalRandomnessEnable),
                                     getSb()->setting<Distance>(PS::Optimizations::kLocalRandomnessRadius),
                                     getSb()->setting<bool>(PS::Optimizations::kEnablePointOrderSegmentBreaking));
        optimizer.setConsecutiveReferencePoint(getPreviousLayerStartPoint());
    };

    PolylineOrderOptimizer poo(current_location, layerNumber);
    configureOptimizer(poo, pointOrderOptimization);

    m_paths.clear();

    if (static_cast<PrintDirection>(m_sb->setting<int>(PS::Ordering::kInsetReverseDirection)) !=
        PrintDirection::kReverse_off) {
        for (Polyline& line : m_computed_geometry) { line = line.reverse(); }
    }

    auto appendSpiralPaths = [&](const QVector<Polyline>& spiral_groups, bool ccw, Distance min_path_length) {
        for (const Polyline& spiral_group : spiral_groups) {
            if (spiral_group.size() < 3) { continue; }

            Path newPath = createPath(spiral_group);
            newPath.setCCW(ccw);

            if (newPath.size() > 0) { newPath.getSegments().removeLast(); }

            if (newPath.calculateLength() < min_path_length) { continue; }

            if (newPath.size() > 0) {
                calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3), true);
                PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                      m_sb->setting<Velocity>(PS::Travel::kSpeed));

                current_location = newPath.back()->end();
                m_paths.push_back(newPath);
            }
        }
    };

    if (m_sb->setting<bool>(PS::Inset::kEnableSpiralInset)) {
        if (!m_sb->setting<bool>(PS::Inset::kAdaptive)) {
            Point spiral_query_location = current_location;
            PolylineOrderOptimizer spiral_poo(spiral_query_location, layerNumber);
            configureOptimizer(spiral_poo, pointOrderOptimization);
            spiral_poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kInset, pathOrderOptimization);

            QVector<Polyline> ordered_insets;
            const Distance min_path_length = m_sb->setting<Distance>(PS::Inset::kMinPathLength);
            const Distance bead_width      = m_sb->setting<Distance>(PS::Inset::kBeadWidth);

            while (spiral_poo.getCurrentPolylineCount() > 0) {
                if (!ordered_insets.isEmpty()) {
                    spiral_poo.setPointParameters(PointOrderOptimization::kNextClosest, false, 0, 0, false, 0, false);
                }

                Polyline result = spiral_poo.linkNextPolyline();

                if (result.size() < 3 || SpiralPath::closedPolylineLength(result) < min_path_length) { continue; }

                spiral_query_location = SpiralPath::transitionStartPoint(result, bead_width);
                ordered_insets.push_back(result);
            }

            if (ordered_insets.isEmpty()) { return; }

            if (ordered_insets.size() == 1) {
                Polyline result = ordered_insets.first();

                Path newPath = createPath(result);
                newPath.setCCW(result.orientation());

                if (newPath.calculateLength() < min_path_length) { return; }

                if (newPath.size() > 0) {
                    calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3));
                    PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                          m_sb->setting<Velocity>(PS::Travel::kSpeed));

                    current_location = newPath.back()->end();
                    m_paths.push_back(newPath);
                }

                return;
            }

            appendSpiralPaths(SpiralPath::linkClosedPolylineGroups(ordered_insets, bead_width),
                              ordered_insets.front().orientation(), min_path_length);

            return;
        }

        Point spiral_query_location = current_location;
        PolylineOrderOptimizer spiral_poo(spiral_query_location, layerNumber);
        configureOptimizer(spiral_poo, pointOrderOptimization);
        spiral_poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kInset, pathOrderOptimization);

        QVector<Polyline> ordered_insets;
        QVector<Distance> ordered_inset_widths;
        bool has_spiral_orientation    = false;
        bool spiral_orientation        = false;
        const Distance min_path_length = m_sb->setting<Distance>(PS::Inset::kMinPathLength);

        while (spiral_poo.getCurrentPolylineCount() > 0) {
            if (!ordered_insets.isEmpty()) {
                spiral_poo.setPointParameters(PointOrderOptimization::kNextClosest, false, 0, 0, false, 0, false);
            }

            Polyline result = spiral_poo.linkNextPolyline();

            if (result.size() < 3 || SpiralPath::closedPolylineLength(result) < min_path_length) { continue; }

            if (!has_spiral_orientation) {
                spiral_orientation     = result.orientation();
                has_spiral_orientation = true;
            }
            else if (result.orientation() != spiral_orientation) {
                result = result.reverse();
                result.move(result.size() - 1, 0);
            }

            const Distance result_width = beadWidthForSegment(result.front(), result[1], m_sb);
            ordered_inset_widths.push_back(result_width);
            ordered_insets.push_back(result);
            spiral_query_location = SpiralPath::transitionStartPoint(result, result_width);
        }

        if (ordered_insets.isEmpty()) { return; }

        if (ordered_insets.size() == 1) {
            PolylineOrderOptimizer first_loop_poo(current_location, layerNumber);
            configureOptimizer(first_loop_poo, pointOrderOptimization);
            first_loop_poo.setGeometryToEvaluate({ordered_insets.first()}, RegionType::kInset,
                                                 PathOrderOptimization::kNextClosest);
            Polyline result = first_loop_poo.linkNextPolyline();
            if (result.size() < 3) { result = ordered_insets.first(); }

            Path newPath = createPath(result);
            newPath.setCCW(result.orientation());

            if (newPath.calculateLength() < min_path_length) { return; }

            if (newPath.size() > 0) {
                calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3));
                PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                      m_sb->setting<Velocity>(PS::Travel::kSpeed));

                current_location = newPath.back()->end();
                m_paths.push_back(newPath);
            }

            return;
        }

        const Distance nominal_width = m_sb->setting<Distance>(PS::Inset::kBeadWidth);
        appendSpiralPaths(SpiralPath::linkClosedPolylineGroups(ordered_insets, ordered_inset_widths, nominal_width),
                          ordered_insets.front().orientation(), min_path_length);

        return;
    }

    poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kInset, pathOrderOptimization);

    while (poo.getCurrentPolylineCount() > 0) {
        Polyline result = poo.linkNextPolyline();

        // Exit early if no inset path can be made
        if (result.size() < 3) { continue; }

        // Create path from polyline
        Path newPath = createPath(result);
        newPath.setCCW(result.orientation());

        // Exit early if inset path is too short
        if (newPath.calculateLength() < m_sb->setting<Distance>(PS::Inset::kMinPathLength)) { continue; }

        if (newPath.size() > 0) {
            calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3));
            PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                  m_sb->setting<Velocity>(PS::Travel::kSpeed));
            current_location = newPath.back()->end();
            m_paths.push_back(newPath);
        }
    }
}

Path Inset::createPath(Polyline line) {
    line = line.removeShortSegments(m_sb->setting<Distance>(PS::Inset::kMinSegmentLength), true);
    if (line.size() < 3) { return Path(); }

    // ---------- No Settings Regions ----------
    if (m_settings_polygons.isEmpty()) {
        Path path;

        for (size_t i = 0; i < line.size(); ++i) {
            const Point& start        = line[i];
            const Point& end          = line[(i + 1) % line.size()];
            const Distance bead_width = beadWidthForSegment(start, end, m_sb);

            LSegmentPtr segment = LSegmentPtr::create(start, end);
            populateSegmentSettings(segment->getSb(), m_sb, bead_width, isAdaptedWidth(bead_width, m_sb));
            path.append(segment);
        }
        return path;
    }

    // ---------- Settings Regions ----------
    return createPathWithLocalizedSettings(line);
}

QVector<Polyline> Inset::getComputedGeometry() {
    return m_computed_geometry;
}

void Inset::calculateModifiers(Path& path, bool supportsG3) {
    calculateModifiers(path, supportsG3, false);
}

void Inset::calculateModifiers(Path& path, bool supportsG3, bool open_loop_tip_wipe) {
    PathModifierGenerator::GenerateSharpCornerExtension(path, m_sb);

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
        const TipWipeDirection wipe_direction =
            static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kInsetDirection));
        if (wipe_direction == TipWipeDirection::kForward ||
            (!open_loop_tip_wipe && wipe_direction == TipWipeDirection::kOptimal)) {
            if (open_loop_tip_wipe && wipe_direction == TipWipeDirection::kForward) {
                PathModifierGenerator::GenerateForwardTipWipeOpenLoop(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInsetDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kInsetSpeed),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kInsetExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kInsetLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kInsetCutoffDistance));
            }
            else {
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kInsetDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kInsetSpeed), m_sb->setting<Angle>(MS::TipWipe::kInsetAngle),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kInsetExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kInsetLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kInsetCutoffDistance));
            }
        }
        else if (wipe_direction == TipWipeDirection::kAngled) {
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
        const Point& end   = line[(i + 1) % line.size()];

        // Clip the segment against the settings polygons
        QVector<Point> cuts;
        for (const SettingsPolygon& polygon : m_settings_polygons) { cuts += polygon.clipLine(start, end); }

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

            LSegmentPtr segment       = LSegmentPtr::create(p0, p1);
            const Distance bead_width = beadWidthForSegment(p0, p1, parent_sb);
            populateSegmentSettings(segment->getSb(), parent_sb, bead_width, isAdaptedWidth(bead_width, parent_sb));
            path.append(segment);
        }
    }
    return path;
}

void Inset::populateSegmentSettings(QSharedPointer<SettingsBase> segment_sb,
                                    const QSharedPointer<SettingsBase>& parent_sb, const Distance& bead_width,
                                    bool adapted) {
    // Populate segment settings with the provided settings base
    segment_sb->populate(parent_sb);

    Velocity speed = parent_sb->setting<Velocity>(PS::Inset::kSpeed);
    if (adapted && bead_width > 0) {
        const Distance ref_width = parent_sb->setting<Distance>(PS::Inset::kBeadWidth);
        const Velocity ref_speed = parent_sb->setting<Velocity>(PS::Inset::kSpeed);
        const double min_speed   = ref_speed() * 0.01;
        speed                    = Velocity(std::max((ref_speed() * ref_width()) / bead_width(), min_speed));
    }

    segment_sb->setSetting(SS::kWidth, bead_width);
    segment_sb->setSetting(SS::kHeight, parent_sb->setting<Distance>(PS::Layer::kLayerHeight));
    segment_sb->setSetting(SS::kSpeed, speed);
    segment_sb->setSetting(SS::kAccel, parent_sb->setting<Acceleration>(PRS::Acceleration::kInset));
    segment_sb->setSetting(SS::kExtruderSpeed, parent_sb->setting<AngularVelocity>(PS::Inset::kExtruderSpeed));
    segment_sb->setSetting(SS::kMaterialNumber, parent_sb->setting<int>(MS::MultiMaterial::kInsetNum));
    segment_sb->setSetting(SS::kRegionType, RegionType::kInset);
    segment_sb->setSetting(SS::kAdapted, adapted);
}

Distance Inset::beadWidthForSegment(const Point& start, const Point& end,
                                    const QSharedPointer<SettingsBase>& parent_sb) const {
    const Distance fallback_width = parent_sb->setting<Distance>(PS::Inset::kBeadWidth);
    if (!parent_sb->setting<bool>(PS::Inset::kAdaptive)) { return fallback_width; }

    Point midpoint = (start + end) * 0.5;
    const Distance tolerance(std::max(fallback_width() * 1.0e-3, 1.0e-6));
    for (int i = 0; i < m_computed_geometry.size() && i < m_computed_widths.size(); ++i) {
        if (pointOnClosedPolylineXY(midpoint, m_computed_geometry[i], tolerance())) { return m_computed_widths[i]; }
    }
    if (!m_computed_widths.isEmpty()) {
        const Distance first_width = m_computed_widths.first();
        const bool uniform_width   = std::all_of(m_computed_widths.begin(), m_computed_widths.end(),
                                                 [first_width, tolerance](const Distance& width) {
                                                   return std::fabs(width() - first_width()) <= tolerance();
                                                 });
        if (uniform_width) { return first_width; }
    }

    return fallback_width;
}

bool Inset::isAdaptedWidth(const Distance& width, const QSharedPointer<SettingsBase>& parent_sb) {
    const Distance nominal_width = parent_sb->setting<Distance>(PS::Inset::kBeadWidth);
    return std::fabs(width() - nominal_width()) > std::max(nominal_width() * 1.0e-3, 1.0e-6);
}
}  // namespace ORNL
