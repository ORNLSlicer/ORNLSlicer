#include "threading/slicers/cylindrical_slicer.h"

#include <QPair>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>

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
#include "slicing/helical_region_profile.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/cylindrical_layer.h"
#include "threading/traditional_ast.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
const Angle kHelicalTopDeadCenterStartAngle = 90.0 * degree;

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
    struct Intersection {
        Point point;
        bool entering_model = false;
    };

    QVector<Intersection> intersections;
    bool first_point_inside = false;
    bool last_point_inside  = false;
    bool has_inside_points  = false;
    bool has_outside_points = false;
};

//! @brief Sampled polyline for one retained helical profile region run.
struct HelicalRegionPolylineRun {
    RegionType region_type = RegionType::kUnknown;
    Polyline polyline;
};

//! @brief Region type and cumulative revolution span for one profile run.
struct HelicalRegionProfileRun {
    RegionType region_type   = RegionType::kUnknown;
    double start_revolutions = 0.0;
    double end_revolutions   = 0.0;
};

//! @brief Cumulative helical profile bounds retained after model clipping.
struct HelicalProfileClipBounds {
    double start_revolutions = 0.0;
    double end_revolutions   = 0.0;
    bool ends_at_model_exit  = false;
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

//! @brief Adds a point unless it duplicates the current polyline endpoint.
void appendDistinct(Polyline& polyline, const Point& point) {
    if (polyline.isEmpty() || polyline.last() != point) { polyline.push_back(point); }
}

//! @brief Linearly interpolates between two points.
Point interpolate(const Point& start, const Point& end, double t) {
    return Point(start.x() + (end.x() - start.x()) * t, start.y() + (end.y() - start.y()) * t,
                 start.z() + (end.z() - start.z()) * t);
}

//! @brief Returns the point on a helical region profile at a cumulative revolution coordinate.
Point pointAtProfileRevolutions(const HelicalRegionProfile& profile, const Point& center, Distance radius,
                                HelicalPathHandedness handedness, Angle start_angle, double revolutions) {
    const double direction = handedness == HelicalPathHandedness::kLeftHanded ? -1.0 : 1.0;
    const double angle     = start_angle() + direction * revolutions * 2.0 * M_PI;
    const Distance z       = profile.zAtRevolutions(revolutions);

    return Point(center.x() + radius() * std::cos(angle), center.y() + radius() * std::sin(angle), z());
}

//! @brief Samples one profile interval without crossing retained region boundaries.
Polyline createProfileRunPolyline(const HelicalRegionProfile& profile, const Point& center, Distance radius,
                                  HelicalPathHandedness handedness, Angle start_angle, double start_revolutions,
                                  double end_revolutions) {
    Polyline polyline;
    if (end_revolutions <= start_revolutions) { return polyline; }

    const double interval_revolutions = end_revolutions - start_revolutions;
    const Distance interval_height =
        profile.zAtRevolutions(end_revolutions) - profile.zAtRevolutions(start_revolutions);
    const double vertical_per_radian = interval_height() / (interval_revolutions * 2.0 * M_PI);
    const double length_per_radian   = std::hypot(radius(), vertical_per_radian);
    const Distance profile_min_pitch = positiveOrFallback(profile.minPitch(), kMinCircleSegmentLength);
    const Distance target_segment_length =
        profile_min_pitch / 2.0 > kMinCircleSegmentLength ? profile_min_pitch / 2.0 : kMinCircleSegmentLength;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(interval_revolutions * 2.0 * M_PI * length_per_radian / target_segment_length())), 1,
        20000);

    polyline.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(segments);
        polyline.push_back(pointAtProfileRevolutions(profile, center, radius, handedness, start_angle,
                                                     start_revolutions + interval_revolutions * t));
    }

    return polyline;
}

