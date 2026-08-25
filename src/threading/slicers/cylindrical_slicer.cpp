#include "threading/slicers/cylindrical_slicer.h"

#include <QPair>
#include <QTextStream>
#include <algorithm>
#include <cmath>

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "cross_section/cross_section.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/arc_specialties_writer.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/mesh/open_mesh.h"
#include "geometry/path.h"
#include "geometry/plane.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "slicing/helical_path_rounding.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/cylindrical_layer.h"
#include "threading/traditional_ast.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
//! @brief Segment setting key used by the radial writer to recover the cylinder center X.
const QString kRadialCenterX = "radial_center_x";

//! @brief Segment setting key used by the radial writer to recover the cylinder center Y.
const QString kRadialCenterY = "radial_center_y";

//! @brief Returns a positive setting value or a safe fallback.
Distance positiveOrFallback(Distance value, Distance fallback) {
    return value > 0 ? value : fallback;
}

//! @brief Physical fallback used when the cylindrical layer spacing setting is invalid.
const Distance kDefaultCylindricalLayerHeight = 1.0 * mm;

//! @brief Smallest circle approximation segment used to avoid excessive candidate points.
const Distance kMinCircleSegmentLength = 100.0 * micron;

//! @brief Smallest Z spacing used for model cross sections.
const Distance kMinSectionSpacing = 100.0 * micron;

//! @brief Minimum path segment length kept after clipping candidate helices.
const Distance kMinPathSegmentLength = 10.0 * micron;

//! @brief Returns the total length of all printable polyline fragments.
double totalPolylineLength(const QVector<Polyline>& polylines) {
    double total_length = 0.0;
    for (const Polyline& polyline : polylines) {
        if (polyline.size() > 1) { total_length += polyline.length()(); }
    }
    return total_length;
}

//! @brief Clips a candidate radial ring against the model cross section.
QVector<Polyline> clipCircleToSection(PolygonList& geometry, const Polyline& circle) {
    return geometry & circle;
}

//! @brief Returns whether model clipping removed any meaningful portion of the radial path.
bool crossesModelBoundary(const Polyline& circle, const QVector<Polyline>& clipped_lines) {
    if (clipped_lines.isEmpty() || circle.size() < 2) { return false; }

    const double circle_length  = circle.length()();
    const double clipped_length = totalPolylineLength(clipped_lines);
    return clipped_length < circle_length - kMinPathSegmentLength();
}

//! @brief Applies the configured radial boundary policy to a candidate radial path.
QVector<Polyline> applyBoundaryPolicy(const Polyline& circle, const QVector<Polyline>& clipped_lines,
                                      RadialPathBoundaryPolicy handling) {
    if (clipped_lines.isEmpty()) { return {}; }

    switch (handling) {
        case RadialPathBoundaryPolicy::kKeepBoundaryCrossingPath:
            return {circle};
        case RadialPathBoundaryPolicy::kDiscardBoundaryCrossingPath:
            return crossesModelBoundary(circle, clipped_lines) ? QVector<Polyline>() : clipped_lines;
        case RadialPathBoundaryPolicy::kClipToModel:
        default:
            return clipped_lines;
    }
}

//! @brief Cached horizontal section of the model reused by every radial layer at the same Z.
struct RadialCrossSection {
    Distance z;
    PolygonList geometry;
};

//! @brief Cached horizontal section of the model used to clip helical points.
struct HelicalCrossSection {
    Distance z;
    PolygonList geometry;
};

//! @brief Model-clipping result for one sampled helix.
struct HelixClipResult {
    QVector<Polyline> fragments;
    QVector<HelicalPathBoundaryIntersection> intersections;
    bool has_inside_points  = false;
    bool has_outside_points = false;
};

//! @brief Calculates combined bounds for non-empty meshes.
bool meshBounds(const QVector<QSharedPointer<MeshBase>>& meshes, Point& mesh_min, Point& mesh_max) {
    bool has_bounds = false;
    for (const QSharedPointer<MeshBase>& mesh : meshes) {
        if (mesh == nullptr || mesh->vertices().isEmpty()) { continue; }

        if (!has_bounds) {
            mesh_min   = mesh->min();
            mesh_max   = mesh->max();
            has_bounds = true;
            continue;
        }

        const Point current_min = mesh->min();
        const Point current_max = mesh->max();
        mesh_min.x(std::min(mesh_min.x(), current_min.x()));
        mesh_min.y(std::min(mesh_min.y(), current_min.y()));
        mesh_min.z(std::min(mesh_min.z(), current_min.z()));
        mesh_max.x(std::max(mesh_max.x(), current_max.x()));
        mesh_max.y(std::max(mesh_max.y(), current_max.y()));
        mesh_max.z(std::max(mesh_max.z(), current_max.z()));
    }

    return has_bounds;
}

//! @brief Linearly interpolates between two points.
Point interpolate(const Point& start, const Point& end, double t) {
    return Point(start.x() + (end.x() - start.x()) * t, start.y() + (end.y() - start.y()) * t,
                 start.z() + (end.z() - start.z()) * t);
}

