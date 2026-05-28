#include "threading/slicers/helical_slicer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QTextStream>
#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "cross_section/cross_section.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/radial_writer.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/mesh/open_mesh.h"
#include "geometry/path.h"
#include "geometry/plane.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/helical_layer.h"
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
Distance positiveOrFallback(Distance value, Distance fallback) { return value > 0 ? value : fallback; }

//! @brief Physical fallback used when the radial layer spacing setting is invalid.
const Distance kDefaultHelicalLayerHeight = 1.0 * mm;

//! @brief Smallest helix approximation segment used to avoid excessive candidate points.
const Distance kMinHelixSegmentLength = 100.0 * micron;

//! @brief Smallest Z spacing used for model cross sections.
const Distance kMinSectionSpacing = 100.0 * micron;

//! @brief Minimum path segment length kept after clipping candidate helices.
const Distance kMinPathSegmentLength = 10.0 * micron;

//! @brief Cached horizontal section of the model used to clip helical points.
struct HelicalCrossSection {
    Distance z;
    PolygonList geometry;
};

//! @brief Calculates combined bounds for non-empty meshes.
bool meshBounds(const QVector<QSharedPointer<MeshBase>>& meshes, Point& mesh_min, Point& mesh_max) {
    bool has_bounds = false;
    for (const QSharedPointer<MeshBase>& mesh : meshes) {
        if (mesh == nullptr || mesh->vertices().isEmpty()) {
            continue;
        }

        if (!has_bounds) {
            mesh_min = mesh->min();
            mesh_max = mesh->max();
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

//! @brief Returns the total length of all printable polyline fragments.
double totalPolylineLength(const QVector<Polyline>& polylines) {
    double total_length = 0.0;
    for (const Polyline& polyline : polylines) {
        if (polyline.size() > 1) {
            total_length += polyline.length()();
        }
    }
    return total_length;
}

//! @brief Returns whether model clipping removed any meaningful portion of the helical path.
bool crossesModelBoundary(const Polyline& helix, const QVector<Polyline>& clipped_lines) {
    if (clipped_lines.isEmpty() || helix.size() < 2) {
        return false;
    }

    const double helix_length = helix.length()();
    const double clipped_length = totalPolylineLength(clipped_lines);
    return clipped_length < helix_length - kMinPathSegmentLength();
}

//! @brief Applies the configured boundary policy to a candidate helical path.
QVector<Polyline> applyBoundaryHandling(const Polyline& helix, const QVector<Polyline>& clipped_lines,
                                        RadialBoundaryHandling handling) {
    if (clipped_lines.isEmpty()) {
        return {};
    }

    switch (handling) {
        case RadialBoundaryHandling::kKeepBoundaryCrossingPath:
            return {helix};
        case RadialBoundaryHandling::kDiscardBoundaryCrossingPath:
            return crossesModelBoundary(helix, clipped_lines) ? QVector<Polyline>() : clipped_lines;
        case RadialBoundaryHandling::kClipToModel:
        default:
            return clipped_lines;
    }
}

//! @brief Linearly interpolates between two points.
Point interpolate(const Point& start, const Point& end, double t) {
    return Point(start.x() + (end.x() - start.x()) * t, start.y() + (end.y() - start.y()) * t,
                 start.z() + (end.z() - start.z()) * t);
}

//! @brief Returns the nearest cached cross section index for a point Z.
int nearestSectionIndex(const QVector<HelicalCrossSection>& sections, const Point& point, Distance first_z,
                        Distance section_spacing) {
    if (sections.isEmpty()) {
        return -1;
    }

    const double raw_index = (point.z() - first_z()) / section_spacing();
    return std::clamp(static_cast<int>(std::llround(raw_index)), 0, static_cast<int>(sections.size()) - 1);
}

//! @brief Checks whether a 3D point is inside the nearest horizontal model section.
bool pointInsideModel(const QVector<HelicalCrossSection>& sections, const Point& point, Distance first_z,
                      Distance section_spacing) {
    const int index = nearestSectionIndex(sections, point, first_z, section_spacing);
    if (index < 0 || sections[index].geometry.isEmpty()) {
        return false;
    }

    return sections[index].geometry.inside(point, true);
}

//! @brief Finds an approximate model-boundary point along a sampled helix segment.
Point findBoundaryPoint(const Point& start, const Point& end, bool start_inside,
                        const QVector<HelicalCrossSection>& sections, Distance first_z, Distance section_spacing) {
    Point low = start;
    Point high = end;
    bool low_inside = start_inside;

    for (int i = 0; i < 12; ++i) {
        const Point mid = interpolate(low, high, 0.5);
        const bool mid_inside = pointInsideModel(sections, mid, first_z, section_spacing);
        if (mid_inside == low_inside) {
            low = mid;
            low_inside = mid_inside;
        }
        else {
            high = mid;
        }
    }

    return start_inside ? low : high;
}

//! @brief Clips a sampled helix into contiguous fragments inside the model.
QVector<Polyline> clipHelixToSections(const Polyline& helix, const QVector<HelicalCrossSection>& sections,
                                      Distance first_z, Distance section_spacing) {
    QVector<Polyline> clipped_lines;
    if (helix.size() < 2 || sections.isEmpty()) {
        return clipped_lines;
    }

    Point previous = helix.first();
    bool previous_inside = pointInsideModel(sections, previous, first_z, section_spacing);

    Polyline current_line;
    if (previous_inside) {
        current_line.push_back(previous);
    }

    for (int i = 1, end = helix.size(); i < end; ++i) {
        const Point current = helix[i];
        const bool current_inside = pointInsideModel(sections, current, first_z, section_spacing);

        if (previous_inside && current_inside) {
            if (current_line.isEmpty()) {
                current_line.push_back(previous);
            }
            current_line.push_back(current);
        }
        else if (previous_inside && !current_inside) {
            current_line.push_back(findBoundaryPoint(previous, current, true, sections, first_z, section_spacing));
            if (current_line.size() > 1 && current_line.length() > kMinPathSegmentLength) {
                clipped_lines.push_back(current_line);
            }
            current_line.clear();
        }
        else if (!previous_inside && current_inside) {
            current_line.clear();
            current_line.push_back(findBoundaryPoint(previous, current, false, sections, first_z, section_spacing));
            current_line.push_back(current);
        }

        previous = current;
        previous_inside = current_inside;
    }

    if (current_line.size() > 1 && current_line.length() > kMinPathSegmentLength) {
        clipped_lines.push_back(current_line);
    }

    return clipped_lines;
}
} // namespace

HelicalSlicer::HelicalSlicer(QString gcodeLocation) : TraditionalAST(gcodeLocation) {
    m_syntax = GcodeSyntax::kRadial3Plus2;
    m_base = QSharedPointer<RadialWriter>::create(GcodeMetaList::RadialMeta, GSM->getGlobal());
}

void HelicalSlicer::preProcess(nlohmann::json opt_data) {
    m_helical_layers.clear();
    this->setMaxSteps(0);

    QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
    global_sb->makeGlobalAdjustments();

    QVector<QSharedPointer<Part>> build_parts = SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kBuild);
    QVector<QSharedPointer<MeshBase>> clipping_meshes =
        SlicingUtilities::GetMeshesByType(CSM->parts(), MeshType::kClipping);

    int parts_processed = 0;
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
            continue;
        }

        Point mesh_min;
        Point mesh_max;
        if (!meshBounds(meshes, mesh_min, mesh_max)) {
            continue;
        }

        const Distance layer_height =
            positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kLayerHeight), kDefaultHelicalLayerHeight);
        const Distance bead_width = positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kBeadWidth), layer_height);
        const Distance section_spacing =
            bead_width / 2.0 > kMinSectionSpacing ? bead_width / 2.0 : kMinSectionSpacing;
        const RadialBoundaryHandling boundary_handling =
            static_cast<RadialBoundaryHandling>(part_sb->setting<int>(PS::Slicing::kRadialBoundaryHandling));
        Distance initial_radius = part_sb->setting<Distance>(PS::Slicing::kRadialInitialRadius);
        if (initial_radius < 0) {
            initial_radius = 0.0 * micron;
        }

        const Distance base_z(mesh_min.z());
        const Distance top_z(mesh_max.z());
        Point center = helicalCenterForPart(part_sb, part, base_z);
        const Distance max_radius(maxRadiusForMeshes(meshes, center));

        const Distance first_radius = initial_radius + (layer_height / 2.0);
        const Distance first_bead_z = base_z + (bead_width / 2.0);
        if (first_bead_z >= top_z) {
            continue;
        }

        QVector<HelicalCrossSection> cross_sections;
        bool has_geometry = false;
        for (Distance z = first_bead_z; z <= top_z; z += section_spacing) {
            Plane slicing_plane(Point(center.x(), center.y(), z()), QVector3D(0, 0, 1));
            PolygonList combined_geometry;

            for (const QSharedPointer<MeshBase>& mesh : meshes) {
                Point shift;
                QVector3D average_normal;
                PolygonList geometry =
                    CrossSection::doCrossSection(mesh, slicing_plane, shift, average_normal, part_sb);
                if (geometry.isEmpty()) {
                    continue;
                }

                if (combined_geometry.isEmpty()) {
                    combined_geometry = geometry;
                }
                else {
                    combined_geometry += geometry;
                }
            }

            has_geometry = has_geometry || !combined_geometry.isEmpty();
            cross_sections.push_back(HelicalCrossSection {z, combined_geometry});
        }

        if (cross_sections.isEmpty() || cross_sections.last().z < top_z) {
            Plane slicing_plane(Point(center.x(), center.y(), top_z()), QVector3D(0, 0, 1));
            PolygonList combined_geometry;
            for (const QSharedPointer<MeshBase>& mesh : meshes) {
                Point shift;
                QVector3D average_normal;
                PolygonList geometry =
                    CrossSection::doCrossSection(mesh, slicing_plane, shift, average_normal, part_sb);
                if (geometry.isEmpty()) {
                    continue;
                }

                if (combined_geometry.isEmpty()) {
                    combined_geometry = geometry;
                }
                else {
                    combined_geometry += geometry;
                }
            }

            has_geometry = has_geometry || !combined_geometry.isEmpty();
            cross_sections.push_back(HelicalCrossSection {top_z, combined_geometry});
        }

        if (!has_geometry) {
            continue;
        }

        Point current_location(center.x(), center.y(), first_bead_z());
        int helical_layer_number = 0;
        for (Distance radius = first_radius; radius <= max_radius; radius += layer_height) {
            QSharedPointer<SettingsBase> layer_settings = QSharedPointer<SettingsBase>::create(*part_sb);
            layer_settings->makeLocalAdjustments(helical_layer_number);

            QSharedPointer<HelicalLayer> helical_layer =
                QSharedPointer<HelicalLayer>::create(helical_layer_number + 1, layer_settings);

            Polyline helix = createHelix(center, radius, first_bead_z, top_z, bead_width);
            QVector<Polyline> clipped_lines = clipHelixToSections(helix, cross_sections, first_bead_z, section_spacing);
            QVector<Polyline> candidate_lines = applyBoundaryHandling(helix, clipped_lines, boundary_handling);

            for (const Polyline& line : candidate_lines) {
                if (line.size() < 2) {
                    continue;
                }

                Path path = createPath(line, layer_settings, center, current_location);
                if (path.size() > 0) {
                    helical_layer->addPath(path);
                }
            }

            if (helical_layer->hasPaths()) {
                part->appendStep(helical_layer);
                m_helical_layers.push_back(helical_layer);
                this->setMaxSteps(m_helical_layers.size());
            }

            ++helical_layer_number;
        }

        ++parts_processed;
        emit statusUpdate(StatusUpdateStepType::kPreProcess,
                          build_parts.isEmpty() ? 100 : (double)parts_processed / (double)build_parts.size() * 100);
    }

    if (m_helical_layers.isEmpty()) {
        const QString message =
            "Warning: Helical slicing generated no printable paths. Check Radial Initial Radius, Radial Axis Mode, "
            "clipping meshes, and Radial Boundary Handling.";
        qWarning() << message;
        emit statusMessage(message);
        emit statusUpdate(StatusUpdateStepType::kPreProcess, 100);
    }
}

