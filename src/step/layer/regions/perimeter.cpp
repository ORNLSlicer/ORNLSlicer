#include "step/layer/regions/perimeter.h"

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
    if (!line.isEmpty()) {
        line.pop_back();
    }
    return line;
}

bool isValidPerimeterLine(const Polyline& line, Distance min_path_length) {
    return line.size() >= 3 && SpiralPath::closedPolylineLength(line) >= min_path_length;
}

QVector<Polygon> appendValidPathLines(const PolygonList& path_lines, QVector<Polyline>& computed_geometry,
                                      QVector<Distance>& computed_widths, Distance min_path_length, Distance bead_width,
                                      Distance min_segment_length, bool& skipped_path_line) {
    QVector<Polygon> valid_path_lines;

    for (const Polygon& poly : path_lines) {
        Polyline line = toOpenPolyline(poly).removeShortSegments(min_segment_length, true);
        if (!isValidPerimeterLine(line, min_path_length)) {
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
        if (isValidPerimeterLine(toOpenPolyline(poly).removeShortSegments(min_segment_length, true), min_path_length)) {
            valid_path_lines.push_back(poly);
        }
    }

    return valid_path_lines;
}

Distance totalPathLineLength(const QVector<Polygon>& path_lines) {
    Distance total_length;
    for (const Polygon& poly : path_lines) {
        total_length += SpiralPath::closedPolylineLength(toOpenPolyline(poly));
    }
    return total_length;
}

PolygonList pathLineFootprint(const Polygon& path_line, Distance bead_width, const PolygonList& clipping_geometry) {
    PolygonList outer_offset = path_line.offset(bead_width / 2);
    PolygonList inner_offset = path_line.offset(-bead_width / 2);
    PolygonList footprint = outer_offset ^ inner_offset;

    return footprint & clipping_geometry;
}

bool subtractPathLineFootprints(PolygonList& geometry, const QVector<Polygon>& path_lines, Distance bead_width) {
    PolygonList path_line_footprint;
    for (const Polygon& poly : path_lines) {
        path_line_footprint += pathLineFootprint(poly, bead_width, geometry);
    }

    if (path_line_footprint.isEmpty()) {
        return false;
    }

    geometry -= path_line_footprint;
    return true;
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

PolygonList selectedBoundaryPathLines(const PolygonList& external_boundaries, const PolygonList& internal_boundaries,
                                      PerimeterBoundarySelection selection, Distance path_offset) {
    PolygonList offset_geometry =
        selectedBoundaryOffsetGeometry(external_boundaries, internal_boundaries, selection, path_offset);

    return selection == PerimeterBoundarySelection::kInternal ? offset_geometry.internalPolygonBoundaries()
                                                              : offset_geometry.externalPolygonBoundaries();
}

PolygonList selectedBoundaryPathLines(const PolygonList& geometry, PerimeterBoundarySelection selection,
                                      Distance bead_width) {
    if (selection == PerimeterBoundarySelection::kAll) {
        return geometry.offset(-bead_width / 2);
    }

    return selectedBoundaryPathLines(geometry.externalPolygonBoundaries(), geometry.internalPolygonBoundaries(),
                                     selection, bead_width / 2);
}

double netAreaAbs(PolygonList geometry) { return std::fabs(geometry.netArea()()); }

double negligibleAreaTolerance(double reference_area, Distance nominal_width) {
    return std::max(reference_area * 1.0e-6, nominal_width() * nominal_width() * 1.0e-4);
}

bool hasInternalBoundaries(const PolygonList& geometry) { return !geometry.internalPolygonBoundaries().isEmpty(); }

Distance clampedAdaptiveWidth(Distance requested_width, Distance nominal_width, Distance min_width,
                              Distance max_width) {
    double requested = requested_width();
    if (!std::isfinite(requested) || requested <= 0) {
        requested = nominal_width();
    }

    double lower = std::max(0.0, min_width());
    double upper = max_width() > 0 ? max_width() : std::numeric_limits<double>::max();
    if (upper < lower) {
        upper = lower;
    }

    return Distance(std::clamp(requested, lower, upper));
}

bool geometryClearedByFootprints(PolygonList geometry, const QVector<Polygon>& path_lines, Distance bead_width,
                                 double reference_area, Distance nominal_width) {
    if (!subtractPathLineFootprints(geometry, path_lines, bead_width)) {
        return false;
    }

    return geometry.isEmpty() || netAreaAbs(geometry) <= negligibleAreaTolerance(reference_area, nominal_width);
}

bool shellWidthCoversGeometry(PolygonList geometry, const QVector<Polygon>& path_lines, Distance bead_width,
                              Distance nominal_width) {
    const Distance path_length = totalPathLineLength(path_lines);
    if (path_length <= 0) {
        return false;
    }

    const Distance full_area_width = Distance(netAreaAbs(geometry) / path_length());
    return std::fabs(full_area_width() - bead_width()) <= std::max(nominal_width() * 1.0e-4, 1.0e-6);
}

std::optional<Distance> fullCoverageAdaptiveWidth(PolygonList geometry, Distance nominal_width,
                                                  Distance min_path_length, Distance min_segment_length,
                                                  PerimeterBoundarySelection selection, Distance min_width,
                                                  Distance max_width) {
    const double initial_area = netAreaAbs(geometry);
    if (initial_area <= 0) {
        return std::nullopt;
    }
    const bool shell_geometry = selection == PerimeterBoundarySelection::kAll && hasInternalBoundaries(geometry);

    Distance candidate_width = nominal_width;
    QVector<Polygon> candidate_path_lines;
    for (int i = 0; i < 4; ++i) {
        candidate_path_lines = validPathLines(selectedBoundaryPathLines(geometry, selection, candidate_width),
                                              min_path_length, min_segment_length);
        const Distance candidate_length = totalPathLineLength(candidate_path_lines);
        if (candidate_length <= 0) {
            return std::nullopt;
        }

        const Distance next_width =
            clampedAdaptiveWidth(Distance(initial_area / candidate_length()), nominal_width, min_width, max_width);
        if (std::fabs(next_width() - candidate_width()) <= std::max(nominal_width() * 1.0e-4, 1.0e-6)) {
            candidate_width = next_width;
            break;
        }

        candidate_width = next_width;
    }

    candidate_path_lines = validPathLines(selectedBoundaryPathLines(geometry, selection, candidate_width),
                                          min_path_length, min_segment_length);
    if (candidate_path_lines.isEmpty()) {
        return std::nullopt;
    }

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
                                         Distance min_path_length, Distance min_segment_length,
                                         PerimeterBoundarySelection selection, Distance min_width, Distance max_width) {
    if (std::optional<Distance> full_width = fullCoverageAdaptiveWidth(
            geometry, nominal_width, min_path_length, min_segment_length, selection, min_width, max_width)) {
        return *full_width;
    }

    PolygonList preview_geometry = geometry;
    Distance preview_length;

    for (int i = 0; i < remaining_count && !preview_geometry.isEmpty(); ++i) {
        QVector<Polygon> preview_path_lines = validPathLines(
            selectedBoundaryPathLines(preview_geometry, selection, nominal_width), min_path_length, min_segment_length);
        if (preview_path_lines.isEmpty()) {
            break;
        }

        preview_length += totalPathLineLength(preview_path_lines);
        if (!subtractPathLineFootprints(preview_geometry, preview_path_lines, nominal_width)) {
            break;
        }
    }

    if (preview_length <= 0) {
        return nominal_width;
    }

    const bool more_nominal_paths_fit =
        !validPathLines(selectedBoundaryPathLines(preview_geometry, selection, nominal_width), min_path_length,
                        min_segment_length)
             .isEmpty();
    const double initial_area = netAreaAbs(geometry);
    const double preview_remaining_area = netAreaAbs(preview_geometry);
    const double target_area =
        more_nominal_paths_fit ? std::max(0.0, initial_area - preview_remaining_area) : initial_area;

    return clampedAdaptiveWidth(Distance(target_area / preview_length()), nominal_width, min_width, max_width);
}

QVector<Distance> plannedAdaptiveContourWidths(PolygonList geometry, Distance nominal_width, int remaining_count,
                                               Distance min_path_length, Distance min_segment_length,
                                               PerimeterBoundarySelection selection, Distance min_width,
                                               Distance max_width) {
    QVector<Distance> widths;

    for (int i = 0; i < remaining_count && !geometry.isEmpty(); ++i) {
        const Distance path_width =
            adaptiveContourWidthForGeometry(geometry, nominal_width, remaining_count - i, min_path_length,
                                            min_segment_length, selection, min_width, max_width);
        PolygonList path_lines = selectedBoundaryPathLines(geometry, selection, path_width);
        QVector<Polygon> valid_path_lines = validPathLines(path_lines, min_path_length, min_segment_length);
        if (valid_path_lines.isEmpty()) {
            break;
        }

        widths.push_back(path_width);

        const bool clear_shell_geometry =
            selection == PerimeterBoundarySelection::kAll && hasInternalBoundaries(geometry) &&
            shellWidthCoversGeometry(geometry, valid_path_lines, path_width, nominal_width);

        if (!subtractPathLineFootprints(geometry, valid_path_lines, path_width)) {
            break;
        }
        if (clear_shell_geometry) {
            geometry.clear();
        }
    }

    return widths;
}

Distance adaptiveContourWidth(PolygonList geometry, Distance nominal_width, int remaining_count,
                              Distance min_path_length, Distance min_segment_length,
                              PerimeterBoundarySelection selection, Distance min_width, Distance max_width) {
    QVector<Distance> widths = plannedAdaptiveContourWidths(geometry, nominal_width, remaining_count, min_path_length,
                                                            min_segment_length, selection, min_width, max_width);
    if (widths.isEmpty()) {
        return nominal_width;
    }

    return *std::max_element(widths.begin(), widths.end(),
                             [](const Distance& lhs, const Distance& rhs) { return lhs() < rhs(); });
}

double distanceXYToSegment(const Point& point, const Point& start, const Point& end) {
    const double dx = end.x() - start.x();
    const double dy = end.y() - start.y();
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
    if (line.size() < 2) {
        return false;
    }

    for (int i = 0; i < line.size(); ++i) {
        if (distanceXYToSegment(point, line[i], line[(i + 1) % line.size()]) <= tolerance) {
            return true;
        }
    }

    return false;
}

} // namespace

Perimeter::Perimeter(const QSharedPointer<SettingsBase>& sb, const int index,
                     const QVector<SettingsPolygon>& settings_polygons, PolygonList uncut_geometry)
    : RegionBase(sb, index, settings_polygons, uncut_geometry, RegionType::kPerimeter) {}

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
    m_computed_geometry.clear();
    m_computed_widths.clear();

    setMaterialNumber(m_sb->setting<int>(MS::MultiMaterial::kPerimeterNum));
    Distance beadWidth = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
    int perimeter_count = m_sb->setting<int>(PS::Perimeter::kCount);
    const Distance min_path_length = m_sb->setting<Distance>(PS::Perimeter::kMinPathLength);
    const Distance min_segment_length = m_sb->setting<Distance>(PS::Perimeter::kMinSegmentLength);
    const PerimeterBoundarySelection boundary_selection =
        static_cast<PerimeterBoundarySelection>(m_sb->setting<int>(PS::Perimeter::kBoundarySelection));

    const PolygonList original_geometry = m_geometry;
    if (m_sb->setting<bool>(PS::Perimeter::kAdaptive)) {
        const Distance min_adaptive_width = m_sb->setting<Distance>(PS::Perimeter::kAdaptiveMinWidth);
        const Distance max_adaptive_width = m_sb->setting<Distance>(PS::Perimeter::kAdaptiveMaxWidth);

        for (int perimeter_number = 0; perimeter_number < perimeter_count && !m_geometry.isEmpty();
             ++perimeter_number) {
            const int remaining_count = perimeter_count - perimeter_number;
            const Distance path_width =
                adaptiveContourWidth(m_geometry, beadWidth, remaining_count, min_path_length, min_segment_length,
                                     boundary_selection, min_adaptive_width, max_adaptive_width);
            PolygonList path_lines = selectedBoundaryPathLines(m_geometry, boundary_selection, path_width);

            bool skipped_path_line = false;
            QVector<Polygon> valid_path_lines =
                appendValidPathLines(path_lines, m_computed_geometry, m_computed_widths, min_path_length, path_width,
                                     min_segment_length, skipped_path_line);

            if (valid_path_lines.isEmpty()) {
                break;
            }

            const bool clear_shell_geometry =
                boundary_selection == PerimeterBoundarySelection::kAll && hasInternalBoundaries(m_geometry) &&
                shellWidthCoversGeometry(m_geometry, valid_path_lines, path_width, beadWidth);

            if (!subtractPathLineFootprints(m_geometry, valid_path_lines, path_width)) {
                break;
            }
            if (clear_shell_geometry) {
                m_geometry.clear();
            }
        }
        return;
    }

    if (boundary_selection == PerimeterBoundarySelection::kAll) {
        PolygonList path_lines = m_geometry.offset(-beadWidth / 2);
        bool use_footprint_subtraction = false;

        for (int perimeter_number = 0; !path_lines.isEmpty() && perimeter_number < perimeter_count;
             ++perimeter_number) {
            bool skipped_path_line = false;
            QVector<Polygon> valid_path_lines =
                appendValidPathLines(path_lines, m_computed_geometry, m_computed_widths, min_path_length, beadWidth,
                                     min_segment_length, skipped_path_line);

            if (valid_path_lines.isEmpty()) {
                break;
            }

            if (skipped_path_line || use_footprint_subtraction) {
                use_footprint_subtraction = true;
                if (!subtractPathLineFootprints(m_geometry, valid_path_lines, beadWidth)) {
                    break;
                }
                path_lines = selectedBoundaryPathLines(m_geometry, boundary_selection, beadWidth);
            }
            else {
                m_geometry = path_lines.offset(-beadWidth / 2, -beadWidth / 2);
                path_lines = path_lines.offset(-beadWidth, -beadWidth / 2);
            }
        }
        return;
    }

    // Selective modes offset the full part mask and then extract the requested boundary type. Offsetting a hole or an
    // outline as a standalone polygon would send paths into void space or through holes because the opposite boundary
    // would no longer clip the offset.
    const PolygonList external_boundaries = original_geometry.externalPolygonBoundaries();
    const PolygonList internal_boundaries = original_geometry.internalPolygonBoundaries();
    bool use_footprint_subtraction = false;
    PolygonList path_lines;

    for (int perimeter_number = 0; perimeter_number < perimeter_count; ++perimeter_number) {
        if (use_footprint_subtraction) {
            path_lines = selectedBoundaryPathLines(m_geometry, boundary_selection, beadWidth);
        }
        else {
            const Distance path_offset = beadWidth * perimeter_number + beadWidth / 2;
            path_lines =
                selectedBoundaryPathLines(external_boundaries, internal_boundaries, boundary_selection, path_offset);
        }

        if (path_lines.isEmpty()) {
            break;
        }

        bool skipped_path_line = false;
        QVector<Polygon> valid_path_lines =
            appendValidPathLines(path_lines, m_computed_geometry, m_computed_widths, min_path_length, beadWidth,
                                 min_segment_length, skipped_path_line);

        if (valid_path_lines.isEmpty()) {
            break;
        }

        if (skipped_path_line || use_footprint_subtraction) {
            use_footprint_subtraction = true;
            if (!subtractPathLineFootprints(m_geometry, valid_path_lines, beadWidth)) {
                break;
            }
        }
        else {
            const Distance remaining_offset = beadWidth * (perimeter_number + 1);
            m_geometry = selectedBoundaryOffsetGeometry(external_boundaries, internal_boundaries, boundary_selection,
                                                        remaining_offset);
        }
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
            if (newPath.size() == 0) {
                return;
            }

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
            if (!m_sb->setting<bool>(PS::Perimeter::kAdaptive)) {
                Point spiral_query_location = current_location;
                PolylineOrderOptimizer spiral_poo(spiral_query_location, layerNumber);
                configureOptimizer(spiral_poo, pointOrderOptimization);
                spiral_poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kPerimeter, pathOrderOptimization);

                QVector<Polyline> ordered_perimeters;
                const Distance min_path_length = m_sb->setting<Distance>(PS::Perimeter::kMinPathLength);
                const Distance bead_width = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);

                while (spiral_poo.getCurrentPolylineCount() > 0) {
                    if (!ordered_perimeters.isEmpty()) {
                        spiral_poo.setPointParameters(PointOrderOptimization::kNextClosest, false, 0, 0, false, 0,
                                                      false);
                    }

                    Polyline result = spiral_poo.linkNextPolyline();

                    if (result.size() < 3 || SpiralPath::closedPolylineLength(result) < min_path_length) {
                        continue;
                    }

                    spiral_query_location = SpiralPath::transitionStartPoint(result, bead_width);
                    ordered_perimeters.push_back(result);
                }

                if (ordered_perimeters.isEmpty()) {
                    return;
                }

                if (ordered_perimeters.size() == 1) {
                    Polyline result = ordered_perimeters.first();

                    Path newPath = createPath(result);
                    newPath.setCCW(result.orientation());

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

                Polyline result = SpiralPath::linkClosedPolylines(ordered_perimeters, bead_width);

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
                    calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3), true);
                    PathModifierGenerator::GenerateTravel(newPath, current_location,
                                                          m_sb->setting<Velocity>(PS::Travel::kSpeed));

                    current_location = newPath.back()->end();
                    m_paths.push_back(newPath);
                }

                return;
            }

            Point spiral_query_location = current_location;
            PolylineOrderOptimizer spiral_poo(spiral_query_location, layerNumber);
            configureOptimizer(spiral_poo, pointOrderOptimization);
            spiral_poo.setGeometryToEvaluate(m_computed_geometry, RegionType::kPerimeter, pathOrderOptimization);

            QVector<Polyline> ordered_perimeters;
            QVector<Distance> ordered_perimeter_widths;
            bool has_spiral_orientation = false;
            bool spiral_orientation = false;
            const Distance min_path_length = m_sb->setting<Distance>(PS::Perimeter::kMinPathLength);

            while (spiral_poo.getCurrentPolylineCount() > 0) {
                if (!ordered_perimeters.isEmpty()) {
                    spiral_poo.setPointParameters(PointOrderOptimization::kNextClosest, false, 0, 0, false, 0, false);
                }

                Polyline result = spiral_poo.linkNextPolyline();

                if (result.size() < 3 || SpiralPath::closedPolylineLength(result) < min_path_length) {
                    continue;
                }

                if (!has_spiral_orientation) {
                    spiral_orientation = result.orientation();
                    has_spiral_orientation = true;
                }
                else if (result.orientation() != spiral_orientation) {
                    result = result.reverse();
                    result.move(result.size() - 1, 0);
                }

                const Distance result_width = beadWidthForSegment(result.front(), result[1], m_sb);
                ordered_perimeter_widths.push_back(result_width);
                ordered_perimeters.push_back(result);
                spiral_query_location = SpiralPath::transitionStartPoint(result, result_width);
            }

            if (ordered_perimeters.isEmpty()) {
                return;
            }

            if (ordered_perimeters.size() == 1) {
                PolylineOrderOptimizer first_loop_poo(current_location, layerNumber);
                configureOptimizer(first_loop_poo, pointOrderOptimization);
                first_loop_poo.setGeometryToEvaluate({ordered_perimeters.first()}, RegionType::kPerimeter,
                                                     PathOrderOptimization::kNextClosest);
                Polyline result = first_loop_poo.linkNextPolyline();
                if (result.size() < 3) {
                    result = ordered_perimeters.first();
                }

                Path newPath = createPath(result);
                newPath.setCCW(result.orientation());

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

            const Distance nominal_width = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
            Polyline result =
                SpiralPath::linkClosedPolylines(ordered_perimeters, ordered_perimeter_widths, nominal_width);

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
                calculateModifiers(newPath, m_sb->setting<bool>(PRS::MachineSetup::kSupportG3), true);
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
    line = line.removeShortSegments(m_sb->setting<Distance>(PS::Perimeter::kMinSegmentLength), true);
    if (line.size() < 3) {
        return Path();
    }

    // ---------- No Settings Regions ----------
    if (m_settings_polygons.isEmpty()) {
        Path path;

        for (size_t i = 0; i < line.size(); ++i) {
            const Point& start = line[i];
            const Point& end = line[(i + 1) % line.size()];
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

QVector<Polyline> Perimeter::getComputedGeometry() { return m_computed_geometry; }

void Perimeter::calculateModifiers(Path& path, bool supportsG3) { calculateModifiers(path, supportsG3, false); }

void Perimeter::calculateModifiers(Path& path, bool supportsG3, bool open_loop_tip_wipe) {
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
        const TipWipeDirection wipe_direction =
            static_cast<TipWipeDirection>(m_sb->setting<int>(MS::TipWipe::kPerimeterDirection));
        if (wipe_direction == TipWipeDirection::kForward ||
            (!open_loop_tip_wipe && wipe_direction == TipWipeDirection::kOptimal)) {
            if (open_loop_tip_wipe && wipe_direction == TipWipeDirection::kForward) {
                PathModifierGenerator::GenerateForwardTipWipeOpenLoop(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kPerimeterDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kPerimeterSpeed),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kPerimeterExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kPerimeterLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kPerimeterCutoffDistance));
            }
            else {
                PathModifierGenerator::GenerateTipWipe(
                    path, PathModifiers::kForwardTipWipe, m_sb->setting<Distance>(MS::TipWipe::kPerimeterDistance),
                    m_sb->setting<Velocity>(MS::TipWipe::kPerimeterSpeed),
                    m_sb->setting<Angle>(MS::TipWipe::kPerimeterAngle),
                    m_sb->setting<AngularVelocity>(MS::TipWipe::kPerimeterExtruderSpeed),
                    m_sb->setting<Distance>(MS::TipWipe::kPerimeterLiftHeight),
                    m_sb->setting<Distance>(MS::TipWipe::kPerimeterCutoffDistance));
            }
        }
        else if (wipe_direction == TipWipeDirection::kAngled) {
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
            const Distance bead_width = beadWidthForSegment(p0, p1, parent_sb);
            populateSegmentSettings(segment->getSb(), parent_sb, bead_width, isAdaptedWidth(bead_width, parent_sb));
            path.append(segment);
        }
    }
    return path;
}

