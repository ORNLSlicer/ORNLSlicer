#pragma once

#include <QColor>
#include <qquaternion.h>

#include "graphics/graphics_object.h"
#include "graphics/objects/cube_object.h"

namespace ORNL {
/*!
 * \brief A simple plane. Derived from CubeObject with an enforced minimal height.
 *
 * The plane object is another commonly used object. It has the ability to lock itself to a specific rotation.
 */
class PlaneObject : public CubeObject {
  public:
    //! \brief Constructor
    //! \param length: Plane length.
    //! \param width: Plane width.
    //! \param color: Plane color.
    PlaneObject(BaseView* view, float length, float width, QColor color = QColor(127, 0, 255, 102));

    //! \brief Sets the rotation of the plane.
    //! \param rotation: New rotation quaternion.
    void setLockedRotationQuaternion(const QQuaternion& rotation);

    //! \brief If the plane should lock rotation.
    void setLockedRotation(bool lock);

    //! \brief Scales the plane to an new length/width.
    void updateDimensions(float length, float width);

  protected:
    //! \brief Handles plane rotation locking.
    void transformationCallback();

  private:
    //! \brief Starting dims.
    float m_starting_length;
    float m_starting_width;

    //! \brief Color
    QColor m_color;

    //! \brief Rotation quaternion.
    QQuaternion m_locked_rotation;
    //! \brief If locked or not.
    bool m_rotation_toggle = false;
};
} // namespace ORNL
