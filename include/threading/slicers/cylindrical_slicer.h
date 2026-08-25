#pragma once

#include <functional>

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "geometry/mesh/mesh_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "threading/traditional_ast.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
class CylindricalLayer;
class Part;
class SettingsBase;

/*!
 * @class CylindricalSlicer
 * @brief Generates direct radial or helical cylindrical toolpaths.
 *
 * Cylindrical slicing uses either concentric radial rings/arcs or rising
 * helices around each part's resolved cylinder axis. Common direct-slicer
 * lifecycle, layer ownership, and G-code streaming live here; path pattern
 * differences stay in radial and helical generation helpers.
 *
 * This slicer stores generated paths in CylindricalLayer and bypasses the normal
 * polymer region stack.
 */
class CylindricalSlicer : public TraditionalAST {
   public:
    /*!
     * @brief Constructs a cylindrical slicer using the selected cylindrical G-code syntax.
     * @param gcodeLocation Temporary gcode output path used by the slicing thread.
     */
    CylindricalSlicer(QString gcodeLocation);

    /*!
     * @brief Runs cylindrical slicing with stage reporting that matches direct path generation.
     *
     * Cylindrical slicing generates its printable paths directly instead of using StepThread computation, so it
     * bypasses the TraditionalAST compute queue to avoid reporting a second no-op compute stage.
     */
    void doSlice() override;

   protected:
    /*!
     * @brief Builds cylindrical layers by dispatching to the selected path-pattern generator.
     * @param opt_data Optional process data.  Currently unused by cylindrical slicing.
     */
    void preProcess(nlohmann::json opt_data = nlohmann::json()) override;

    /*!
     * @brief Completes post-processing for cylindrical slicing.
     * @param opt_data Optional process data.  Currently unused by cylindrical slicing.
     */
    void postProcess(nlohmann::json opt_data = nlohmann::json()) override;

    /*!
     * @brief Writes all generated cylindrical layers with the selected cylindrical writer.
     */
    void writeGCode() override;

   private:
    //! @brief Part progress callback used by path-pattern generation helpers.
    using ProgressCallback = std::function<void(int, double)>;

    /*!
     * @brief Copies a mesh so clipping can be applied without mutating the loaded model.
     * @param mesh Mesh to copy.
     * @return A copied mesh of the same concrete mesh type, or nullptr if unsupported.
     */
    QSharedPointer<MeshBase> copyMesh(const QSharedPointer<MeshBase>& mesh);

    /*!
     * @brief Resolves the cylinder center for the part and settings.
     * @param part_sb Settings that select the cylinder axis source and custom XY values.
     * @param part Part whose centroid is used by Part Centroid mode.
     * @param base_z Z value assigned to the resolved center.
     * @return Cylindrical slicing center.
     */
    Point cylinderCenterForPart(const QSharedPointer<SettingsBase>& part_sb, const QSharedPointer<Part>& part,
                                Distance base_z);

    /*!
     * @brief Computes the largest XY radial distance from the cylinder center to any mesh vertex.
     * @param meshes Meshes whose vertices define the radial extent.
     * @param center XY center of the cylinder axis.
     * @return Maximum radius in internal distance units.
     */
    double maxRadiusForMeshes(const QVector<QSharedPointer<MeshBase>>& meshes, const Point& center);

    /*!
     * @brief Generates radial cylindrical layers for one part.
     * @return True if any radial paths were generated for the part.
     */
    bool generateRadialLayers(const QSharedPointer<Part>& part, const QSharedPointer<SettingsBase>& part_sb,
                              const QVector<QSharedPointer<MeshBase>>& meshes, const Point& mesh_min,
                              const Point& mesh_max, RadialPathBoundaryPolicy boundary_policy, int part_index,
                              const ProgressCallback& emit_pre_process_progress,
                              const ProgressCallback& emit_compute_progress);

    /*!
     * @brief Generates helical cylindrical layers for one part.
     * @return True if any helical paths were generated for the part.
     */
    bool generateHelicalLayers(const QSharedPointer<Part>& part, const QSharedPointer<SettingsBase>& part_sb,
                               const QVector<QSharedPointer<MeshBase>>& meshes, const Point& mesh_min,
                               const Point& mesh_max, HelicalPathBoundaryPolicy boundary_policy,
                               HelicalPathZClipRounding z_clip_rounding, HelicalPathHandedness handedness,
                               int part_index, const ProgressCallback& emit_pre_process_progress,
                               const ProgressCallback& emit_compute_progress);

    /*!
     * @brief Creates an open polyline approximation of a helix at one radius.
     * @param center XY center of the cylinder axis.
     * @param radius Radius of the candidate helical path.
     * @param start_z First bead centerline Z.
     * @param top_z Retained mesh maximum Z.
     * @param bead_width Vertical rise per full revolution and sampling scale.
     * @param handedness Angular handedness of the generated helix.
     * @param start_angle Angular start position around the cylinder axis.
     * @return Open polyline approximation of the candidate bead.
     */
    Polyline createHelix(const Point& center, Distance radius, Distance start_z, Distance top_z, Distance bead_width,
                         HelicalPathHandedness handedness, Angle start_angle);

    /*!
     * @brief Creates a closed polyline approximation of a horizontal circle.
     * @param center XY center of the circle.
     * @param radius Radius of the candidate cylindrical layer.
     * @param z Z height of this bead centerline.
     * @param bead_width Width used to choose a practical circle segment length.
     * @param start_angle Angular start position around the cylinder axis.
     * @return Closed polyline approximation of the candidate bead.
     */
    Polyline createCircle(const Point& center, Distance radius, Distance z, Distance bead_width, Angle start_angle);

    /*!
     * @brief Creates segment settings required by the path writer.
     * @param layer_settings Layer-level settings to copy.
     * @param center Cylinder center stored for C-axis calculation.
     * @param region_start Whether the segment begins a new path region.
     * @return Segment-local settings used by travel and line segments.
     */
    QSharedPointer<SettingsBase> createSegmentSettings(const QSharedPointer<SettingsBase>& layer_settings,
                                                       const Point& center, bool region_start);

    /*!
     * @brief Converts a clipped cylindrical polyline into a path with an optional travel followed by line segments.
     * @param polyline Clipped cylindrical fragment to convert.
     * @param layer_settings Settings for generated segment metadata.
     * @param center Cylinder center stored on each segment for the writer.
     * @param radius Exact radius of the generated cylindrical path.
     * @param counterclockwise True when this path should be emitted as counter-clockwise arcs.
     * @param current_location Last emitted endpoint, updated when a path is generated.
     * @return Path containing travel and print segments for this clipped fragment.
     */
    Path createPath(const Polyline& polyline, const QSharedPointer<SettingsBase>& layer_settings, const Point& center,
                    Distance radius, bool counterclockwise, Point& current_location);

    //! @brief Ordered cylindrical layers generated during cylindrical path computation.
    QList<QSharedPointer<CylindricalLayer>> m_cylindrical_layers;

    //! @brief Whether generated helical paths exceeded or confirmed the mesh-derived build maximum.
    bool m_has_generated_path_max_z = false;

    //! @brief Maximum Z reached by generated helical paths, including rounded Clip Z extensions.
    Distance m_generated_path_max_z = 0;
};
}  // namespace ORNL