void Perimeter::populateSegmentSettings(QSharedPointer<SettingsBase> segment_sb,
                                        const QSharedPointer<SettingsBase>& parent_sb, const Distance& bead_width,
                                        bool adapted) {
    // Populate segment settings with the provided settings base
    segment_sb->populate(parent_sb);

    Velocity speed = parent_sb->setting<Velocity>(PS::Perimeter::kSpeed);
    if (adapted && bead_width > 0) {
        const Distance ref_width = parent_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
        const Velocity ref_speed = parent_sb->setting<Velocity>(PS::Perimeter::kSpeed);
        const double min_speed = ref_speed() * 0.01;
        speed = Velocity(std::max((ref_speed() * ref_width()) / bead_width(), min_speed));
    }

    segment_sb->setSetting(SS::kWidth, bead_width);
    segment_sb->setSetting(SS::kHeight, parent_sb->setting<Distance>(PS::Layer::kLayerHeight));
    segment_sb->setSetting(SS::kSpeed, speed);
    segment_sb->setSetting(SS::kAccel, parent_sb->setting<Acceleration>(PRS::Acceleration::kPerimeter));
    segment_sb->setSetting(SS::kExtruderSpeed, parent_sb->setting<AngularVelocity>(PS::Perimeter::kExtruderSpeed));
    segment_sb->setSetting(SS::kMaterialNumber, parent_sb->setting<int>(MS::MultiMaterial::kPerimeterNum));
    segment_sb->setSetting(SS::kRegionType, RegionType::kPerimeter);
    segment_sb->setSetting(SS::kAdapted, adapted);
}