//! @brief Returns the nearest cached cross section index for a point Z.
int nearestSectionIndex(const QVector<HelicalCrossSection>& sections, const Point& point, Distance first_z,
                        Distance section_spacing) {
    if (sections.isEmpty()) { return -1; }

    const double raw_index = (point.z() - first_z()) / section_spacing();
    return std::clamp(static_cast<int>(std::llround(raw_index)), 0, static_cast<int>(sections.size()) - 1);
}

//! @brief Checks whether a 3D point is inside the nearest horizontal model section.
bool pointInsideModel(const QVector<HelicalCrossSection>& sections, const Point& point, Distance first_z,
                      Distance section_spacing) {
    const int index = nearestSectionIndex(sections, point, first_z, section_spacing);
    if (index < 0 || sections[index].geometry.isEmpty()) { return false; }

    return sections[index].geometry.inside(point, true);
}

//! @brief Finds an approximate model-boundary point along a sampled helix segment.
Point findBoundaryPoint(const Point& start, const Point& end, bool start_inside,
                        const QVector<HelicalCrossSection>& sections, Distance first_z, Distance section_spacing) {
    Point low       = start;
    Point high      = end;
    bool low_inside = start_inside;

    for (int i = 0; i < 12; ++i) {
        const Point mid       = interpolate(low, high, 0.5);
        const bool mid_inside = pointInsideModel(sections, mid, first_z, section_spacing);
        if (mid_inside == low_inside) {
            low        = mid;
            low_inside = mid_inside;
        }
        else { high = mid; }
    }

    return start_inside ? low : high;
}

//! @brief Clips a sampled helix into contiguous fragments and records model-boundary crossings.
HelixClipResult clipHelixToSections(const Polyline& helix, const QVector<HelicalCrossSection>& sections,
                                    Distance first_z, Distance section_spacing) {
    HelixClipResult result;
    if (helix.size() < 2 || sections.isEmpty()) { return result; }

    Point previous            = helix.first();
    bool previous_inside      = pointInsideModel(sections, previous, first_z, section_spacing);
    result.has_inside_points  = previous_inside;
    result.has_outside_points = !previous_inside;

    Polyline current_line;
    if (previous_inside) { current_line.push_back(previous); }

    for (int i = 1, end = helix.size(); i < end; ++i) {
        const Point current       = helix[i];
        const bool current_inside = pointInsideModel(sections, current, first_z, section_spacing);
        result.has_inside_points  = result.has_inside_points || current_inside;
        result.has_outside_points = result.has_outside_points || !current_inside;

        if (previous_inside && current_inside) {
            if (current_line.isEmpty()) { current_line.push_back(previous); }
            current_line.push_back(current);
        }
        else if (previous_inside && !current_inside) {
            const Point boundary_point = findBoundaryPoint(previous, current, true, sections, first_z, section_spacing);
            result.intersections.push_back(HelicalPathBoundaryIntersection {boundary_point, i});
            current_line.push_back(boundary_point);
            if (current_line.size() > 1 && current_line.length() > kMinPathSegmentLength) {
                result.fragments.push_back(current_line);
            }
            current_line.clear();
        }
        else if (!previous_inside && current_inside) {
            const Point boundary_point =
                findBoundaryPoint(previous, current, false, sections, first_z, section_spacing);
            result.intersections.push_back(HelicalPathBoundaryIntersection {boundary_point, i});
            current_line.clear();
            current_line.push_back(boundary_point);
            current_line.push_back(current);
        }

        previous        = current;
        previous_inside = current_inside;
    }

    if (current_line.size() > 1 && current_line.length() > kMinPathSegmentLength) {
        result.fragments.push_back(current_line);
    }

    return result;
}

//! @brief Splits a sampled helical fragment into path-length-limited fragments.
QVector<Polyline> splitPolylineByLength(const Polyline& polyline, Distance max_path_length) {
    if (polyline.size() < 2) { return {}; }

    if (max_path_length <= kMinPathSegmentLength || polyline.length() <= max_path_length) { return {polyline}; }

    QVector<Polyline> split_lines;
    Polyline current_line;
    current_line.push_back(polyline.first());
    Distance current_length = 0;

    for (int i = 1, end = polyline.size(); i < end; ++i) {
        Point segment_start               = polyline[i - 1];
        const Point segment_end           = polyline[i];
        Distance remaining_segment_length = segment_start.distance(segment_end);

        while (remaining_segment_length > 0) {
            Distance remaining_path_length = max_path_length - current_length;
            if (remaining_path_length <= kMinPathSegmentLength && current_line.size() > 1) {
                split_lines.push_back(current_line);
                current_line.clear();
                current_line.push_back(segment_start);
                current_length        = 0;
                remaining_path_length = max_path_length;
            }

            if (remaining_segment_length <= remaining_path_length) {
                if (current_line.last() != segment_end) { current_line.push_back(segment_end); }
                current_length += remaining_segment_length;
                remaining_segment_length = 0;
            }
            else {
                const double split_fraction = remaining_path_length() / remaining_segment_length();
                const Point split_point     = interpolate(segment_start, segment_end, split_fraction);
                if (current_line.last() != split_point) { current_line.push_back(split_point); }
                if (current_line.size() > 1) { split_lines.push_back(current_line); }

                current_line.clear();
                current_line.push_back(split_point);
                segment_start            = split_point;
                current_length           = 0;
                remaining_segment_length = segment_start.distance(segment_end);
            }
        }
    }

    if (current_line.size() > 1) { split_lines.push_back(current_line); }

    return split_lines;
}

