#pragma once

#include <qcolor.h>

#include "graphics/graphics_object.h"
#include "graphics/objects/cylinder_object.h"

namespace ORNL {
/*!
 * \brief A plane object that is a circle.
 */
class CylinderPlaneObject : public CylinderObject {
  public:
    //! \brief Constructor.
    //! \param view: View to render to.
    //! \param radius: Plane radius.
    //! \param color: Color of plane.
    CylinderPlaneObject(BaseView* view, float radius, QColor color);

    //! \brief Constructor.
    //! \param view: View to render to.
    //! \param outer_radius: Plane radius.
    //! \param inner_radius: Interior plane radius.
    //! \param color: Color of plane.
    //! \todo This class (the base class actually) has support for inner and outer, but needs shape factory support
    //! first.
    CylinderPlaneObject(BaseView* view, float outer_radius, float inner_radius,
                        QColor color = QColor(127, 0, 255, 102));

    //! \brief Updates plane size.
    //! \param outer_radius: New plane radius.
    //! \param inner_radius: New interior plane radius.
    //! \todo Inner radius is currently unused.
    void updateDimensions(float outer_radius, float inner_radius = -1);

  private:
    //! \brief Radius
    float m_starting_outer_radius;
    //! \brief Color
    QColor m_color;
};
} // namespace ORNL