Distance Perimeter::beadWidthForSegment(const Point& start, const Point& end,
                                        const QSharedPointer<SettingsBase>& parent_sb) const {
    const Distance fallback_width = parent_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
    if (!parent_sb->setting<bool>(PS::Perimeter::kAdaptive)) {
        return fallback_width;
    }

    Point midpoint = (start + end) * 0.5;
    const Distance tolerance(std::max(fallback_width() * 1.0e-3, 1.0e-6));
    for (int i = 0; i < m_computed_geometry.size() && i < m_computed_widths.size(); ++i) {
        if (pointOnClosedPolylineXY(midpoint, m_computed_geometry[i], tolerance())) {
            return m_computed_widths[i];
        }
    }
    if (!m_computed_widths.isEmpty()) {
        const Distance first_width = m_computed_widths.first();
        const bool uniform_width = std::all_of(m_computed_widths.begin(), m_computed_widths.end(),
                                               [first_width, tolerance](const Distance& width) {
                                                   return std::fabs(width() - first_width()) <= tolerance();
                                               });
        if (uniform_width) {
            return first_width;
        }
    }

    return fallback_width;
}

bool Perimeter::isAdaptedWidth(const Distance& width, const QSharedPointer<SettingsBase>& parent_sb) {
    const Distance nominal_width = parent_sb->setting<Distance>(PS::Perimeter::kBeadWidth);
    return std::fabs(width() - nominal_width()) > std::max(nominal_width() * 1.0e-3, 1.0e-6);
}
} // namespace ORNL