void HelicalSlicer::postProcess(nlohmann::json opt_data) {
    if (m_helical_layers.isEmpty()) {
        emit statusUpdate(StatusUpdateStepType::kPostProcess, 100);
        return;
    }

    Point current_location = m_helical_layers.first()->getStartLocation();
    for (int layer_index = 0, layer_count = m_helical_layers.size(); layer_index < layer_count; ++layer_index) {
        m_helical_layers[layer_index]->calculateModifiers(current_location);
        emit statusUpdate(StatusUpdateStepType::kPostProcess,
                          (static_cast<double>(layer_index + 1) / static_cast<double>(layer_count)) * 100.0);
    }
}

void HelicalSlicer::writeGCode() {
    QTextStream stream(&m_temp_gcode_output_file);

    const double num_layers = std::max(1.0, static_cast<double>(m_helical_layers.size()));
    int layer_number = 0;
    for (const QSharedPointer<HelicalLayer>& layer : m_helical_layers) {
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

QSharedPointer<MeshBase> HelicalSlicer::copyMesh(const QSharedPointer<MeshBase>& mesh) {
    if (mesh == nullptr) {
        return nullptr;
    }

    if (ClosedMesh* closed_mesh = dynamic_cast<ClosedMesh*>(mesh.get())) {
        return QSharedPointer<ClosedMesh>::create(*closed_mesh);
    }

    if (OpenMesh* open_mesh = dynamic_cast<OpenMesh*>(mesh.get())) {
        return QSharedPointer<OpenMesh>::create(*open_mesh);
    }

    return nullptr;
}

Point HelicalSlicer::helicalCenterForPart(const QSharedPointer<SettingsBase>& part_sb,
                                          const QSharedPointer<Part>& part, Distance base_z) {
    const RadialAxisMode axis_mode = static_cast<RadialAxisMode>(part_sb->setting<int>(PS::Slicing::kRadialAxisMode));

    Point center = part->rootMesh()->centroid();
    if (axis_mode == RadialAxisMode::kCustomXY) {
        center.x(part_sb->setting<Distance>(PS::Slicing::kRadialAxisX));
        center.y(part_sb->setting<Distance>(PS::Slicing::kRadialAxisY));
    }

    center.z(base_z);
    return center;
}

double HelicalSlicer::maxRadiusForMeshes(const QVector<QSharedPointer<MeshBase>>& meshes, const Point& center) {
    double max_radius = 0.0;
    for (const QSharedPointer<MeshBase>& mesh : meshes) {
        if (mesh == nullptr) {
            continue;
        }

        for (const MeshVertex& vertex : mesh->vertices()) {
            const double dx = static_cast<double>(vertex.location.x() - center.x());
            const double dy = static_cast<double>(vertex.location.y() - center.y());
            max_radius = std::max(max_radius, std::hypot(dx, dy));
        }
    }
    return max_radius;
}

Polyline HelicalSlicer::createHelix(const Point& center, Distance radius, Distance start_z, Distance top_z,
                                    Distance bead_width) {
    Polyline helix;
    if (top_z <= start_z || bead_width <= 0) {
        return helix;
    }

    const double vertical_rise_per_radian = bead_width() / (2.0 * M_PI);
    const double max_t = (top_z() - start_z()) / vertical_rise_per_radian;
    if (max_t <= 0.0) {
        return helix;
    }

    const double length_per_radian = std::hypot(radius(), vertical_rise_per_radian);
    const Distance target_segment_length =
        bead_width / 2.0 > kMinHelixSegmentLength ? bead_width / 2.0 : kMinHelixSegmentLength;
    const int segments =
        std::clamp(static_cast<int>(std::ceil(max_t * length_per_radian / target_segment_length())), 1, 20000);

    helix.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        const double t = max_t * static_cast<double>(i) / static_cast<double>(segments);
        helix.push_back(Point(center.x() + radius() * std::cos(t), center.y() + radius() * std::sin(t),
                              start_z() + vertical_rise_per_radian * t));
    }

    return helix;
}

QSharedPointer<SettingsBase> HelicalSlicer::createSegmentSettings(const QSharedPointer<SettingsBase>& layer_settings,
                                                                  const Point& center, bool region_start) {
    QSharedPointer<SettingsBase> segment_settings = QSharedPointer<SettingsBase>::create(*layer_settings);
    segment_settings->setSetting(SS::kWidth, layer_settings->setting<Distance>(PS::Layer::kBeadWidth));
    segment_settings->setSetting(SS::kHeight, layer_settings->setting<Distance>(PS::Layer::kLayerHeight));
    segment_settings->setSetting(SS::kSpeed, layer_settings->setting<Velocity>(PS::Layer::kSpeed));
    segment_settings->setSetting(SS::kRegionType, RegionType::kPerimeter);
    segment_settings->setSetting(SS::kPathModifiers, PathModifiers::kNone);
    segment_settings->setSetting(SS::kMaterialNumber, 0);
    segment_settings->setSetting(SS::kRecipe, 0);
    segment_settings->setSetting(SS::kExtruders, QVector<int> {0});
    segment_settings->setSetting(SS::kIsRegionStartSegment, region_start);

    segment_settings->setSetting(kRadialCenterX, Distance(center.x()));
    segment_settings->setSetting(kRadialCenterY, Distance(center.y()));
    return segment_settings;
}

Path HelicalSlicer::createPath(const Polyline& polyline, const QSharedPointer<SettingsBase>& layer_settings,
                               const Point& center, Point& current_location) {
    Path path;
    if (polyline.size() < 2) {
        return path;
    }

    QSharedPointer<SettingsBase> region_start_settings = createSegmentSettings(layer_settings, center, true);
    QSharedPointer<SettingsBase> print_settings = createSegmentSettings(layer_settings, center, false);
    QSharedPointer<TravelSegment> travel = QSharedPointer<TravelSegment>::create(current_location, polyline.first());
    travel->setSb(region_start_settings);

    if (current_location.distance(polyline.first()) > kMinPathSegmentLength) {
        path.add(travel);
    }

    for (int i = 1, end = polyline.size(); i < end; ++i) {
        if (polyline[i - 1].distance(polyline[i]) <= kMinPathSegmentLength) {
            continue;
        }

        QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(polyline[i - 1], polyline[i]);
        segment->setSb(i == 1 ? region_start_settings : print_settings);
        path.add(segment);
    }

    if (path.size() > 0) {
        current_location = polyline.last();
    }

    return path;
}
} // namespace ORNL