//! @brief Estimates progress loop iterations for an inclusive Distance range.
int estimateInclusiveCount(Distance start, Distance end, Distance step) {
    if (step <= 0 || start > end) { return 1; }

    return std::max(1, static_cast<int>(std::floor((end() - start()) / step())) + 1);
}

//! @brief Returns the upper Z limit for generated cylindrical candidates.
Distance cylindricalTopZ(const QSharedPointer<SettingsBase>& part_sb, Distance base_z, Distance mesh_top_z) {
    const Distance cylinder_height = part_sb->setting<Distance>(PS::Slicing::kCylinderHeight);
    if (cylinder_height <= 0) { return mesh_top_z; }

    const Distance capped_top_z(base_z + cylinder_height);
    return capped_top_z < mesh_top_z ? capped_top_z : mesh_top_z;
}
}  // namespace

CylindricalSlicer::CylindricalSlicer(QString gcodeLocation) : TraditionalAST(gcodeLocation) {
    m_syntax = GcodeSyntax::kArcSpecialties;
    m_base   = QSharedPointer<ArcSpecialtiesWriter>::create(GcodeMetaList::ArcSpecialtiesMeta, GSM->getGlobal());
}

void CylindricalSlicer::doSlice() {
    if (CSM->parts().empty()) {
        qWarning() << "Attempted to start a slice when no data has been loaded.";
        return;
    }

    m_timer.start();

    this->setMaxSteps(0);
    this->preProcess();

    if (this->shouldCancel()) { return; }

    this->postProcess();
    m_elapsed_time = m_timer.elapsed();

    if (this->shouldCancel()) { return; }

    if (!m_skip_gcode) {
        this->writeGCodeSetup();
        if (m_has_generated_path_max_z) {
            Distance build_maximum_z = m_generated_path_max_z;
            if (m_base->hasBuildMaximumZ() && m_base->getBuildMaximumZ() > build_maximum_z) {
                build_maximum_z = m_base->getBuildMaximumZ();
            }
            m_base->setBuildMaximumZ(build_maximum_z);
        }
        this->writeGCode();
        this->writeGCodeShutdown();
    }

    if (this->shouldCancel()) { return; }

    emit sliceComplete();
}

