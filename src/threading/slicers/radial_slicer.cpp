#include "threading/slicers/radial_slicer.h"

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
#include "slicing/slicing_utilities.h"
#include "step/layer/radial_layer.h"
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
const Distance kDefaultRadialLayerHeight = 1.0 * mm;

//! @brief Smallest circle approximation segment used to avoid excessive candidate points.
const Distance kMinCircleSegmentLength = 100.0 * micron;

//! @brief Minimum path segment length kept after clipping candidate circles.
const Distance kMinPathSegmentLength = 10.0 * micron;

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

//! @brief Clips a candidate radial ring against the model cross section.
QVector<Polyline> clipCircleToSection(PolygonList& geometry, const Polyline& circle) { return geometry & circle; }

//! @brief Returns whether model clipping removed any meaningful portion of the radial path.
bool crossesModelBoundary(const Polyline& circle, const QVector<Polyline>& clipped_lines) {
    if (clipped_lines.isEmpty() || circle.size() < 2) {
        return false;
    }

    const double circle_length = circle.length()();
    const double clipped_length = totalPolylineLength(clipped_lines);
    return clipped_length < circle_length - kMinPathSegmentLength();
}

//! @brief Applies the configured radial boundary policy to a candidate radial path.
QVector<Polyline> applyBoundaryPolicy(const Polyline& circle, const QVector<Polyline>& clipped_lines,
                                      RadialPathBoundaryPolicy handling) {
    if (clipped_lines.isEmpty()) {
        return {};
    }

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
} // namespace

RadialSlicer::RadialSlicer(QString gcodeLocation) : TraditionalAST(gcodeLocation) {
    m_syntax = GcodeSyntax::kArcSpecialties;
    m_base = QSharedPointer<ArcSpecialtiesWriter>::create(GcodeMetaList::ArcSpecialtiesMeta, GSM->getGlobal());
}

void RadialSlicer::preProcess(nlohmann::json opt_data) {
    m_radial_layers.clear();
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
            positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kLayerHeight), kDefaultRadialLayerHeight);
        const Distance bead_width = positiveOrFallback(part_sb->setting<Distance>(PS::Layer::kBeadWidth), layer_height);
        const RadialPathBoundaryPolicy boundary_policy =
            static_cast<RadialPathBoundaryPolicy>(part_sb->setting<int>(PS::Slicing::kRadialPathBoundaryPolicy));
        Distance initial_radius = part_sb->setting<Distance>(PS::Slicing::kCylinderInnerRadius);
        if (initial_radius < 0) {
            initial_radius = 0.0 * micron;
        }

        const Distance base_z(mesh_min.z());
        const Distance top_z(mesh_max.z());
        Point center = radialCenterForPart(part_sb, part, base_z);
        const Distance max_radius(maxRadiusForMeshes(meshes, center));

        // Match polymer slicing's centerline convention: the first layer sits
        // half a step inside the printable band, then subsequent layers advance
        // by the full configured spacing.
        const Distance first_radius = initial_radius + (layer_height / 2.0);
        const Distance first_bead_z = base_z + (bead_width / 2.0);
        Point current_location(center.x(), center.y(), first_bead_z());

        QVector<RadialCrossSection> cross_sections;
        for (Distance z = first_bead_z; z <= top_z; z += bead_width) {
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

            if (!combined_geometry.isEmpty()) {
                cross_sections.push_back(RadialCrossSection {z, combined_geometry});
            }
        }

        if (cross_sections.isEmpty()) {
            continue;
        }

        int radial_layer_number = 0;
        for (Distance radius = first_radius; radius <= max_radius; radius += layer_height) {
            QSharedPointer<SettingsBase> layer_settings = QSharedPointer<SettingsBase>::create(*part_sb);
            layer_settings->makeLocalAdjustments(radial_layer_number);

            QSharedPointer<RadialLayer> radial_layer =
                QSharedPointer<RadialLayer>::create(radial_layer_number + 1, layer_settings);

            for (RadialCrossSection& section : cross_sections) {
                Polyline circle = createCircle(center, radius, section.z, bead_width);

                // Intersect the horizontal model cross section with the
                // current candidate path, then apply the radial boundary
                // policy for paths intersected by the part boundary.
                QVector<Polyline> clipped_lines = clipCircleToSection(section.geometry, circle);
                QVector<Polyline> candidate_lines = applyBoundaryPolicy(circle, clipped_lines, boundary_policy);

                for (Polyline line : candidate_lines) {
                    if (line.size() < 2) {
                        continue;
                    }

                    for (Point& point : line) {
                        point.z(section.z());
                    }

                    Path path = createPath(line, layer_settings, center, radius, current_location);
                    if (path.size() > 0) {
                        radial_layer->addPath(path);
                    }
                }
            }

            if (radial_layer->hasPaths()) {
                part->appendStep(radial_layer);
                m_radial_layers.push_back(radial_layer);
                this->setMaxSteps(m_radial_layers.size());
            }

            ++radial_layer_number;
        }

        ++parts_processed;
        emit statusUpdate(StatusUpdateStepType::kPreProcess,
                          build_parts.isEmpty() ? 100 : (double)parts_processed / (double)build_parts.size() * 100);
    }

    if (m_radial_layers.isEmpty()) {
        const QString message =
            "Warning: Radial slicing generated no printable paths. Check Cylinder Inner Radius, Cylinder Axis Source, "
            "clipping meshes, and Radial Path Boundary Policy.";
        qWarning() << message;
        emit statusMessage(message);
        emit statusUpdate(StatusUpdateStepType::kPreProcess, 100);
    }
}

