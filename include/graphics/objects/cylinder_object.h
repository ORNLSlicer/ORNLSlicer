#pragma once

#include <qcolor.h>

#include "graphics/graphics_object.h"

namespace ORNL {
/*!
 * \brief Simple graphics object for rendering a cylinder.
 *
 * This class is a base object type that can be used as primative for other objects.
 */
class CylinderObject : public GraphicsObject {
  public:
    //! \brief Constructor.
    //! \param view: View to render to.
    //! \param radius: GL radius of cylinder.
    //! \param height: GL height of cylinder.
    //! \param color: Color of cylinder.
    CylinderObject(BaseView* view, float radius, float height, QColor color);
    //! \brief Constructor.
    //! \param view: View to render to.
    //! \param outer_radius: GL radius of cylinder.
    //! \param inner_radius: GL interior radius of cylinder.
    //! \param height: GL height of cylinder.
    //! \param color: Color of cylinder.
    //! \todo Shape factory needs support for inner radius for this constructor to be useful.
    CylinderObject(BaseView* view, float outer_radius, float inner_radius, float height, QColor color);

  private:
    //! \brief Dims
    float m_inner_radius;
    float m_outer_radius;
    float m_height;
};
} // namespace ORNL