void CylindricalSlicer::preProcess(nlohmann::json opt_data) {
    m_cylindrical_layers.clear();
    m_has_generated_path_max_z = false;
    m_generated_path_max_z     = 0;
    this->setMaxSteps(0);

    QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
    global_sb->makeGlobalAdjustments();
    const CylindricalPathPattern selected_path_pattern =
        static_cast<CylindricalPathPattern>(global_sb->setting<int>(PS::Slicing::kCylindricalPathPattern));

    QVector<QSharedPointer<Part>> build_parts = SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kBuild);
    QVector<QSharedPointer<MeshBase>> clipping_meshes =
        SlicingUtilities::GetMeshesByType(CSM->parts(), MeshType::kClipping);
    QVector<QPair<QString, HelicalPathBoundaryPolicy>> effective_boundary_policy;
    QVector<QPair<QString, HelicalPathZClipRounding>> effective_z_clip_rounding;
    QVector<QPair<QString, HelicalPathHandedness>> effective_handedness;

    int last_preprocess_percent = -1;
    int last_compute_percent    = -1;
    auto emitPartProgress       = [this, &build_parts](StatusUpdateStepType type, int part_index, double part_fraction,
                                                       int& last_percent) {
        const double total_parts           = std::max(1, static_cast<int>(build_parts.size()));
        const double bounded_part_fraction = std::clamp(part_fraction, 0.0, 1.0);
        const int percent =
            std::clamp(static_cast<int>(((part_index + bounded_part_fraction) / total_parts) * 100.0), 0, 100);

        if (percent > last_percent) {
            emit statusUpdate(type, percent);
            last_percent = percent;
        }
    };
    auto emitPreProcessProgress = [&emitPartProgress, &last_preprocess_percent](int part_index, double part_fraction) {
        emitPartProgress(StatusUpdateStepType::kPreProcess, part_index, part_fraction, last_preprocess_percent);
    };
    auto emitComputeProgress = [&emitPartProgress, &last_compute_percent](int part_index, double part_fraction) {
        emitPartProgress(StatusUpdateStepType::kCompute, part_index, part_fraction, last_compute_percent);
    };

    int parts_processed = 0;
    emitPreProcessProgress(parts_processed, 0.0);
    for (const QSharedPointer<Part>& part : build_parts) {
        QSharedPointer<SettingsBase> part_sb = QSharedPointer<SettingsBase>::create(*global_sb);
        part_sb->populate(part->getSb());
        part->clearSteps();

        QVector<QSharedPointer<MeshBase>> meshes;
        for (const QSharedPointer<MeshBase>& original_mesh : part->meshes()) {
            QSharedPointer<MeshBase> mesh = copyMesh(original_mesh);
            if (mesh != nullptr) {
                SlicingUtilities::ClipMesh(mesh, clipping_meshes);
                meshes.push_back(mesh);
            }
        }

        if (meshes.isEmpty()) {
            ++parts_processed;
            emitPreProcessProgress(parts_processed, 0.0);
            emitComputeProgress(parts_processed, 0.0);
            continue;
        }

        Point mesh_min;
        Point mesh_max;
        if (!meshBounds(meshes, mesh_min, mesh_max)) {
            ++parts_processed;
            emitPreProcessProgress(parts_processed, 0.0);
            emitComputeProgress(parts_processed, 0.0);
            continue;
        }

        const CylindricalPathPattern path_pattern =
            static_cast<CylindricalPathPattern>(part_sb->setting<int>(PS::Slicing::kCylindricalPathPattern));
        bool part_generated_paths = false;

        if (path_pattern == CylindricalPathPattern::kHelical) {
            const HelicalPathBoundaryPolicy boundary_policy =
                static_cast<HelicalPathBoundaryPolicy>(part_sb->setting<int>(PS::Slicing::kHelicalPathBoundaryPolicy));
            const HelicalPathZClipRounding z_clip_rounding =
                static_cast<HelicalPathZClipRounding>(part_sb->setting<int>(PS::Slicing::kHelicalPathZClipRounding));
            const HelicalPathHandedness handedness =
                static_cast<HelicalPathHandedness>(part_sb->setting<int>(PS::Slicing::kHelicalPathHandedness));

            part_generated_paths =
                generateHelicalLayers(part, part_sb, meshes, mesh_min, mesh_max, boundary_policy, z_clip_rounding,
                                      handedness, parts_processed, emitPreProcessProgress, emitComputeProgress);
            if (part_generated_paths) {
                effective_boundary_policy.push_back(
                    QPair<QString, HelicalPathBoundaryPolicy> {part->name(), boundary_policy});
                if (boundary_policy == HelicalPathBoundaryPolicy::kClipZ) {
                    effective_z_clip_rounding.push_back(
                        QPair<QString, HelicalPathZClipRounding> {part->name(), z_clip_rounding});
                }
                effective_handedness.push_back(QPair<QString, HelicalPathHandedness> {part->name(), handedness});
            }
        }
        else {
            const RadialPathBoundaryPolicy boundary_policy =
                static_cast<RadialPathBoundaryPolicy>(part_sb->setting<int>(PS::Slicing::kRadialPathBoundaryPolicy));
            part_generated_paths = generateRadialLayers(part, part_sb, meshes, mesh_min, mesh_max, boundary_policy,
                                                        parts_processed, emitPreProcessProgress, emitComputeProgress);
        }

        ++parts_processed;
        emitPreProcessProgress(parts_processed, 0.0);
        emitComputeProgress(parts_processed, 0.0);
    }

    QSharedPointer<ArcSpecialtiesWriter> arc_specialties_writer = m_base.dynamicCast<ArcSpecialtiesWriter>();
    if (!arc_specialties_writer.isNull()) {
        arc_specialties_writer->setHelicalPathBoundaryPolicy(effective_boundary_policy);
        arc_specialties_writer->setHelicalPathZClipRounding(effective_z_clip_rounding);
        arc_specialties_writer->setHelicalPathHandedness(effective_handedness);
    }

    if (m_cylindrical_layers.isEmpty()) {
        const QString boundary_setting = selected_path_pattern == CylindricalPathPattern::kHelical
                                             ? "Helical Path Boundary Policy"
                                             : "Radial Path Boundary Policy";
        const QString message          = QString(
                                             "Warning: %1 slicing generated no printable paths. Check Cylinder "
                                             "Inner Radius, Cylinder Axis Source, clipping meshes, and %2.")
                                             .arg(toString(selected_path_pattern), boundary_setting);
        qWarning() << message;
        emit statusMessage(message);
    }

    emitPreProcessProgress(static_cast<int>(build_parts.size()), 0.0);
    emitComputeProgress(static_cast<int>(build_parts.size()), 0.0);
}