//! @brief Returns raw profile-band intervals inside the requested cumulative revolution interval.
QVector<HelicalRegionProfileRun> profileRegionRuns(const HelicalRegionProfile& profile, double start_revolutions,
                                                   double end_revolutions) {
    QVector<HelicalRegionProfileRun> intervals;
    const double interval_start = std::clamp(start_revolutions, 0.0, profile.totalRevolutions());
    const double interval_end   = std::min(end_revolutions, profile.totalRevolutions());
    if (interval_end <= interval_start) { return intervals; }

    for (const HelicalRegionProfileBand& band : profile.bands) {
        const double run_start = std::max(band.start_revolutions, interval_start);
        const double run_end   = std::min(band.endRevolutions(), interval_end);
        if (run_end <= run_start) { continue; }

        intervals.push_back(HelicalRegionProfileRun {band.region_type, run_start, run_end});
    }

    return intervals;
}

//! @brief Samples all helical profile intervals inside the requested cumulative revolution interval.
QVector<HelicalRegionPolylineRun> createHelicalRegionRuns(const HelicalRegionProfile& profile, const Point& center,
                                                          Distance radius, HelicalPathHandedness handedness,
                                                          Angle start_angle, double start_revolutions,
                                                          double end_revolutions) {
    QVector<HelicalRegionPolylineRun> runs;
    const QVector<HelicalRegionProfileRun> intervals = profileRegionRuns(profile, start_revolutions, end_revolutions);

    for (const HelicalRegionProfileRun& interval : intervals) {
        Polyline polyline = createProfileRunPolyline(profile, center, radius, handedness, start_angle,
                                                     interval.start_revolutions, interval.end_revolutions);
        if (polyline.size() >= 2) { runs.push_back(HelicalRegionPolylineRun {interval.region_type, polyline}); }
    }

    return runs;
}

//! @brief Flattens adjacent region runs into one polyline used only for model-intersection testing.
Polyline flattenHelicalRegionRuns(const QVector<HelicalRegionPolylineRun>& runs) {
    Polyline polyline;
    for (const HelicalRegionPolylineRun& run : runs) {
        for (const Point& point : run.polyline) { appendDistinct(polyline, point); }
    }

    return polyline;
}

//! @brief Returns the raw cumulative profile-revolution bounds that remain after model clipping.
std::optional<HelicalProfileClipBounds> retainedProfileClipBounds(const HelicalRegionProfile& profile,
                                                                  const HelixClipResult& clip_result) {
    constexpr double revolution_tolerance = 1.0e-9;

    if (clip_result.intersections.isEmpty()) {
        if (!clip_result.has_inside_points || clip_result.has_outside_points) { return std::nullopt; }

        return HelicalProfileClipBounds {0.0, profile.totalRevolutions(), false};
    }

    if (!clip_result.has_inside_points) { return std::nullopt; }

    double start_revolutions = 0.0;
    if (!clip_result.first_point_inside) {
        const auto first_entry =
            std::find_if(clip_result.intersections.cbegin(), clip_result.intersections.cend(),
                         [](const HelixClipResult::Intersection& intersection) { return intersection.entering_model; });
        if (first_entry == clip_result.intersections.cend()) { return std::nullopt; }

        start_revolutions = profile.revolutionsAtZ(Distance(first_entry->point.z()));
    }

    double retained_revolutions = profile.totalRevolutions();
    bool ends_at_model_exit     = false;
    if (!clip_result.last_point_inside) {
        std::optional<HelixClipResult::Intersection> highest_exit;
        for (const HelixClipResult::Intersection& intersection : clip_result.intersections) {
            if (intersection.entering_model) { continue; }
            if (!highest_exit.has_value() || intersection.point.z() > highest_exit->point.z()) {
                highest_exit = intersection;
            }
        }
        if (!highest_exit.has_value()) { return std::nullopt; }

        retained_revolutions = profile.revolutionsAtZ(Distance(highest_exit->point.z()));
        ends_at_model_exit   = true;
    }

    start_revolutions    = std::clamp(start_revolutions, 0.0, profile.totalRevolutions());
    retained_revolutions = std::clamp(retained_revolutions, 0.0, profile.totalRevolutions());
    if (retained_revolutions <= start_revolutions + revolution_tolerance) { return std::nullopt; }

    return HelicalProfileClipBounds {start_revolutions, retained_revolutions, ends_at_model_exit};
}

