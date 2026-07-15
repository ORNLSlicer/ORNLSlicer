#pragma once

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "geometry/mesh/mesh_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "threading/traditional_ast.h"
#include "units/unit.h"

namespace ORNL {
class HelicalLayer;
class Part;
class SettingsBase;

/*!
 * @class HelicalSlicer
 * @brief Generates direct toolpaths along clipped helical paths.
 *
 * Helical slicing uses the same vertical axis settings as radial slicing.  For
 * each generated radius, it creates a helix around that axis using
 * x(t)=r*cos(t), y(t)=r*sin(t), and z(t)=z0+(bead_width/(2*pi))*t.  Successive
 * radii are spaced by the configured layer height, starting half a layer height
 * outward from the configured radial initial radius.  The sampled helix is
 * clipped against horizontal model cross sections before being emitted as
 * direct paths.
 *
 * This slicer builds paths directly and stores them in HelicalLayer instances;
 * it intentionally bypasses polymer perimeter/infill/skin/support processing.
 */
class HelicalSlicer : public TraditionalAST {
  public:
    /*!
     * @brief Constructs a helical slicer using the selected cylindrical G-code syntax.
     * @param gcodeLocation Temporary gcode output path used by the slicing thread.
     */
    HelicalSlicer(QString gcodeLocation);

  protected:
    /*!
     * @brief Builds helical layers by clipping helix candidates against each model cross section.
     * @param opt_data Optional process data.  Currently unused by helical slicing.
     */
    void preProcess(nlohmann::json opt_data = nlohmann::json()) override;

    /*!
     * @brief Completes post-processing for helical slicing.
     * @param opt_data Optional process data.  Currently unused by helical slicing.
     */
    void postProcess(nlohmann::json opt_data = nlohmann::json()) override;

    /*!
     * @brief Writes all generated helical layers with the selected cylindrical writer.
     */
    void writeGCode() override;

  private:
    /*!
     * @brief Copies a mesh so clipping can be applied without mutating the loaded model.
     * @param mesh Mesh to copy.
     * @return A copied mesh of the same concrete mesh type, or nullptr if unsupported.
     */
    QSharedPointer<MeshBase> copyMesh(const QSharedPointer<MeshBase>& mesh);

    /*!
     * @brief Resolves the helical cylinder center for the part and settings.
     * @param part_sb Settings that select the cylinder axis source and custom XY values.
     * @param part Part whose centroid is used by Part Centroid mode.
     * @param base_z Z value assigned to the resolved center.
     * @return Helical cylinder center.
     */
    Point helicalCenterForPart(const QSharedPointer<SettingsBase>& part_sb, const QSharedPointer<Part>& part,
                               Distance base_z);

    /*!
     * @brief Computes the largest XY radial distance from the cylinder center to any mesh vertex.
     * @param meshes Meshes whose vertices define the radial extent.
     * @param center XY center of the helical axis.
     * @return Maximum radius in internal distance units.
     */
    double maxRadiusForMeshes(const QVector<QSharedPointer<MeshBase>>& meshes, const Point& center);

    /*!
     * @brief Creates an open polyline approximation of a helix at one radius.
     * @param center XY center of the helical axis.
     * @param radius Radius of the candidate helical layer.
     * @param start_z First bead centerline Z.
     * @param top_z Retained mesh maximum Z.
     * @param bead_width Vertical rise per full revolution and sampling scale.
     * @return Open polyline approximation of the candidate bead.
     */
    Polyline createHelix(const Point& center, Distance radius, Distance start_z, Distance top_z, Distance bead_width);

    /*!
     * @brief Creates segment settings required by the path writer.
     * @param layer_settings Layer-level settings to copy.
     * @param center Helical center stored for C-axis calculation.
     * @param region_start Whether the segment begins a new path region.
     * @return Segment-local settings used by travel and line segments.
     */
    QSharedPointer<SettingsBase> createSegmentSettings(const QSharedPointer<SettingsBase>& layer_settings,
                                                       const Point& center, bool region_start);

    /*!
     * @brief Converts a clipped helical polyline into a path with an optional travel followed by line segments.
     * @param polyline Clipped helix fragment to convert.
     * @param layer_settings Settings for generated segment metadata.
     * @param center Helical center stored on each segment for the writer.
     * @param radius Exact radius of the generated helical path.
     * @param current_location Last emitted endpoint, updated when a path is generated.
     * @return Path containing travel and print segments for this clipped fragment.
     */
    Path createPath(const Polyline& polyline, const QSharedPointer<SettingsBase>& layer_settings, const Point& center,
                    Distance radius, Point& current_location);

    //! @brief Ordered helical layers generated during preprocessing.
    QList<QSharedPointer<HelicalLayer>> m_helical_layers;
};
} // namespace ORNL