bool CylindricalSlicer::generateRadialLayers(const QSharedPointer<Part>& part,
                                             const QSharedPointer<SettingsBase>& part_sb,
                                             const QVector<QSharedPointer<MeshBase>>& meshes, const Point& mesh_min,
                                             const Point& mesh_max, RadialPathBoundaryPolicy boundary_policy,
                                             int part_index, const ProgressCallback& emit_pre_process_progress,
                                             const ProgressCallback& emit_compute_progress) {
    const Distance layer_height =
        positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kLayerHeight), kDefaultCylindricalLayerHeight);
    const Distance bead_width = positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kBeadWidth), layer_height);
    Distance initial_radius   = part_sb->setting<Distance>(PS::Slicing::kCylinderInnerRadius);
    if (initial_radius < 0) { initial_radius = 0.0 * micron; }

    const Distance base_z(mesh_min.z());
    const Distance top_z = cylindricalTopZ(part_sb, base_z, mesh_max.z());
    Point center         = cylinderCenterForPart(part_sb, part, base_z);
    const Distance max_radius(maxRadiusForMeshes(meshes, center));

    // Match planar slicing's centerline convention: the first layer sits half a
    // step inside the printable band, then subsequent layers advance by the
    // full configured spacing.
    const Distance first_radius = initial_radius + (layer_height / 2.0);
    const Distance first_bead_z = base_z + (bead_width / 2.0);
    Point current_location(center.x(), center.y(), first_bead_z());

    QVector<RadialCrossSection> cross_sections;
    const int estimated_section_count = estimateInclusiveCount(first_bead_z, top_z, bead_width);
    int sections_processed            = 0;
    for (Distance z = first_bead_z; z <= top_z; z += bead_width) {
        Plane slicing_plane(Point(center.x(), center.y(), z()), QVector3D(0, 0, 1));
        PolygonList combined_geometry;

        for (const QSharedPointer<MeshBase>& mesh : meshes) {
            Point shift;
            QVector3D average_normal;
            PolygonList geometry = CrossSection::doCrossSection(mesh, slicing_plane, shift, average_normal, part_sb);
            if (geometry.isEmpty()) { continue; }

            if (combined_geometry.isEmpty()) { combined_geometry = geometry; }
            else { combined_geometry += geometry; }
        }

        if (!combined_geometry.isEmpty()) { cross_sections.push_back(RadialCrossSection {z, combined_geometry}); }

        ++sections_processed;
        emit_pre_process_progress(
            part_index, static_cast<double>(sections_processed) / static_cast<double>(estimated_section_count));
    }
    emit_pre_process_progress(part_index, 1.0);

    if (cross_sections.isEmpty()) {
        emit_compute_progress(part_index, 0.0);
        return false;
    }

    bool part_generated_paths = false;
    emit_compute_progress(part_index, 0.0);
    int radial_layer_number          = 0;
    const int estimated_radius_count = estimateInclusiveCount(first_radius, max_radius, layer_height);
    int radii_processed              = 0;
    for (Distance radius = first_radius; radius <= max_radius; radius += layer_height) {
        QSharedPointer<SettingsBase> layer_settings = QSharedPointer<SettingsBase>::create(*part_sb);
        layer_settings->makeLocalAdjustments(radial_layer_number);

        QSharedPointer<CylindricalLayer> radial_layer = QSharedPointer<CylindricalLayer>::create(
            radial_layer_number + 1, layer_settings, CylindricalPathPattern::kRadial);

        for (RadialCrossSection& section : cross_sections) {
            Polyline circle = createCircle(center, radius, section.z, bead_width,
                                           layer_settings->setting<Angle>(PS::Slicing::kRadialPathStartAngle));

            QVector<Polyline> clipped_lines   = clipCircleToSection(section.geometry, circle);
            QVector<Polyline> candidate_lines = applyBoundaryPolicy(circle, clipped_lines, boundary_policy);

            for (Polyline line : candidate_lines) {
                if (line.size() < 2) { continue; }

                for (Point& point : line) { point.z(section.z()); }

                Path path = createPath(line, layer_settings, center, radius, true, current_location);
                if (path.size() > 0) { radial_layer->addPath(path); }
            }
        }

        if (radial_layer->hasPaths()) {
            part->appendStep(radial_layer);
            m_cylindrical_layers.push_back(radial_layer);
            this->setMaxSteps(m_cylindrical_layers.size());
            part_generated_paths = true;
        }

        ++radial_layer_number;
        ++radii_processed;
        emit_compute_progress(part_index,
                              static_cast<double>(radii_processed) / static_cast<double>(estimated_radius_count));
    }

    return part_generated_paths;
}