//! @brief Returns the nearest cached cross section index for a point Z.
int nearestSectionIndex(const QVector<HelicalCrossSection>& sections, const Point& point) {
    if (sections.isEmpty()) { return -1; }

    const double point_z = point.z();
    const auto upper = std::lower_bound(sections.cbegin(), sections.cend(), point_z,
                                        [](const HelicalCrossSection& section, double z) { return section.z() < z; });

    if (upper == sections.cbegin()) { return 0; }
    if (upper == sections.cend()) { return static_cast<int>(sections.size()) - 1; }

    const auto lower   = upper - 1;
    const auto nearest = point_z - lower->z() <= upper->z() - point_z ? lower : upper;
    return static_cast<int>(std::distance(sections.cbegin(), nearest));
}

//! @brief Checks whether a 3D point is inside the nearest horizontal model section.
bool pointInsideModel(const QVector<HelicalCrossSection>& sections, const Point& point) {
    const int index = nearestSectionIndex(sections, point);
    if (index < 0 || sections[index].geometry.isEmpty()) { return false; }

    return sections[index].geometry.inside(point, true);
}

//! @brief Finds an approximate model-boundary point along a sampled helix segment.
Point findBoundaryPoint(const Point& start, const Point& end, bool start_inside,
                        const QVector<HelicalCrossSection>& sections) {
    Point low       = start;
    Point high      = end;
    bool low_inside = start_inside;

    for (int i = 0; i < 12; ++i) {
        const Point mid       = interpolate(low, high, 0.5);
        const bool mid_inside = pointInsideModel(sections, mid);
        if (mid_inside == low_inside) {
            low        = mid;
            low_inside = mid_inside;
        }
        else { high = mid; }
    }

    return start_inside ? low : high;
}