void RadialSlicer::postProcess(nlohmann::json opt_data) {
    if (m_radial_layers.isEmpty()) {
        emit statusUpdate(StatusUpdateStepType::kPostProcess, 100);
        return;
    }

    Point current_location = m_radial_layers.first()->getStartLocation();
    for (int layer_index = 0, layer_count = m_radial_layers.size(); layer_index < layer_count; ++layer_index) {
        m_radial_layers[layer_index]->calculateModifiers(current_location);
        emit statusUpdate(StatusUpdateStepType::kPostProcess,
                          (static_cast<double>(layer_index + 1) / static_cast<double>(layer_count)) * 100.0);
    }
}

void RadialSlicer::writeGCode() {
    QTextStream stream(&m_temp_gcode_output_file);

    const double num_layers = std::max(1.0, static_cast<double>(m_radial_layers.size()));
    int layer_number = 0;
    for (const QSharedPointer<RadialLayer>& layer : m_radial_layers) {
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

QSharedPointer<MeshBase> RadialSlicer::copyMesh(const QSharedPointer<MeshBase>& mesh) {
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

Point RadialSlicer::radialCenterForPart(const QSharedPointer<SettingsBase>& part_sb, const QSharedPointer<Part>& part,
                                        Distance base_z) {
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

double RadialSlicer::maxRadiusForMeshes(const QVector<QSharedPointer<MeshBase>>& meshes, const Point& center) {
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

Polyline RadialSlicer::createCircle(const Point& center, Distance radius, Distance z, Distance bead_width) {
    const double circumference = 2.0 * M_PI * radius();
    const Distance target_segment_length =
        bead_width / 2.0 > kMinCircleSegmentLength ? bead_width / 2.0 : kMinCircleSegmentLength;
    const int segments = std::clamp(static_cast<int>(std::ceil(circumference / target_segment_length())), 64, 720);

    Polyline circle;
    circle.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        const double theta = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(segments);
        circle.push_back(Point(center.x() + radius() * std::cos(theta), center.y() + radius() * std::sin(theta), z()));
    }

    return circle;
}

QSharedPointer<SettingsBase> RadialSlicer::createSegmentSettings(const QSharedPointer<SettingsBase>& layer_settings,
                                                                 const Point& center, bool region_start) {
    QSharedPointer<SettingsBase> segment_settings = QSharedPointer<SettingsBase>::create(*layer_settings);
    segment_settings->setSetting(SS::kWidth, layer_settings->setting<Distance>(PS::Layer::kBeadWidth));
    segment_settings->setSetting(SS::kHeight, layer_settings->setting<Distance>(PS::Layer::kLayerHeight));
    segment_settings->setSetting(SS::kSpeed, layer_settings->setting<Velocity>(PS::Layer::kSpeed));
    segment_settings->setSetting(SS::kRegionType, RegionType::kPerimeter);
    segment_settings->setSetting(SS::kPathModifiers, PathModifiers::kNone);
    segment_settings->setSetting(SS::kMaterialNumber, 0);
    segment_settings->setSetting(SS::kRecipe, 0);
    segment_settings->setSetting(SS::kIsRegionStartSegment, region_start);

    // Store the radial center on each segment so the writer can compute C from
    // the endpoint angle without needing slicer-global state.
    segment_settings->setSetting(kRadialCenterX, Distance(center.x()));
    segment_settings->setSetting(kRadialCenterY, Distance(center.y()));
    return segment_settings;
}

Path RadialSlicer::createPath(const Polyline& polyline, const QSharedPointer<SettingsBase>& layer_settings,
                              const Point& center, Distance radius, Point& current_location) {
    Path path;
    if (polyline.size() < 2) {
        return path;
    }

    const bool write_arcs = layer_settings->setting<bool>(PRS::MachineSetup::kSupportG3);
    const int arcs_per_revolution = std::max(1, layer_settings->setting<int>(PS::Slicing::kArcsPerRevolution));
    const QVector<Point> arc_points =
        write_arcs ? SlicingUtilities::GetCylindricalArcPoints(polyline, center, radius, arcs_per_revolution, true)
                   : QVector<Point>();
    const Point path_start = arc_points.size() > 1 ? arc_points.first() : polyline.first();
    const Point path_end = arc_points.size() > 1 ? arc_points.last() : polyline.last();

    QSharedPointer<SettingsBase> region_start_settings = createSegmentSettings(layer_settings, center, true);
    QSharedPointer<SettingsBase> print_settings = createSegmentSettings(layer_settings, center, false);
    QSharedPointer<TravelSegment> travel = QSharedPointer<TravelSegment>::create(current_location, path_start);
    travel->setSb(region_start_settings);

    // Avoid tiny zero-length moves created when clipped arcs share endpoints.
    if (current_location.distance(path_start) > kMinPathSegmentLength) {
        path.add(travel);
    }

    if (arc_points.size() > 1) {
        for (int i = 1, end = arc_points.size(); i < end; ++i) {
            const bool is_arc = SlicingUtilities::IsCylindricalArcSegment(arc_points[i - 1], arc_points[i], center,
                                                                          radius, arcs_per_revolution, true);
            if (!is_arc && arc_points[i - 1].distance(arc_points[i]) <= kMinPathSegmentLength) {
                continue;
            }

            QSharedPointer<SegmentBase> segment;
            if (is_arc) {
                const Point arc_center =
                    SlicingUtilities::GetCylindricalArcCenter(arc_points[i - 1], arc_points[i], center);
                segment = QSharedPointer<ArcSegment>::create(arc_points[i - 1], arc_points[i], arc_center, true);
            }
            else {
                segment = QSharedPointer<LineSegment>::create(arc_points[i - 1], arc_points[i]);
            }

            segment->setSb(i == 1 ? region_start_settings : print_settings);
            path.add(segment);
        }
    }
    else {
        for (int i = 1, end = polyline.size(); i < end; ++i) {
            if (polyline[i - 1].distance(polyline[i]) <= kMinPathSegmentLength) {
                continue;
            }

            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(polyline[i - 1], polyline[i]);
            segment->setSb(i == 1 ? region_start_settings : print_settings);
            path.add(segment);
        }
    }

    if (path.size() > 0) {
        current_location = path_end;
    }

    return path;
}
} // namespace ORNL