bool CylindricalSlicer::generateHelicalLayers(const QSharedPointer<Part>& part,
                                              const QSharedPointer<SettingsBase>& part_sb,
                                              const QVector<QSharedPointer<MeshBase>>& meshes, const Point& mesh_min,
                                              const Point& mesh_max, HelicalPathBoundaryPolicy boundary_policy,
                                              HelicalPathZClipRounding z_clip_rounding,
                                              HelicalPathHandedness handedness, int part_index,
                                              const ProgressCallback& emit_pre_process_progress,
                                              const ProgressCallback& emit_compute_progress) {
    const Distance layer_height =
        positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kLayerHeight), kDefaultCylindricalLayerHeight);
    const Distance bead_width = positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kBeadWidth), layer_height);
    const Distance section_spacing    = bead_width / 2.0 > kMinSectionSpacing ? bead_width / 2.0 : kMinSectionSpacing;
    const bool helix_counterclockwise = handedness == HelicalPathHandedness::kRightHanded;
    Distance initial_radius           = part_sb->setting<Distance>(PS::Slicing::kCylinderInnerRadius);
    if (initial_radius < 0) { initial_radius = 0.0 * micron; }

    const Distance base_z(mesh_min.z());
    const Distance top_z = cylindricalTopZ(part_sb, base_z, mesh_max.z());
    Point center         = cylinderCenterForPart(part_sb, part, base_z);
    const Distance max_radius(maxRadiusForMeshes(meshes, center));

    const Distance first_radius = initial_radius + (layer_height / 2.0);
    const Distance first_bead_z = base_z + (bead_width / 2.0);
    if (first_bead_z >= top_z) {
        emit_pre_process_progress(part_index, 0.0);
        emit_compute_progress(part_index, 0.0);
        return false;
    }

    QVector<HelicalCrossSection> cross_sections;
    bool has_geometry                 = false;
    const int estimated_section_count = estimateInclusiveCount(first_bead_z, top_z, section_spacing);
    int sections_processed            = 0;
    for (Distance z = first_bead_z; z <= top_z; z += section_spacing) {
        Plane slicing_plane(Point(center.x(), center.y(), z()), QVector3D(0, 0, 1));
        PolygonList combined_geometry;

        for (const QSharedPointer<MeshBase>& mesh : meshes) {
            Point shift;
            QVector3D average_normal;
            PolygonList geometry = CrossSection::doCrossSection(mesh, slicing_plane, shift, average_normal, part_sb);
            if (geometry.isEmpty()) { continue; }

            if (combined_geometry.isEmpty()) { combined_geometry = geometry; }
            else { combined_geometry += geometry; }
        }

        has_geometry = has_geometry || !combined_geometry.isEmpty();
        cross_sections.push_back(HelicalCrossSection {z, combined_geometry});
        ++sections_processed;
        emit_pre_process_progress(
            part_index, static_cast<double>(sections_processed) / static_cast<double>(estimated_section_count));
    }

    if (cross_sections.isEmpty() || cross_sections.last().z < top_z) {
        Plane slicing_plane(Point(center.x(), center.y(), top_z()), QVector3D(0, 0, 1));
        PolygonList combined_geometry;
        for (const QSharedPointer<MeshBase>& mesh : meshes) {
            Point shift;
            QVector3D average_normal;
            PolygonList geometry = CrossSection::doCrossSection(mesh, slicing_plane, shift, average_normal, part_sb);
            if (geometry.isEmpty()) { continue; }

            if (combined_geometry.isEmpty()) { combined_geometry = geometry; }
            else { combined_geometry += geometry; }
        }

        has_geometry = has_geometry || !combined_geometry.isEmpty();
        cross_sections.push_back(HelicalCrossSection {top_z, combined_geometry});
    }
    emit_pre_process_progress(part_index, 1.0);

    if (!has_geometry) {
        emit_compute_progress(part_index, 0.0);
        return false;
    }

    bool part_generated_paths = false;
    emit_compute_progress(part_index, 0.0);
    Point current_location(center.x(), center.y(), first_bead_z());
    int helical_layer_number         = 0;
    const int estimated_radius_count = estimateInclusiveCount(first_radius, max_radius, layer_height);
    int radii_processed              = 0;
    for (Distance radius = first_radius; radius <= max_radius; radius += layer_height) {
        QSharedPointer<SettingsBase> layer_settings = QSharedPointer<SettingsBase>::create(*part_sb);
        layer_settings->makeLocalAdjustments(helical_layer_number);

        QSharedPointer<CylindricalLayer> helical_layer = QSharedPointer<CylindricalLayer>::create(
            helical_layer_number + 1, layer_settings, CylindricalPathPattern::kHelical);

        const Angle helical_start_angle = layer_settings->setting<Angle>(PS::Slicing::kHelicalPathStartAngle);
        Polyline helix = createHelix(center, radius, first_bead_z, top_z, bead_width, handedness, helical_start_angle);
        const HelixClipResult clip_result = clipHelixToSections(helix, cross_sections, first_bead_z, section_spacing);
        QVector<Polyline> clipped_lines   = clip_result.fragments;
        if (boundary_policy == HelicalPathBoundaryPolicy::kClipZ) {
            clipped_lines = HelicalPathRounding::clipAtHighestIntersection(
                helix, clip_result.intersections, clip_result.has_inside_points, clip_result.has_outside_points, center,
                radius, first_bead_z, bead_width, handedness, helical_start_angle, z_clip_rounding,
                kMinPathSegmentLength);
        }

        const Distance max_path_length = layer_settings->setting<Distance>(PS::Slicing::kMaxHelicalPathLength);
        for (const Polyline& line : clipped_lines) {
            if (line.size() < 2) { continue; }

            const QVector<Polyline> path_lines = splitPolylineByLength(line, max_path_length);
            for (const Polyline& path_line : path_lines) {
                Path path =
                    createPath(path_line, layer_settings, center, radius, helix_counterclockwise, current_location);

                if (path.size() > 0) {
                    helical_layer->addPath(path);
                    for (const Point& point : path_line) {
                        const Distance point_z(point.z());
                        if (!m_has_generated_path_max_z || point_z > m_generated_path_max_z) {
                            m_generated_path_max_z     = point_z;
                            m_has_generated_path_max_z = true;
                        }
                    }
                }
            }
        }

        if (helical_layer->hasPaths()) {
            part->appendStep(helical_layer);
            m_cylindrical_layers.push_back(helical_layer);
            this->setMaxSteps(m_cylindrical_layers.size());
            part_generated_paths = true;
        }

        ++helical_layer_number;
        ++radii_processed;
        emit_compute_progress(part_index,
                              static_cast<double>(radii_processed) / static_cast<double>(estimated_radius_count));
    }

    return part_generated_paths;
}