//! @brief Records sampled-helix model-boundary crossings used for z clipping.
HelixClipResult clipHelixToSections(const Polyline& helix, const QVector<HelicalCrossSection>& sections) {
    HelixClipResult result;
    if (helix.size() < 2 || sections.isEmpty()) { return result; }

    Point previous            = helix.first();
    bool previous_inside      = pointInsideModel(sections, previous);
    result.first_point_inside = previous_inside;
    result.last_point_inside  = previous_inside;
    result.has_inside_points  = previous_inside;
    result.has_outside_points = !previous_inside;

    for (int i = 1, end = helix.size(); i < end; ++i) {
        const Point current       = helix[i];
        const bool current_inside = pointInsideModel(sections, current);
        result.has_inside_points  = result.has_inside_points || current_inside;
        result.has_outside_points = result.has_outside_points || !current_inside;

        if (previous_inside && !current_inside) {
            const Point boundary_point = findBoundaryPoint(previous, current, true, sections);
            result.intersections.push_back(HelixClipResult::Intersection {boundary_point, false});
        }
        else if (!previous_inside && current_inside) {
            const Point boundary_point = findBoundaryPoint(previous, current, false, sections);
            result.intersections.push_back(HelixClipResult::Intersection {boundary_point, true});
        }

        previous                 = current;
        previous_inside          = current_inside;
        result.last_point_inside = current_inside;
    }

    return result;
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
            const HelicalPathZClipRounding z_clip_rounding =
                static_cast<HelicalPathZClipRounding>(part_sb->setting<int>(PS::Helical::kHelicalPathZClipRounding));
            const HelicalPathHandedness handedness =
                static_cast<HelicalPathHandedness>(part_sb->setting<int>(PS::Helical::kHelicalPathHandedness));

            part_generated_paths =
                generateHelicalLayers(part, part_sb, meshes, mesh_min, mesh_max, z_clip_rounding, handedness,
                                      parts_processed, emitPreProcessProgress, emitComputeProgress);
            if (part_generated_paths) {
                effective_z_clip_rounding.push_back(
                    QPair<QString, HelicalPathZClipRounding> {part->name(), z_clip_rounding});
                effective_handedness.push_back(QPair<QString, HelicalPathHandedness> {part->name(), handedness});
            }
        }
        else {
            const RadialPathBoundaryPolicy boundary_policy =
                static_cast<RadialPathBoundaryPolicy>(part_sb->setting<int>(PS::Radial::kRadialPathBoundaryPolicy));
            part_generated_paths = generateRadialLayers(part, part_sb, meshes, mesh_min, mesh_max, boundary_policy,
                                                        parts_processed, emitPreProcessProgress, emitComputeProgress);
        }

        ++parts_processed;
        emitPreProcessProgress(parts_processed, 0.0);
        emitComputeProgress(parts_processed, 0.0);
    }

    QSharedPointer<ArcSpecialtiesWriter> arc_specialties_writer = m_base.dynamicCast<ArcSpecialtiesWriter>();
    if (!arc_specialties_writer.isNull()) {
        arc_specialties_writer->setHelicalPathZClipRounding(effective_z_clip_rounding);
        arc_specialties_writer->setHelicalPathHandedness(effective_handedness);
    }

    if (m_cylindrical_layers.isEmpty()) {
        const QString boundary_setting = selected_path_pattern == CylindricalPathPattern::kHelical
                                             ? "Helical Z Clip Rounding"
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
                                           layer_settings->setting<Angle>(PS::Radial::kRadialPathStartAngle));

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
                                              const Point& mesh_max, HelicalPathZClipRounding z_clip_rounding,
                                              HelicalPathHandedness handedness, int part_index,
                                              const ProgressCallback& emit_pre_process_progress,
                                              const ProgressCallback& emit_compute_progress) {
    const Distance layer_height =
        positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kLayerHeight), kDefaultCylindricalLayerHeight);
    const Distance bead_width = positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kBeadWidth), layer_height);
    const bool helix_counterclockwise = handedness == HelicalPathHandedness::kRightHanded;
    Distance initial_radius           = part_sb->setting<Distance>(PS::Slicing::kCylinderInnerRadius);
    if (initial_radius < 0) { initial_radius = 0.0 * micron; }

    const Distance base_z(mesh_min.z());
    const Distance top_z = cylindricalTopZ(part_sb, base_z, mesh_max.z());
    Point center         = cylinderCenterForPart(part_sb, part, base_z);
    const Distance max_radius(maxRadiusForMeshes(meshes, center));

    const Distance first_radius = initial_radius + (layer_height / 2.0);
    const Distance start_z      = base_z;
    if (start_z >= top_z) {
        emit_pre_process_progress(part_index, 0.0);
        emit_compute_progress(part_index, 0.0);
        return false;
    }

    HelicalRegionProfileParameters profile_params;
    profile_params.start_z                     = start_z;
    profile_params.top_z                       = top_z;
    profile_params.bead_width                  = bead_width;
    profile_params.perimeter_revolutions       = part_sb->setting<int>(PS::Helical::kHelicalPerimeterRevolutions);
    profile_params.inset_revolutions           = part_sb->setting<int>(PS::Helical::kHelicalInsetRevolutions);
    profile_params.perimeter_stepover          = part_sb->setting<Distance>(PS::Helical::kHelicalPerimeterStepover);
    profile_params.inset_stepover              = part_sb->setting<Distance>(PS::Helical::kHelicalInsetStepover);
    profile_params.infill_stepover             = part_sb->setting<Distance>(PS::Helical::kHelicalInfillStepover);
    profile_params.infill_revolutions_rounding = static_cast<HelicalInfillRevolutionsRounding>(
        part_sb->setting<int>(PS::Helical::kHelicalInfillRevolutionsRounding));

    const HelicalRegionProfileResult profile_result = buildHelicalRegionProfile(profile_params);
    if (!profile_result.valid()) {
        const QString message = "Warning: Helical slicing generated no printable paths. " % profile_result.reason;
        qWarning() << message;
        emit statusMessage(message);
        emit_pre_process_progress(part_index, 0.0);
        emit_compute_progress(part_index, 0.0);
        return false;
    }

    const HelicalRegionProfile& profile = profile_result.profile;
    const Distance profile_clip_top_z   = profile.generatedTopZ() < top_z ? profile.generatedTopZ() : top_z;
    if (profile_clip_top_z <= start_z) {
        emit_pre_process_progress(part_index, 0.0);
        emit_compute_progress(part_index, 0.0);
        return false;
    }

    const Distance profile_min_pitch = positiveOrFallback(profile.minPitch(), bead_width);
    const Distance section_spacing =
        profile_min_pitch / 2.0 > kMinSectionSpacing ? profile_min_pitch / 2.0 : kMinSectionSpacing;
    QVector<Distance> section_zs;
    section_zs.push_back(start_z);
    if (kMinSectionSpacing < section_spacing && start_z + kMinSectionSpacing < profile_clip_top_z) {
        section_zs.push_back(start_z + kMinSectionSpacing);
    }
    for (Distance z = start_z + section_spacing; z <= profile_clip_top_z; z += section_spacing) {
        section_zs.push_back(z);
    }
    if (section_zs.isEmpty() || section_zs.last() < profile_clip_top_z) { section_zs.push_back(profile_clip_top_z); }

    const double available_revolutions = profile.revolutionsAtZ(profile_clip_top_z);

    QVector<HelicalCrossSection> cross_sections;
    bool has_geometry      = false;
    int sections_processed = 0;
    for (const Distance z : section_zs) {
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
        emit_pre_process_progress(part_index,
                                  static_cast<double>(sections_processed) / static_cast<double>(section_zs.size()));
    }
    emit_pre_process_progress(part_index, 1.0);

    if (!has_geometry) {
        emit_compute_progress(part_index, 0.0);
        return false;
    }

    bool part_generated_paths = false;
    emit_compute_progress(part_index, 0.0);
    Point current_location(center.x(), center.y(), start_z());
    int helical_layer_number         = 0;
    const int estimated_radius_count = estimateInclusiveCount(first_radius, max_radius, layer_height);
    int radii_processed              = 0;
    for (Distance radius = first_radius; radius <= max_radius; radius += layer_height) {
        QSharedPointer<SettingsBase> layer_settings = QSharedPointer<SettingsBase>::create(*part_sb);
        layer_settings->makeLocalAdjustments(helical_layer_number);

        QSharedPointer<CylindricalLayer> helical_layer = QSharedPointer<CylindricalLayer>::create(
            helical_layer_number + 1, layer_settings, CylindricalPathPattern::kHelical);

        const Angle helical_start_angle =
            kHelicalTopDeadCenterStartAngle + layer_settings->setting<Angle>(PS::Helical::kHelicalStartAngleOffset);
        const QVector<HelicalRegionPolylineRun> available_runs = createHelicalRegionRuns(
            profile, center, radius, handedness, helical_start_angle, 0.0, available_revolutions);
        const Polyline helix                                          = flattenHelicalRegionRuns(available_runs);
        const HelixClipResult clip_result                             = clipHelixToSections(helix, cross_sections);
        const std::optional<HelicalProfileClipBounds> retained_bounds = retainedProfileClipBounds(profile, clip_result);

        if (retained_bounds.has_value()) {
            const Distance retained_start_z = profile.zAtRevolutions(retained_bounds->start_revolutions);
            const Distance retained_top_z   = profile.zAtRevolutions(retained_bounds->end_revolutions);
            const HelicalRegionProfileResult retained_profile_result =
                retained_bounds->ends_at_model_exit
                    ? buildRetainedHelicalRegionProfile(profile_params, retained_start_z, retained_top_z,
                                                        z_clip_rounding)
                    : buildRetainedHelicalRegionProfile(profile_params, retained_start_z, retained_top_z);
            if (!retained_profile_result.valid()) {
                ++helical_layer_number;
                ++radii_processed;
                emit_compute_progress(
                    part_index, static_cast<double>(radii_processed) / static_cast<double>(estimated_radius_count));
                continue;
            }

            const HelicalRegionProfile& retained_profile = retained_profile_result.profile;
            double retained_end_revolutions              = retained_profile.totalRevolutions();
            if (retained_bounds->ends_at_model_exit &&
                z_clip_rounding == HelicalPathZClipRounding::kExactIntersection) {
                retained_end_revolutions = retained_profile.revolutionsAtZ(retained_top_z);
            }

            const QVector<HelicalRegionPolylineRun> retained_runs = createHelicalRegionRuns(
                retained_profile, center, radius, handedness, helical_start_angle, 0.0, retained_end_revolutions);
            Path path;
            Point path_current_location = current_location;

            for (const HelicalRegionPolylineRun& run : retained_runs) {
                if (run.polyline.size() < 2) { continue; }

                path.append(createPath(run.polyline, layer_settings, center, radius, helix_counterclockwise,
                                       path_current_location, run.region_type));
            }

            if (path.size() > 0) {
                current_location = path_current_location;
                helical_layer->addPath(path);

                for (const HelicalRegionPolylineRun& run : retained_runs) {
                    for (const Point& point : run.polyline) {
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
    const QSharedPointer<SettingsBase>& layer_settings, const Point& center, bool region_start,
    RegionType region_type) {
    QSharedPointer<SettingsBase> segment_settings = QSharedPointer<SettingsBase>::create(*layer_settings);
    segment_settings->setSetting(SS::kWidth, layer_settings->setting<Distance>(PS::Layer::kBeadWidth));
    segment_settings->setSetting(SS::kHeight, layer_settings->setting<Distance>(PS::Layer::kLayerHeight));
    segment_settings->setSetting(SS::kSpeed, layer_settings->setting<Velocity>(PS::Layer::kSpeed));
    segment_settings->setSetting(SS::kRegionType, region_type);
    segment_settings->setSetting(SS::kPathModifiers, PathModifiers::kNone);
    segment_settings->setSetting(SS::kMaterialNumber, 0);
    segment_settings->setSetting(SS::kRecipe, 0);
    segment_settings->setSetting(SS::kIsRegionStartSegment, region_start);

    segment_settings->setSetting(kRadialCenterX, Distance(center.x()));
    segment_settings->setSetting(kRadialCenterY, Distance(center.y()));
    return segment_settings;
}

Path CylindricalSlicer::createPath(const Polyline& polyline, const QSharedPointer<SettingsBase>& layer_settings,
                                   const Point& center, Distance radius, bool counterclockwise, Point& current_location,
                                   RegionType region_type) {
    Path path;
    if (polyline.size() < 2) { return path; }

    const bool write_arcs           = layer_settings->setting<bool>(PRS::MachineSetup::kSupportG3);
    const int arcs_per_revolution   = std::max(1, layer_settings->setting<int>(PS::Slicing::kArcsPerRevolution));
    const QVector<Point> arc_points = write_arcs ? SlicingUtilities::GetCylindricalArcPoints(
                                                       polyline, center, radius, arcs_per_revolution, counterclockwise)
                                                 : QVector<Point>();
    const Point path_start          = arc_points.size() > 1 ? arc_points.first() : polyline.first();
    const Point path_end            = arc_points.size() > 1 ? arc_points.last() : polyline.last();

    QSharedPointer<SettingsBase> region_start_settings =
        createSegmentSettings(layer_settings, center, true, region_type);
    QSharedPointer<SettingsBase> print_settings = createSegmentSettings(layer_settings, center, false, region_type);
    QSharedPointer<TravelSegment> travel        = QSharedPointer<TravelSegment>::create(current_location, path_start);
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
