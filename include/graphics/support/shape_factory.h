#pragma once

#include <vector>

#include <QColor>
#include <QMatrix4x4>
#include <qvectornd.h>

#include "geometry/point.h"

namespace ORNL {
//! \brief "Class" for creating various geometries. Input vectors are appended to so shapes can be built additively.
class ShapeFactory {
  public:
    /*! \brief Append the data for a rectangular prism to input vectors
     *
     *  @param length Total X dimension
     *  @param width Total Y dimension
     *  @param height Total Z dimension
     *  @param transform Matrix to apply to each vertex
     *  @param color Color of prism
     *  @param vertices Vector of vertices to append the new vertices to
     *  @param colors Vector of colors to append the new colors to
     *  @param normals Vector of normals to append the new normals to
     */
    static void createRectangle(float length, float width, float height, const QMatrix4x4& transform,
                                const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                                std::vector<float>& normals);

    /*! \brief Append the data for a cylinder to input vectors
     *
     *  @param radius Radius of cylinder
     *  @param height Height of cylinder
     *  @param transform Matrix to apply to each vertex
     *  @param color Color of cylinder
     *  @param vertices Vector of vertices to append the new vertices to
     *  @param colors Vector of colors to append the new colors to
     *  @param normals Vector of normals to append the new normals to
     */
    static void createCylinder(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                               std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals);

    /*! \brief Append the data for a sphere to input vectors
     *
     *  @param radius: Radius of sphere
     *  @param sectorCount: Number of horizontal sectors that make up sphere
     *  @param stackCount: Number of vertical sectors that make up sphere
     *  @param transform: Matrix to apply to each vertex
     *  @param color: Color of sphere
     *  @param vertices: Vector of vertices to append the new vertices to
     *  @param colors: Vector of colors to append the new colors to
     *  @param normals: Vector of normals to append the new normals to
     */
    static void createSphere(float radius, int sectorCount, int stackCount, const QMatrix4x4& transform,
                             const QColor& color, std::vector<float>& vertices, std::vector<float>& colors,
                             std::vector<float>& normals);

    /*! \brief Append the data for a clipped cylinder to the input vector
     *  @param width: Width of the clipped cylinder
     *  @param length: Length of the clipped cylinder
     *  @param height: Height of the clipped cylinder
     *  @param start: Start point of the gcode segment
     *  @param end: End point of the gcode segment
     *  @param color: Color of the clipped cylinder
     *  @param vertices Vertices of the clipped cylinder
     *  @param colors Vertex colors of the clipped cylinder
     *  @param normals Normal vectors of the clipped cylinder
     */
    static void createGcodeCylinder(const float& width, const float& length, const float& height,
                                    const QVector3D& start, const QVector3D& end, const QColor& color,
                                    std::vector<float>& vertices, std::vector<float>& colors,
                                    std::vector<float>& normals);

    /*! \brief Append the data for a clipped cylinder with a custom cross-section normal
     *  @param width: Width of the clipped cylinder
     *  @param length: Length of the clipped cylinder
     *  @param height: Height of the clipped cylinder
     *  @param start: Start point of the gcode segment
     *  @param end: Displacement of the gcode segment
     *  @param display_normal: Cross-section height direction. A zero vector uses the global slicing vector.
     *  @param color: Color of the clipped cylinder
     *  @param vertices Vertices of the clipped cylinder
     *  @param colors Vertex colors of the clipped cylinder
     *  @param normals Normal vectors of the clipped cylinder
     */
    static void createGcodeCylinder(const float& width, const float& length, const float& height,
                                    const QVector3D& start, const QVector3D& end,
                                    const QVector3D& display_normal, const QColor& color,
                                    std::vector<float>& vertices, std::vector<float>& colors,
                                    std::vector<float>& normals);

    /*!
     * \brief appends the data for a arc cylinder to input vectors
     * \note this is an overload that automatically computes the transform
     * @param cylinder_height how thick of a cylinder to draw
     * @param start the start point of the cylinder
     * @param center the center point of the arc
     * @param angle the angle of the arc
     * @param transform the transformation matrix to place the arc at the correct point
     * @param color the color to draw the arc as
     * @param vertices Vector of vertices to append the new vertices to
     * @param colors Vector of colors to append the new colors to
     * @param normals Vector of normals to append the new normals to
     */
    static void createArcCylinder(const float cylinder_height, const Point& start, const Point& center,
                                  const Point& end, bool is_ccw, const QColor& color, std::vector<float>& vertices,
                                  std::vector<float>& colors, std::vector<float>& normals);

    /*!
     * \brief appends the data for a spline cylinder for input values
     * @param cylinder_height how thick of a cylinder to draw
     * @param start the start point of the cylinder
     * @param control_a the first control point
     * @param control_b the second control point
     * @param end the end point of the spline
     * @param color the color to draw the arc as
     * @param vertices Vector of vertices to append the new vertices to
     * @param colors Vector of colors to append the new colors to
     * @param normals Vector of normals to append the new normals to
     */
    static void createSplineCylinder(const float cylinder_height, const Point& start, const Point& control_a,
                                     const Point& control_b, const Point& end, const QColor& color,
                                     std::vector<float>& vertices, std::vector<float>& colors,
                                     std::vector<float>& normals);

