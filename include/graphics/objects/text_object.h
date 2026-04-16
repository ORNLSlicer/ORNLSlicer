#pragma once

#include <qhashfunctions.h>

#include "graphics/graphics_object.h"

namespace ORNL {
/*!
 * \brief A graphic that renders a cube with a text texture. The object billboards to the camera if enabled.
 */
class TextObject : public GraphicsObject {
  public:
    //! \brief Constructor
    //! \param view: The view this object renders to.
    //! \param str: String to display.
    //! \param scale: Scale of the object.
    //! \param billboarding: If the object billboards or not.
    TextObject(BaseView* view, QString str, float scale = 1, bool billboarding = true);

    //! \brief Sets the text on the object.
    //! \param str: String to display.
    void setText(QString str);

  private:
    //! \brief String
    QString m_str;
};
} // namespace ORNL
