#include "slicing/buffered_slicer.h"

#include <algorithm>
#include <cmath>
#include <tuple>

#include <qcontainerfwd.h>
#include <qmap.h>
#include <qqueue.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "configs/settings_range.h"
#include "cross_section/cross_section.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "part/part.h"
#include "slicing/slicing_utilities.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
constexpr double kProjectionEpsilon = 1.0e-6;
constexpr double kSlabEpsilon       = 0.01;

bool isJuggerBotSyntax(const QSharedPointer<SettingsBase>& settings) {
    return settings->setting<int>(PRS::MachineSetup::kSyntax) == static_cast<int>(GcodeSyntax::kJuggerBot);
}

bool variableLayerHeightEnabled(const QSharedPointer<SettingsBase>& settings) {
    return settings->setting<bool>(PS::Layer::kEnableVariableLayerHeight) && isJuggerBotSyntax(settings);
}

bool faceOverlapsLayerSlab(Plane slicing_plane, const QVector<MeshVertex>& vertices, const MeshFace& face,
                           double bottom, double top) {
    double min_distance = slicing_plane.distanceToPoint(Point(vertices[face.vertex_index[0]].location));
    double max_distance = min_distance;

    for (int i = 1; i < 3; ++i) {
        const double distance = slicing_plane.distanceToPoint(Point(vertices[face.vertex_index[i]].location));
        min_distance          = std::min(min_distance, distance);
        max_distance          = std::max(max_distance, distance);
    }

    if (max_distance - min_distance <= kSlabEpsilon) { return false; }

    return min_distance <= top + kSlabEpsilon && max_distance >= bottom - kSlabEpsilon;
}

double faceNormalProjection(const QVector<MeshVertex>& vertices, const MeshFace& face,
                            const QVector3D& slicing_normal) {
    const QVector3D p0 = vertices[face.vertex_index[0]].location;
    const QVector3D p1 = vertices[face.vertex_index[1]].location;
    const QVector3D p2 = vertices[face.vertex_index[2]].location;

    QVector3D face_normal = QVector3D::crossProduct(p1 - p0, p2 - p0);
    if (face_normal.lengthSquared() <= kProjectionEpsilon) { return 0.0; }

    face_normal.normalize();
    return std::abs(QVector3D::dotProduct(face_normal, slicing_normal));
}

double maxSurfaceNormalProjectionInLayerSlab(const QSharedPointer<MeshBase>& mesh, Plane slicing_plane,
                                             Distance last_layer_height, Distance layer_height) {
    const QVector<MeshVertex> vertices = mesh->vertices();
    const QVector<MeshFace> faces      = mesh->faces();

    QVector3D slicing_normal = slicing_plane.normal();
    if (slicing_normal.lengthSquared() <= kProjectionEpsilon) { return 0.0; }
    slicing_normal.normalize();

    const double bottom = last_layer_height() / 2.0;
    const double top    = bottom + layer_height();

    double max_projection = 0.0;
    for (const MeshFace& face : faces) {
        if (face.ignore || !faceOverlapsLayerSlab(slicing_plane, vertices, face, bottom, top)) { continue; }

        max_projection = std::max(max_projection, faceNormalProjection(vertices, face, slicing_normal));
    }

    return max_projection;
}
}  // namespace

BufferedSlicer::BufferedSlicer() {}

