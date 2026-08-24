#pragma once

#include "graphics/graphics_object.h"

namespace ORNL {
/*!
 * \brief Object that draws a set of coordinate arrows in the XYZ directions. A very commonly used object.
 */
class AxesObject : public GraphicsObject {
   public:
    //! \brief Constructor
    //! \param view: View to render to.
    //! \param axis_length: GL length of the axes.
    AxesObject(BaseView* view, float axis_length);

    //! \brief Updates axis length.
    //! \param axis_length: GL length of the axes.
    void updateDimensions(float axis_length);

   private:
    //! \brief Length that object starts with. For scaling.
    float m_starting_length;
};
}  // namespace ORNL