void CylindricalSlicer::postProcess(nlohmann::json opt_data) {
    if (m_cylindrical_layers.isEmpty()) {
        emit statusUpdate(StatusUpdateStepType::kPostProcess, 100);
        return;
    }

    Point current_location = m_cylindrical_layers.first()->getStartLocation();
    for (int layer_index = 0, layer_count = m_cylindrical_layers.size(); layer_index < layer_count; ++layer_index) {
        m_cylindrical_layers[layer_index]->calculateModifiers(current_location);
        emit statusUpdate(StatusUpdateStepType::kPostProcess,
                          (static_cast<double>(layer_index + 1) / static_cast<double>(layer_count)) * 100.0);
    }
}

void CylindricalSlicer::writeGCode() {
    QTextStream stream(&m_temp_gcode_output_file);

    const double num_layers = std::max(1.0, static_cast<double>(m_cylindrical_layers.size()));
    int layer_number        = 0;
    for (const QSharedPointer<CylindricalLayer>& layer : m_cylindrical_layers) {
        stream << m_base->writeLayerChange(layer_number);
        stream << m_base->writeBeforeLayer(layer->getMinZ(), layer->getSb());
        stream << layer->writeGCode(m_base);
        layer->setDirtyBit(false);
        stream << m_base->writeAfterLayer();

        emit statusUpdate(StatusUpdateStepType::kGcodeGeneraton, (layer_number + 1) / num_layers * 100);
        ++layer_number;
    }

    stream << m_base->writeAfterPart();
}

QSharedPointer<MeshBase> CylindricalSlicer::copyMesh(const QSharedPointer<MeshBase>& mesh) {
    if (mesh == nullptr) { return nullptr; }

    if (ClosedMesh* closed_mesh = dynamic_cast<ClosedMesh*>(mesh.get())) {
        return QSharedPointer<ClosedMesh>::create(*closed_mesh);
    }

    if (OpenMesh* open_mesh = dynamic_cast<OpenMesh*>(mesh.get())) {
        return QSharedPointer<OpenMesh>::create(*open_mesh);
    }

    return nullptr;
}

Point CylindricalSlicer::cylinderCenterForPart(const QSharedPointer<SettingsBase>& part_sb,
                                               const QSharedPointer<Part>& part, Distance base_z) {
    const CylinderAxisSource axis_mode =
        static_cast<CylinderAxisSource>(part_sb->setting<int>(PS::Slicing::kCylinderAxisSource));

    Point center = part->rootMesh()->centroid();
    if (axis_mode == CylinderAxisSource::kCustomXY) {
        center.x(part_sb->setting<Distance>(PS::Slicing::kCylinderAxisX));
        center.y(part_sb->setting<Distance>(PS::Slicing::kCylinderAxisY));
    }

    center.z(base_z);
    return center;
}

double CylindricalSlicer::maxRadiusForMeshes(const QVector<QSharedPointer<MeshBase>>& meshes, const Point& center) {
    double max_radius = 0.0;
    for (const QSharedPointer<MeshBase>& mesh : meshes) {
        if (mesh == nullptr) { continue; }

        for (const MeshVertex& vertex : mesh->vertices()) {
            const double dx = static_cast<double>(vertex.location.x() - center.x());
            const double dy = static_cast<double>(vertex.location.y() - center.y());
            max_radius      = std::max(max_radius, std::hypot(dx, dy));
        }
    }
    return max_radius;
}

Polyline CylindricalSlicer::createHelix(const Point& center, Distance radius, Distance start_z, Distance top_z,
                                        Distance bead_width, HelicalPathHandedness handedness, Angle start_angle) {
    if (top_z <= start_z || bead_width <= 0) { return {}; }

    return HelicalPathRounding::createHelixForRevolutions(center, radius, start_z, bead_width, handedness, start_angle,
                                                          (top_z() - start_z()) / bead_width());
}

