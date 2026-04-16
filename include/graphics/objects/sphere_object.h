#pragma once

#include <GL/gl.h>

#include <qcolor.h>
#include <qtypes.h>

#include "graphics/graphics_object.h"

namespace ORNL {
/*!
 * \brief A simple object class that draws a sphere.
 *
 * This class is a base object type that can be used as primative for other objects.
 */
class SphereObject : public GraphicsObject {
  public:
    //! \brief Constructor
    //! \param view: View to render to.
    //! \param radius: OpenGL radius.
    //! \param color: Color of object.
    //! \param render_mode: OpenGL render mode. Use GL_TRIANGLES, GL_LINES, etc.
    SphereObject(BaseView* view, float radius, QColor color, ushort render_mode = GL_TRIANGLES);

  private:
    //! \brief Radius
    float m_radius;
};
} // namespace ORNL