    /*! \brief Append the data for a cone to input vectors
     *
     *  @param radius Radius of base of cone
     *  @param height Height from base to tip
     *  @param transform Matrix to apply to each vertex
     *  @param color Color of cone
     *  @param vertices Vector of vertices to append the new vertices to
     *  @param colors Vector of colors to append the new colors to
     *  @param normals Vector of normals to append the new normals to
     */
    static void createCone(float radius, float height, const QMatrix4x4& transform, const QColor& color,
                           std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals);

    /*! \brief Create rectangle for use as a plane.
     *
     *  Constructs a wire frame rectangular prism build volume representation
     *  @param length: length of plane
     *  @param width: length of plane
     *  @param x_grid_dist: Distance between grid lines in x direction
     *  @param y_grid_dist: Distance between grid lines in y direction
     *  @param color Color of resulting volume
     *  @param vertices Vector of vertices to append the new vertices to
     *  @param colors Vector of colors to append the new colors to
     */
    static void createGridPlane(float length, float width, float x_grid_dist, float y_grid_dist, const QColor& color,
                                std::vector<float>& vertices, std::vector<float>& colors);

    /*! \brief Create rectangle for build volume representation
     *
     *  Constructs a wire frame rectangular prism build volume representation
     *  @param min Min value of rectangle
     *  @param max Max value of rectangle
     *  @param x_grid_dist: Distance between grid lines in x direction
     *  @param x_grid_offset: Distance to offset the first grid line from the minimum X
     *  @param y_grid_dist: Distance between grid lines in y direction
     *  @param y_grid_offset: Distance to offset the first grid line from the minimum Y
     *  @param color Color of resulting volume
     *  @param vertices Vector of vertices to append the new vertices to
     *  @param colors Vector of colors to append the new colors to
     */
    static void createBuildVolumeRectangle(const QVector3D& min, const QVector3D& max, const float& x_grid_dist,
                                           const float& x_grid_offset, const float& y_grid_dist,
                                           const float& y_grid_offset, const QColor& color,
                                           std::vector<float>& vertices, std::vector<float>& colors);

    /*! \brief Create cylinder for build volume representation
     *
     *  Constructs a wire frame cylindrical build volume representation
     *  @param radius Radius of circle for top/bottom of cylinder
     *  @param height Height of cylinder
     *  @param x_grid_dist: Distance between grid lines in x direction
     *  @param y_grid_dist: Distance between grid lines in y direction
     *  @param color Color of resulting volume
     *  @param vertices Vector of vertices to append the new vertices to
     *  @param colors Vector of colors to append the new colors to
     */
    static void createBuildVolumeCylinder(float radius, float height, float x_grid_dist, float y_grid_dist,
                                          const QColor& color, std::vector<float>& vertices,
                                          std::vector<float>& colors);

  private:
    /*! \brief Helper function to compute the transformation matrix for a gcode cylinder
     *  @param start Start point of the gcode segment
     *  @param end End point of the gcode segment
     *  @return Transformation matrix to place the cylinder at the correct point and orientation
     */
    static QMatrix4x4 computeGcodeCylinderTransform(const QVector3D& start, const QVector3D& end);

    /*! \brief Helper function to compute the transformation matrix for a gcode cylinder with a custom normal
     *  @param start Start point of the gcode segment
     *  @param end Displacement of the gcode segment
     *  @param display_normal Cross-section height direction. A zero vector uses the global slicing vector.
     *  @return Transformation matrix to place the cylinder at the correct point and orientation
     */
    static QMatrix4x4 computeGcodeCylinderTransform(const QVector3D& start, const QVector3D& end,
                                                    const QVector3D& display_normal);

    /*!
     * \brief appends the data for a clockwise (G2) arc cylinder to input vectors
     * @param cylinder_height how thick of a cylinder to draw
     * @param start the start point of the cylinder
     * @param center the center point of the arc
     * @param angle the angle of the arc
     * @param transform the transformation matrix to place the arc at the correct point
     * @param color the color to draw the arc as
     * @param vertices Vector of vertices to append the new vertices to
     * @param colors Vector of colors to append the new colors to
     * @param normals Vector of normals to append the new normals to
     */
    static void createArcCylinder(float cylinder_height, const Point& start, const Point& center, const Point& end,
                                  const QMatrix4x4& transform, const QColor& color, std::vector<float>& vertices,
                                  std::vector<float>& colors, std::vector<float>& normals);

    /*!
     * \brief appends the data for a counter-clockwise (G3) arc cylinder to input vectors
     * @param cylinder_height how thick of a cylinder to draw
     * @param start the start point of the cylinder
     * @param center the center point of the arc
     * @param angle the angle of the arc
     * @param transform the transformation matrix to place the arc at the correct point
     * @param color the color to draw the arc as
     * @param vertices Vector of vertices to append the new vertices to
     * @param colors Vector of colors to append the new colors to
     * @param normals Vector of normals to append the new normals to
     */
    static void createArcCylinderCCW(float cylinder_height, const Point& start, const Point& center, const Point& end,
                                     const QMatrix4x4& transform, const QColor& color, std::vector<float>& vertices,
                                     std::vector<float>& colors, std::vector<float>& normals);

    //! adds three vectors to array and computes normal/ colors
    //! \param v0 the first vertex
    //! \param v1 the second vertex
    //! \param v2 the third vertex
    //! \param color the color to draw as
    //! \param vertices Vector of vertices to append the new vertices to
    //! \param colors Vector of colors to append the new colors to
    //! \param normals Vector of normals to append the new normals to
    static void appendTriangle(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2, const QColor& color,
                               std::vector<float>& vertices, std::vector<float>& colors, std::vector<float>& normals);
};
} // Namespace ORNL