Polyline CylindricalSlicer::createCircle(const Point& center, Distance radius, Distance z, Distance bead_width,
                                         Angle start_angle) {
    const double circumference = 2.0 * M_PI * radius();
    const Distance target_segment_length =
        bead_width / 2.0 > kMinCircleSegmentLength ? bead_width / 2.0 : kMinCircleSegmentLength;
    const int segments = std::clamp(static_cast<int>(std::ceil(circumference / target_segment_length())), 64, 720);

    Polyline circle;
    circle.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        const double theta = start_angle() + (2.0 * M_PI * static_cast<double>(i) / static_cast<double>(segments));
        circle.push_back(Point(center.x() + radius() * std::cos(theta), center.y() + radius() * std::sin(theta), z()));
    }

    return circle;
}

QSharedPointer<SettingsBase> CylindricalSlicer::createSegmentSettings(
    const QSharedPointer<SettingsBase>& layer_settings, const Point& center, bool region_start) {
    QSharedPointer<SettingsBase> segment_settings = QSharedPointer<SettingsBase>::create(*layer_settings);
    segment_settings->setSetting(SS::kWidth, layer_settings->setting<Distance>(PS::Layer::kBeadWidth));
    segment_settings->setSetting(SS::kHeight, layer_settings->setting<Distance>(PS::Layer::kLayerHeight));
    segment_settings->setSetting(SS::kSpeed, layer_settings->setting<Velocity>(PS::Layer::kSpeed));
    segment_settings->setSetting(SS::kRegionType, RegionType::kPerimeter);
    segment_settings->setSetting(SS::kPathModifiers, PathModifiers::kNone);
    segment_settings->setSetting(SS::kMaterialNumber, 0);
    segment_settings->setSetting(SS::kRecipe, 0);
    segment_settings->setSetting(SS::kIsRegionStartSegment, region_start);

    segment_settings->setSetting(kRadialCenterX, Distance(center.x()));
    segment_settings->setSetting(kRadialCenterY, Distance(center.y()));
    return segment_settings;
}

Path CylindricalSlicer::createPath(const Polyline& polyline, const QSharedPointer<SettingsBase>& layer_settings,
                                   const Point& center, Distance radius, bool counterclockwise,
                                   Point& current_location) {
    Path path;
    if (polyline.size() < 2) { return path; }

    const bool write_arcs           = layer_settings->setting<bool>(PRS::MachineSetup::kSupportG3);
    const int arcs_per_revolution   = std::max(1, layer_settings->setting<int>(PS::Slicing::kArcsPerRevolution));
    const QVector<Point> arc_points = write_arcs ? SlicingUtilities::GetCylindricalArcPoints(
                                                       polyline, center, radius, arcs_per_revolution, counterclockwise)
                                                 : QVector<Point>();
    const Point path_start          = arc_points.size() > 1 ? arc_points.first() : polyline.first();
    const Point path_end            = arc_points.size() > 1 ? arc_points.last() : polyline.last();

    QSharedPointer<SettingsBase> region_start_settings = createSegmentSettings(layer_settings, center, true);
    QSharedPointer<SettingsBase> print_settings        = createSegmentSettings(layer_settings, center, false);
    QSharedPointer<TravelSegment> travel = QSharedPointer<TravelSegment>::create(current_location, path_start);
    travel->setSb(region_start_settings);

    if (current_location.distance(path_start) > kMinPathSegmentLength) { path.add(travel); }

    if (arc_points.size() > 1) {
        for (int i = 1, end = arc_points.size(); i < end; ++i) {
            const bool is_arc = SlicingUtilities::IsCylindricalArcSegment(
                arc_points[i - 1], arc_points[i], center, radius, arcs_per_revolution, counterclockwise);
            if (!is_arc && arc_points[i - 1].distance(arc_points[i]) <= kMinPathSegmentLength) { continue; }

            QSharedPointer<SegmentBase> segment;
            if (is_arc) {
                const Point arc_center =
                    SlicingUtilities::GetCylindricalArcCenter(arc_points[i - 1], arc_points[i], center);
                segment =
                    QSharedPointer<ArcSegment>::create(arc_points[i - 1], arc_points[i], arc_center, counterclockwise);
            }
            else { segment = QSharedPointer<LineSegment>::create(arc_points[i - 1], arc_points[i]); }

            segment->setSb(i == 1 ? region_start_settings : print_settings);
            path.add(segment);
        }
    }
    else {
        for (int i = 1, end = polyline.size(); i < end; ++i) {
            if (polyline[i - 1].distance(polyline[i]) <= kMinPathSegmentLength) { continue; }

            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(polyline[i - 1], polyline[i]);
            segment->setSb(i == 1 ? region_start_settings : print_settings);
            path.add(segment);
        }
    }

    if (path.size() > 0) { current_location = path_end; }

    return path;
}
}  // namespace ORNL
