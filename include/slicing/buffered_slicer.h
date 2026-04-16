#pragma once

#include "configs/settings_range.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "part/part.h"

namespace ORNL {
//! \class BufferedSlicer
//! \brief Provides a stateful cross-sectional slicer that can buffer. This class performs and tracks cross-sectioning
//! for a certain number of future and past slices. Depending on the future buffer size, when processNextSlice() is
//! called, it actually computes the Nth next slice, however returns whatever the front of the buffer is.
//! \note when the previous or future buffers cannot be filled with valid slices, they are instead filled with nullptr
class BufferedSlicer {
  public:
    //! \struct SliceMeta
    //! \brief Contains a snapshot of various variables when taking a slice
    struct SliceMeta {
        int number;
        QSharedPointer<SettingsBase> settings;
        PolygonList geometry, modified_geometry, setting_bounded_geometry;
        Plane plane;
        QVector<SettingsPolygon> settings_polygons;
        QVector3D average_normal;
        Point shift_amount;
        Point additional_shift;
        QVector<Polyline> opt_polylines;
    };

    //! \brief Default Constructor
    BufferedSlicer();

    //! \brief Constructor
    //! \param mesh the mesh to perform slicing on
    //! \param settings the settings to use for this mesh
    //! \param settings_parts the settings parts that also need sliced and applied
    //! \param ranges the ranges the apply settings along
    //! \param previous_buffer the number of past slices to track
    //! \param future_buffer the numer of future slices to buffer
    //! \param use_cgal_cross_section use cgal cross-sectioning in place of ORNLSlicer's
    BufferedSlicer(const QSharedPointer<MeshBase>& mesh, const QSharedPointer<SettingsBase>& settings,
                   QVector<QSharedPointer<Part>> settings_parts,
                   QMap<uint, QSharedPointer<SettingsRange>> ranges = QMap<uint, QSharedPointer<SettingsRange>>(),
                   int previous_buffer = 0, int future_buffer = 0, bool use_cgal_cross_section = false);

    //! \brief performs the next slice and returns it
    //! \note if m_future_buffer_size is > 0, then this actually computes N + m_future_buffer_size slice, but still
    //! returns the Nth \return a cross-section (SliceMeta) object
    QSharedPointer<SliceMeta> processNextSlice();

    //! \brief returns the Nth + 1 slice from the buffer without taking a new slice
    //! \note if m_future_buffer_size equals 0, then this will return a nullptr
    //! \return a cross-section (SliceMeta) object
    QSharedPointer<SliceMeta> peekNextSlice();

    //! \brief returns preivous slices in a queue starting with the newest and ending with oldest
    //! \return a queue of previous steps
    QQueue<QSharedPointer<SliceMeta>> getPreviousSlices();

    //! \brief returns future slices in a queue starting with the oldest and ending with newest
    //! \return a queue of previous steps
    QQueue<QSharedPointer<SliceMeta>> getFutureSlices();

    //! \brief gets the number of slice consumed by the slicer
    //! \note this does NOT include what if buffered for the future
    //! \return the number of slices taken
    int getSliceCount();

  private:
    //! \brief processes a single slice (cross-sections)
    //! \return a cross-section (SliceMeta) object
    QSharedPointer<BufferedSlicer::SliceMeta> processSingleSlice();

    //! \brief computes cross-sections for settings parts and extracts their geometry, aligned to a base shift
    //! \param settings_polygons a vector to fill with settings polygons
    //! \param base_shift the shift used by the primary mesh cross-section so settings geometry aligns in 2D space
    void computeSettingsPolygons(QVector<SettingsPolygon>& settings_polygons, const Point& base_shift);

    //! \brief the mesh this slicer is slicing
    QSharedPointer<MeshBase> m_mesh;

    //! \brief min and max points on the mesh
    Point m_mesh_min, m_mesh_max;

    //! \brief the current settings
    QSharedPointer<SettingsBase> m_settings;

    //! \brief ranges that may need applied to a slice
    QMap<uint, QSharedPointer<SettingsRange>> m_settings_ranges;

    //! \brief a queue of slices, past, present, and future
    //! \note this is always m_previous_buffer_size + 1 + m_future_buffer_size big
    QQueue<QSharedPointer<SliceMeta>> m_buffered_slices;

    //! \brief use cgal to cross-section
    bool m_use_cgal_cross_section = false;

    //! \brief number of past slices to keep
    int m_previous_buffer_size = 0;

    //! \brief number of future slices to compute ahead
    int m_future_buffer_size = 0;

    //! \brief the total number of slices that this slicer has computed
    int m_slice_count = 0;

    //! \brief the current plane being used to slice
    Plane m_slicing_plane;

    //! \brief the height of the last layer
    Distance m_last_layer_height;

    //! \brief running total of shifts
    QList<Point> m_running_shifts;

    //! \brief additional shift added from this mesh
    Point m_additional_shift;

    //! \brief list of settings parts being tracked
    QVector<QSharedPointer<Part>> m_settings_parts;

    //! \brief Build mesh cut by settings region
    QSharedPointer<ClosedMesh> m_settings_bounded_mesh, m_settings_remaining_build_mesh;
};
} // namespace ORNL