BufferedSlicer::BufferedSlicer(const QSharedPointer<MeshBase>& mesh, const QSharedPointer<SettingsBase>& settings,
                               QVector<QSharedPointer<Part>> settings_parts,
                               QMap<uint, QSharedPointer<SettingsRange>> ranges, int previous_buffer, int future_buffer,
                               bool use_cgal_cross_section, bool include_build_plate_gap) {
    m_mesh                   = mesh;
    m_settings               = settings;
    m_settings_parts         = settings_parts;
    m_settings_ranges        = ranges;
    m_use_cgal_cross_section = use_cgal_cross_section;

    m_previous_buffer_size = previous_buffer;
    m_future_buffer_size   = future_buffer;

    std::tie(m_slicing_plane, m_mesh_min, m_mesh_max) = SlicingUtilities::GetDefaultSlicingAxis(m_settings, m_mesh);

    // A normal mesh slice starts at the mesh minimum.  For planar support,
    // retain the otherwise-empty layers below a raised mesh so support can
    // reach the physical build plate at Z = 0.
    const QVector3D normal = m_slicing_plane.normal().normalized();
    if (include_build_plate_gap && m_settings->setting<bool>(PS::Support::kEnable) && qFuzzyIsNull(normal.x()) &&
        qFuzzyIsNull(normal.y()) && normal.z() > 0.0f && m_mesh_min.z() > 0.0f) {
        Point build_plate_point = m_slicing_plane.point();
        build_plate_point.z(0.0);
        m_slicing_plane.point(build_plate_point);
    }

    // Fill previous slots will nullptr to start
    for (int i = 0; i < previous_buffer; ++i) m_buffered_slices.enqueue(nullptr);

    // Take first slice
    m_buffered_slices.enqueue(processSingleSlice());

    // Process future slice up to buffer size
    for (int i = 0; i < future_buffer; ++i) m_buffered_slices.enqueue(processSingleSlice());
}

QSharedPointer<BufferedSlicer::SliceMeta> BufferedSlicer::processNextSlice() {
    // Extract slice fromm buffer
    auto current_slice = m_buffered_slices[m_previous_buffer_size];

    // Add new slice to end of the queue
    m_buffered_slices.enqueue(processSingleSlice());

    // Remove old slice from front of queue
    m_buffered_slices.dequeue();

    return current_slice;
}

QSharedPointer<BufferedSlicer::SliceMeta> BufferedSlicer::peekNextSlice() {
    return m_buffered_slices[m_previous_buffer_size];
}

QQueue<QSharedPointer<BufferedSlicer::SliceMeta>> BufferedSlicer::getPreviousSlices() {
    QQueue<QSharedPointer<BufferedSlicer::SliceMeta>> previous_slices;
    for (int i = m_previous_buffer_size - 1; i >= 0; --i) previous_slices.enqueue(m_buffered_slices[i]);

    return previous_slices;
}

QQueue<QSharedPointer<BufferedSlicer::SliceMeta>> BufferedSlicer::getFutureSlices() {
    QQueue<QSharedPointer<BufferedSlicer::SliceMeta>> future_slices;
    for (int i = m_previous_buffer_size + 1, end = m_previous_buffer_size + 1 + m_future_buffer_size; i < end; ++i)
        future_slices.enqueue(m_buffered_slices[i]);

    return future_slices;
}

int BufferedSlicer::getSliceCount() {
    return m_slice_count - m_future_buffer_size;
}

QSharedPointer<BufferedSlicer::SliceMeta> BufferedSlicer::processSingleSlice() {
    QSharedPointer<SliceMeta> slice_meta = nullptr;

    // If mesh_max is above the slicing plane (ie, the slicing plane intersects the part)
    if (m_slicing_plane.evaluatePoint(m_mesh_max) > 0) {
        // Create new layer settings
        QSharedPointer<SettingsBase> layer_specific_settings =
            QSharedPointer<SettingsBase>::create(*m_settings);  // Copy part settings

        // Apply settings ranges if available
        for (const QSharedPointer<SettingsRange>& range : m_settings_ranges) {
            if (range->includesIndex(m_slice_count) && !range->getSb()->json().is_null()) {
                QSharedPointer<SettingsBase> range_sb = range->getSb();
                layer_specific_settings->populate(range_sb);  // Apply range settings overrides
            }
        }
        layer_specific_settings->makeLocalAdjustments(m_slice_count);

        if (layer_specific_settings->setting<Distance>(PS::Layer::kLayerHeight) <= 0) { return nullptr; }

        layer_specific_settings->setSetting(PS::Layer::kLayerHeight,
                                            computeCuspLimitedLayerHeight(layer_specific_settings));

        Plane candidate_plane = m_slicing_plane;
        SlicingUtilities::ShiftSlicingPlane(layer_specific_settings, candidate_plane, m_last_layer_height);

        if (candidate_plane.evaluatePoint(m_mesh_max) < 0) return nullptr;

        CrossSectionData cross_section = computePrimaryCrossSection(candidate_plane, layer_specific_settings);

        m_slicing_plane     = candidate_plane;
        m_last_layer_height = layer_specific_settings->setting<Distance>(PS::Layer::kLayerHeight);

        // Settings regions
        QVector<SettingsPolygon> settings_polygons;
        computeSettingsPolygons(settings_polygons, cross_section.shift_amount);

        SliceMeta meta = {
            m_slice_count,
            layer_specific_settings,
            cross_section.geometry,
            m_slicing_plane,
            settings_polygons,
            cross_section.average_normal,
            cross_section.shift_amount,
            m_additional_shift,
            cross_section.opt_polylines,
        };

        slice_meta = QSharedPointer<SliceMeta>::create(meta);

        ++m_slice_count;
    }

    return slice_meta;
}

