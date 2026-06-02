#pragma once

#include <vector>

#include <QColor>
#include <QMatrix4x4>
#include <QVector3D>

#include "geometry/point.h"

namespace ORNL {
//! @brief Utility for appending generated geometry to OpenGL buffer vectors.
//!
//! All creation functions append to the supplied vectors so callers can build combined meshes without intermediate
//! allocations.
class ShapeFactory final {
  public:
    ShapeFactory() = delete;

    //! @name Closed Triangle Meshes
    //! @{

    /*! @brief Appends a transformed rectangular-prism triangle mesh.
     *
     *  The prism is centered at the local origin before @p transform is applied.
     *  Output is suitable for GL_TRIANGLES.
     *
     *  @param length Local X dimension.
     *  @param width Local Y dimension.
     *  @param height Local Z dimension.
     *  @param transform Matrix applied to every generated vertex.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     *  @param normals Destination xyz normal buffer to append to.
     */
    static void appendBox(float length, float width, float height, const QMatrix4x4& transform, const QColor& color,
                          std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals);

    /*! @brief Appends a transformed capped-cylinder triangle mesh.
     *
     *  The local cylinder runs from z=0 to z=@p height before @p transform is applied.
     *  Output is suitable for GL_TRIANGLES.
     *
     *  @param radius Local cylinder radius.
     *  @param height Local cylinder height.
     *  @param transform Matrix applied to every generated vertex.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     *  @param normals Destination xyz normal buffer to append to.
     */
    static void appendCylinder(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                               std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals);

    /*! @brief Appends a transformed UV-sphere triangle mesh.
     *
     *  Output is suitable for GL_TRIANGLES.
     *
     *  @param radius Local sphere radius.
     *  @param sector_count Number of longitudinal sectors.
     *  @param stack_count Number of latitudinal stacks.
     *  @param transform Matrix applied to every generated vertex.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     *  @param normals Destination xyz normal buffer to append to.
     */
    static void appendSphere(float radius, int sector_count, int stack_count, const QMatrix4x4& transform,
                             const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                             std::vector<float>& normals);

    /*! @brief Appends a transformed capped-cone triangle mesh.
     *
     *  The local cone base is on z=0 and its tip is at z=@p height before @p transform is applied.
     *  Output is suitable for GL_TRIANGLES.
     *
     *  @param radius Local base radius.
     *  @param height Local height from base to tip.
     *  @param transform Matrix applied to every generated vertex.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     *  @param normals Destination xyz normal buffer to append to.
     */
    static void appendCone(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                           std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals);

    //! @}

    //! @name Toolpath Bead Meshes
    //! @{

    /*! @brief Appends a straight squished-bead triangle mesh between two points.
     *
     *  This is the filled G-code line bead representation, not a round cylinder. The cross-section is clipped/squished
     *  from @p width and @p height, capped at @p start and @p end, and oriented using the current slicing vector.
     *  Output is suitable for GL_TRIANGLES.
     *
     *  @param width Display bead width across the path.
     *  @param length Display bead length along the path.
     *  @param height Display bead height.
     *  @param start Segment start point.
     *  @param end Segment end point.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     *  @param normals Destination xyz normal buffer to append to.
     *  @param quads_per_side Number of quads used to approximate each rounded side of the bead cross-section. Defaults
     *                         to 4.
     */
    static void appendLinearBead(float width, float length, float height, const QVector3D& start, const QVector3D& end,
                                 const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                                 std::vector<float>& normals, unsigned int quads_per_side = 4);

    /*! @brief Appends a circular-arc bead triangle mesh.
     *
     *  The bead follows the XY arc defined by @p start, @p center, @p end, and @p is_ccw. Z is linearly interpolated
     *  from start to end. Output is suitable for GL_TRIANGLES.
     *
     *  @param bead_diameter Diameter of the circular bead cross-section.
     *  @param start Arc start point.
     *  @param center Arc center point.
     *  @param end Arc end point.
     *  @param is_ccw True when the arc travels counter-clockwise.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     *  @param normals Destination xyz normal buffer to append to.
     */
    static void appendArcBead(float bead_diameter, const Point& start, const Point& center, const Point& end,
                              bool is_ccw, const QColor& color, std::vector<float>& vertices,
                              std::vector<float>& colors, std::vector<float>& normals);

    /*! @brief Appends a cubic Bezier bead triangle mesh.
     *
     *  The bead follows the cubic Bezier curve defined by start, two controls, and end. Output is suitable for
     *  GL_TRIANGLES.
     *
     *  @param bead_diameter Diameter of the circular bead cross-section.
     *  @param start Curve start point.
     *  @param control_a First Bezier control point.
     *  @param control_b Second Bezier control point.
     *  @param end Curve end point.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     *  @param normals Destination xyz normal buffer to append to.
     */
    static void appendSplineBead(float bead_diameter, const Point& start, const Point& control_a,
                                 const Point& control_b, const Point& end, const QColor& color,
                                 std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals);

    //! @}

    //! @name Line Geometry
    //! @{

    /*! @brief Appends XY grid lines for a rectangular plane centered at the origin.
     *
     *  Output is suitable for GL_LINES.
     *
     *  @param length Plane length in X.
     *  @param width Plane width in Y.
     *  @param x_grid_dist Distance between grid lines parallel to Y.
     *  @param y_grid_dist Distance between grid lines parallel to X.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     */
    static void appendGridPlaneLines(float length, float width, float x_grid_dist, float y_grid_dist,
                                     const QColor& color, std::vector<float>& vertices, std::vector<float>& colors);

    /*! @brief Appends wireframe and floor-grid lines for a rectangular build volume.
     *
     *  Output is suitable for GL_LINES.
     *
     *  @param min Minimum build-volume corner.
     *  @param max Maximum build-volume corner.
     *  @param x_grid_dist Distance between floor grid lines parallel to Y.
     *  @param x_grid_offset X offset for the first floor grid line.
     *  @param y_grid_dist Distance between floor grid lines parallel to X.
     *  @param y_grid_offset Y offset for the first floor grid line.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     */
    static void appendBuildVolumeBoxLines(const QVector3D& min, const QVector3D& max, float x_grid_dist,
                                          float x_grid_offset, float y_grid_dist, float y_grid_offset,
                                          const QColor& color, std::vector<float>& vertices,
                                          std::vector<float>& colors);

    /*! @brief Appends wireframe and floor-grid lines for a cylindrical build volume.
     *
     *  Output is suitable for GL_LINES.
     *
     *  @param radius Build-volume radius.
     *  @param height Build-volume height.
     *  @param x_grid_dist Distance between floor grid lines parallel to Y.
     *  @param y_grid_dist Distance between floor grid lines parallel to X.
     *  @param color RGBA color applied to every generated vertex.
     *  @param vertices Destination xyz vertex buffer to append to.
     *  @param colors Destination rgba color buffer to append to.
     */
    static void appendBuildVolumeCylinderLines(float radius, float height, float x_grid_dist, float y_grid_dist,
                                               const QColor& color, std::vector<float>& vertices,
                                               std::vector<float>& colors);

    //! @}

  private:
    /*! @brief Computes the transform that places a local +Z bead mesh between two endpoints.
     *  @param start Segment start point.
     *  @param end Segment end point.
     *  @return Transform that translates to @p start and rotates local +Z toward @p end.
     */
    static QMatrix4x4 computeLinearBeadTransform(const QVector3D& start, const QVector3D& end);
};
} // namespace ORNL
