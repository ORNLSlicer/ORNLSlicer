#pragma once

#include <QMetaType>

#include "units/unit.h"

namespace ORNL {
/*!
 * \class Distance2D
 * \brief Two dimensional distance
 */
class Distance2D {
  public:
    Distance2D() : x(0), y(0) {}
    Distance2D(Distance x, Distance y) : x(x), y(y) {}

    Distance x, y;

    bool operator==(const Distance2D& other) { return x == other.x && y == other.y; }

    bool operator!=(const Distance2D& other) { return x != other.x || y != other.y; }
}; // class Distance2D

/*!
 * \class Distance3D
 * \brief Three dimensional distance
 */
class Distance3D {
  public:
    Distance3D() : x(0), y(0), z(0) {}
    Distance3D(Distance x, Distance y, Distance z) : x(x), y(y), z(z) {}

    Distance x, y, z;

    bool operator==(const Distance3D& other) { return x == other.x && y == other.y && z == other.z; }

    bool operator!=(const Distance3D& other) { return x != other.x || y != other.y || z != other.z; }
}; // class Distance3D

/*!
 * \class Distance4D
 * \brief Four dimensional distance
 */
class Distance4D {
  public:
    Distance4D() : x(0), y(0), z(0), w(0) {}
    Distance4D(Distance x, Distance y, Distance z, Distance w) : x(x), y(y), z(z), w(w) {}

    Distance x, y, z, w;

    bool operator==(const Distance4D& other) { return x == other.x && y == other.y && z == other.z && w == other.w; }

    bool operator!=(const Distance4D& other) { return x != other.x || y != other.y || z != other.z || w == other.w; }
}; // class Distance4D

/*!
 * \class Angle3D
 * \brief Three dimensional angle
 */
class Angle3D {
  public:
    Angle3D() : theta(0), phi(0), rho(0) {}
    Angle3D(Angle x, Angle y, Angle z) : theta(x), phi(y), rho(z) {}

    Angle theta, phi, rho;

}; // class Angle3D

} // namespace ORNL

Q_DECLARE_METATYPE(ORNL::Distance2D)
Q_DECLARE_METATYPE(ORNL::Distance3D)
Q_DECLARE_METATYPE(ORNL::Distance4D)
Q_DECLARE_METATYPE(ORNL::Angle3D)
