#include "slicing/buffered_slicer.h"

#include "cross_section/cross_section.h"
#include "slicing/slicing_utilities.h"

namespace ORNL {

BufferedSlicer::BufferedSlicer() {}

BufferedSlicer::BufferedSlicer(const QSharedPointer<MeshBase>& mesh, const QSharedPointer<SettingsBase>& settings,
                               QVector<QSharedPointer<Part>> settings_parts,
                               QMap<uint, QSharedPointer<SettingsRange>> ranges, int previous_buffer, int future_buffer,
                               bool use_cgal_cross_section) {
    m_mesh = mesh;
    m_settings = settings;
    m_settings_parts = settings_parts;
    m_settings_ranges = ranges;
    m_use_cgal_cross_section = use_cgal_cross_section;

    auto closed_mesh = dynamic_cast<ClosedMesh*>(mesh.get());
    m_previous_buffer_size = previous_buffer;
    m_future_buffer_size = future_buffer;

    std::tie(m_slicing_plane, m_mesh_min, m_mesh_max) = SlicingUtilities::GetDefaultSlicingAxis(m_settings, m_mesh);

    if (m_settings->setting<bool>(ES::WireFeed::kSettingsRegionMeshSplit)) {
        QSharedPointer<ClosedMesh> single_setting_mesh = QSharedPointer<ClosedMesh>::create();

        if (settings_parts.size() > 0) {
            QVector<QSharedPointer<MeshBase>> all_settings_meshes;
            for (QSharedPointer<Part> part : settings_parts)
                all_settings_meshes += part->meshes();

            QSharedPointer<ClosedMesh> first_mesh = all_settings_meshes.first().staticCast<ClosedMesh>();
            single_setting_mesh = QSharedPointer<ClosedMesh>::create(*first_mesh.get());

            all_settings_meshes.pop_front();
            for (QSharedPointer<MeshBase> mesh : all_settings_meshes)
                SlicingUtilities::UnionMesh(single_setting_mesh, mesh.staticCast<ClosedMesh>());

            ClosedMesh* closed_mesh = dynamic_cast<ClosedMesh*>(mesh.get());
            m_settings_bounded_mesh = QSharedPointer<ClosedMesh>::create(*closed_mesh);
            SlicingUtilities::IntersectMesh(m_settings_bounded_mesh, single_setting_mesh);

            m_settings_remaining_build_mesh = QSharedPointer<ClosedMesh>::create(*closed_mesh);
            SlicingUtilities::ClipMesh(m_settings_remaining_build_mesh,
                                       QVector<QSharedPointer<MeshBase>> {single_setting_mesh});
        }
    }

    // Fill previous slots will nullptr to start
    for (int i = 0; i < previous_buffer; ++i)
        m_buffered_slices.enqueue(nullptr);

    // Take first slice
    m_buffered_slices.enqueue(processSingleSlice());

    // Process future slice up to buffer size
    for (int i = 0; i < future_buffer; ++i)
        m_buffered_slices.enqueue(processSingleSlice());
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
    for (int i = m_previous_buffer_size - 1; i >= 0; --i)
        previous_slices.enqueue(m_buffered_slices[i]);

    return previous_slices;
}

QQueue<QSharedPointer<BufferedSlicer::SliceMeta>> BufferedSlicer::getFutureSlices() {
    QQueue<QSharedPointer<BufferedSlicer::SliceMeta>> future_slices;
    for (int i = m_previous_buffer_size + 1, end = m_previous_buffer_size + 1 + m_future_buffer_size; i < end; ++i)
        future_slices.enqueue(m_buffered_slices[i]);

    return future_slices;
}

int BufferedSlicer::getSliceCount() { return m_slice_count - m_future_buffer_size; }

QSharedPointer<BufferedSlicer::SliceMeta> BufferedSlicer::processSingleSlice() {
    QSharedPointer<SliceMeta> slice_meta = nullptr;

    // If mesh_max is above the slicing plane (ie, the slicing plane intersects the part)
    if (m_slicing_plane.evaluatePoint(m_mesh_max) > 0) {
        // Create new layer settings
        QSharedPointer<SettingsBase> layer_specific_settings =
            QSharedPointer<SettingsBase>::create(*m_settings); // Copy part settings

        // Apply settings ranges if available
        for (const QSharedPointer<SettingsRange>& range : m_settings_ranges) {
            if (range->includesIndex(m_slice_count) && !range->getSb()->json().is_null()) {
                QSharedPointer<SettingsBase> range_sb = range->getSb();
                layer_specific_settings->populate(range_sb); // Apply range settings overrides
            }
        }
        layer_specific_settings->makeLocalAdjustments(m_slice_count);

        // Shift along slicing axis or skeleton
        SlicingUtilities::ShiftSlicingPlane(layer_specific_settings, m_slicing_plane, m_last_layer_height);
        m_last_layer_height = layer_specific_settings->setting<Distance>(PS::Layer::kLayerHeight);

        // If the slicing plane is beyond the max_point of the part, stop
        if (m_slicing_plane.evaluatePoint(m_mesh_max) < 0)
            return nullptr;

        Point shift_amount = Point(0, 0, 0); // cross sectioning will add data (primary mesh)
        QVector3D average_normal;

        PolygonList geometry;
        QVector<Polyline> opt_polylines;

        if (m_use_cgal_cross_section) {
            auto result = m_mesh->intersect(m_slicing_plane);
            opt_polylines = result.first;

            // Extract polygons
            for (auto polygon : result.second) {
                geometry += polygon;
            }

            shift_amount = CrossSection::findSlicingPlaneMidPoint(m_mesh, m_slicing_plane);
        }
        else {
            geometry = CrossSection::doCrossSection(m_mesh, m_slicing_plane, shift_amount, average_normal,
                                                    layer_specific_settings, false);
        }

        if (layer_specific_settings->setting<bool>(PS::SpecialModes::kEnableOversize) && geometry.size() > 0) {
            geometry = geometry.offset(layer_specific_settings->setting<double>(PS::SpecialModes::kOversizeDistance));
        }

        // Settings regions
        QVector<SettingsPolygon> settings_polygons;
        computeSettingsPolygons(settings_polygons, shift_amount);

        PolygonList settings_modified_geometry;
        if (m_settings_remaining_build_mesh != nullptr) {
            Point aux_shift = shift_amount; // preserve base
            QVector3D aux_normal;
            settings_modified_geometry = CrossSection::doCrossSection(
                m_settings_remaining_build_mesh, m_slicing_plane, aux_shift, aux_normal, layer_specific_settings, true);
        }

        PolygonList settings_bounded_geometry;
        if (m_settings_bounded_mesh != nullptr) {
            Point aux_shift = shift_amount;
            QVector3D aux_normal;
            settings_bounded_geometry = CrossSection::doCrossSection(
                m_settings_bounded_mesh, m_slicing_plane, aux_shift, aux_normal, layer_specific_settings, true);
        }

        SliceMeta meta = {
            m_slice_count,
            layer_specific_settings,
            geometry,
            settings_modified_geometry,
            settings_bounded_geometry,
            m_slicing_plane,
            settings_polygons,
            average_normal,
            shift_amount,
            m_additional_shift,
            opt_polylines,
        };

        slice_meta = QSharedPointer<SliceMeta>::create(meta);

        ++m_slice_count;
    }

    return slice_meta;
}

void BufferedSlicer::computeSettingsPolygons(QVector<SettingsPolygon>& settings_polygons, const Point& base_shift) {
    for (const auto& settings_part : m_settings_parts) {
        Point part_shift = base_shift; // preserve base shift
        QVector3D tmp_vec;
        PolygonList geometry = CrossSection::doCrossSection(settings_part->rootMesh(), m_slicing_plane, part_shift,
                                                            tmp_vec, settings_part->getSb(), true);
        // If cross-section altered shift (it should not with preserve flag), translate to match base
        if (part_shift != base_shift) {
            Point delta = base_shift - part_shift;
            for (Polygon& poly : geometry) {
                for (Point& p : poly) {
                    p = p + delta;
                }
            }
        }
        QVector<Polygon> geom_vec;
        geom_vec.reserve(geometry.size());
        for (const Polygon& poly : geometry) {
            geom_vec.push_back(poly);
        }
        auto sb = settings_part->getSb();
        settings_polygons.push_back(SettingsPolygon(geom_vec, sb));
    }
}
} // namespace ORNL
