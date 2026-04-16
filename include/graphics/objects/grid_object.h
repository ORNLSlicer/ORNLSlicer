#pragma once

#include <vector>

#include <qcolor.h>

#include "graphics/graphics_object.h"
#include "utilities/constants.h"

namespace ORNL {
/*!
 * \brief Displays a grid on a plane.
 */
class GridObject : public GraphicsObject {
  public:
    //! \brief Constructor.
    //! \param length: Plane length.
    //! \param width: Plane width.
    //! \param x_grid: Distance between grid x positions.
    //! \param y_grid: Distance between grid y positions.
    //! \param color: Color of the grid.
    GridObject(BaseView* view, float length, float width, float x_grid, float y_grid,
               QColor color = Constants::Colors::kBlack);

    //! \brief Updates the dimensions of the grid.
    //! \param length: Plane length.
    //! \param width: Plane width.
    //! \param x_grid: Distance between grid x positions.
    //! \param y_grid: Distance between grid y positions.
    void updateDimensions(float length, float width, float x_grid = -1, float y_grid = -1);

    //! \brief Override for the actual triangles to provide a simple plane. This allows ray cast detection with the
    //! grid.
    std::vector<Triangle> triangles();

  private:
    //! \brief Grid dims.
    float m_length;
    float m_width;
    float m_x_grid;
    float m_y_grid;

    //! \brief Color
    QColor m_color;

    //! \brief Vertices used to check for cast rays.
    std::vector<float> m_collision_vertices;
};
} // namespace ORNL