BufferedSlicer::CrossSectionData BufferedSlicer::computePrimaryCrossSection(
    Plane slicing_plane, const QSharedPointer<SettingsBase>& settings) {
    CrossSectionData cross_section;
    cross_section.shift_amount = Point(0, 0, 0);  // cross sectioning will add data from the primary mesh

    if (m_use_cgal_cross_section) {
        auto result                 = m_mesh->intersect(slicing_plane);
        cross_section.opt_polylines = result.first;

        // Extract polygons
        for (auto polygon : result.second) { cross_section.geometry += polygon; }

        cross_section.shift_amount = CrossSection::findSlicingPlaneMidPoint(m_mesh, slicing_plane);
    }
    else {
        cross_section.geometry = CrossSection::doCrossSection(m_mesh, slicing_plane, cross_section.shift_amount,
                                                              cross_section.average_normal, settings, false);
    }

    if (settings->setting<bool>(PS::SpecialModes::kEnableOversize) && cross_section.geometry.size() > 0) {
        cross_section.geometry =
            cross_section.geometry.offset(settings->setting<double>(PS::SpecialModes::kOversizeDistance));
    }

    return cross_section;
}

Distance BufferedSlicer::computeCuspLimitedLayerHeight(const QSharedPointer<SettingsBase>& settings) const {
    const Distance standard_layer_height = settings->setting<Distance>(PS::Layer::kLayerHeight);
    if (!variableLayerHeightEnabled(settings)) { return standard_layer_height; }

    const Distance minimum_layer_height = settings->setting<Distance>(PS::Layer::kMinLayerHeight);
    const Distance target_surface_error = settings->setting<Distance>(PS::Layer::kVariableLayerHeightSurfaceError);

    if (standard_layer_height <= 0 || minimum_layer_height <= 0 || minimum_layer_height >= standard_layer_height ||
        target_surface_error <= 0) {
        return standard_layer_height;
    }

    const double max_projection =
        maxSurfaceNormalProjectionInLayerSlab(m_mesh, m_slicing_plane, m_last_layer_height, standard_layer_height);
    if (max_projection <= kProjectionEpsilon) { return standard_layer_height; }

    const double cusp_limited_height = target_surface_error() / max_projection;
    return Distance(std::clamp(cusp_limited_height, minimum_layer_height(), standard_layer_height()));
}

void BufferedSlicer::computeSettingsPolygons(QVector<SettingsPolygon>& settings_polygons, const Point& base_shift) {
    for (const auto& settings_part : m_settings_parts) {
        Point part_shift = base_shift;  // preserve base shift
        QVector3D tmp_vec;
        PolygonList geometry = CrossSection::doCrossSection(settings_part->rootMesh(), m_slicing_plane, part_shift,
                                                            tmp_vec, settings_part->getSb(), true);
        // If cross-section altered shift (it should not with preserve flag), translate to match base
        if (part_shift != base_shift) {
            Point delta = base_shift - part_shift;
            for (Polygon& poly : geometry) {
                for (Point& p : poly) { p = p + delta; }
            }
        }
        QVector<Polygon> geom_vec;
        geom_vec.reserve(geometry.size());
        for (const Polygon& poly : geometry) { geom_vec.push_back(poly); }
        auto sb = settings_part->getSb();
        settings_polygons.push_back(SettingsPolygon(geom_vec, sb));
    }
}
}  // namespace ORNL
