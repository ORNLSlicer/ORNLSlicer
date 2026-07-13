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
class Part;
class RadialLayer;
class SettingsBase;

/*!
 * @class RadialSlicer
 * @brief Generates direct toolpaths on concentric cylindrical shells.
 *
 * Radial slicing uses either each model's XY centroid or configured custom XY
 * coordinates as a vertical cylinder axis.  The configured radial initial radius
 * is treated as the inner cylinder boundary, with the first printable radial
 * layer placed half a layer height outward and subsequent layers spaced by the
 * full layer height.  Beads on each radial layer are horizontal rings whose first
 * Z is half a bead width above the retained mesh base, whose subsequent Z values
 * are spaced by the full bead width, and whose upper limit is the retained mesh's
 * maximum Z.
 *
 * This slicer builds paths directly and stores them in RadialLayer instances;
 * it intentionally bypasses polymer perimeter/infill/skin/support processing.
 */
class RadialSlicer : public TraditionalAST {
  public:
    /*!
     * @brief Constructs a radial slicer that emits the selected radial-capable gcode syntax.
     * @param gcodeLocation Temporary gcode output path used by the slicing thread.
     */
    RadialSlicer(QString gcodeLocation);

  protected:
    /*!
     * @brief Builds radial layers by clipping cylindrical ring candidates against each model cross section.
     * @param opt_data Optional process data.  Currently unused by radial slicing.
     */
    void preProcess(nlohmann::json opt_data = nlohmann::json()) override;

    /*!
     * @brief Completes post-processing for radial slicing.
     * @param opt_data Optional process data.  Currently unused by radial slicing.
     */
    void postProcess(nlohmann::json opt_data = nlohmann::json()) override;

    /*!
     * @brief Writes all generated radial layers with the selected radial writer.
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
     * @brief Resolves the radial cylinder center for the part and settings.
     * @param part_sb Settings that select the radial axis mode and custom XY values.
     * @param part Part whose centroid is used by Part Centroid mode.
     * @param base_z Z value assigned to the resolved center.
     * @return Radial cylinder center.
     */
    Point radialCenterForPart(const QSharedPointer<SettingsBase>& part_sb, const QSharedPointer<Part>& part,
                              Distance base_z);

    /*!
     * @brief Computes the largest XY radial distance from the cylinder center to any mesh vertex.
     * @param meshes Meshes whose vertices define the radial extent.
     * @param center XY center of the radial cylinder axis.
     * @return Maximum radius in internal distance units.
     */
    double maxRadiusForMeshes(const QVector<QSharedPointer<MeshBase>>& meshes, const Point& center);

    /*!
     * @brief Creates a closed polyline approximation of a horizontal circle.
     * @param center XY center of the circle.
     * @param radius Radius of the candidate cylindrical layer.
     * @param z Z height of this bead centerline.
     * @param bead_width Width used to choose a practical circle segment length.
     * @return Closed polyline approximation of the candidate bead.
     */
    Polyline createCircle(const Point& center, Distance radius, Distance z, Distance bead_width);

    /*!
     * @brief Creates segment settings required by the path writer.
     * @param layer_settings Layer-level settings to copy.
     * @param center Radial center stored for C-axis calculation.
     * @param region_start Whether the segment begins a new path region.
     * @return Segment-local settings used by travel and line segments.
     */
    QSharedPointer<SettingsBase> createSegmentSettings(const QSharedPointer<SettingsBase>& layer_settings,
                                                       const Point& center, bool region_start);

    /*!
     * @brief Converts a clipped radial polyline into a path with an optional travel followed by line segments.
     * @param polyline Clipped circle fragment to convert.
     * @param layer_settings Settings for generated segment metadata.
     * @param center Radial center stored on each segment for the writer.
     * @param radius Exact radius of the generated circular path.
     * @param current_location Last emitted endpoint, updated when a path is generated.
     * @return Path containing travel and print segments for this clipped arc.
     */
    Path createPath(const Polyline& polyline, const QSharedPointer<SettingsBase>& layer_settings, const Point& center,
                    Distance radius, Point& current_location);

    //! @brief Ordered radial layers generated during preprocessing.
    QList<QSharedPointer<RadialLayer>> m_radial_layers;
};
} // namespace ORNL
