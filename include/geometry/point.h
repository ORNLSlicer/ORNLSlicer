#pragma once

#include <cstddef>
#include <functional>

#include <QMatrix4x4>
#include <QPoint>
#include <QPointF>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <clipper.hpp>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/mesh/advanced/mesh_types.h"
#include "units/derivative_units.h"
#include "units/unit.h"

// Single Path Lib
#ifdef HAVE_SINGLE_PATH
    #include <single_path/geometry/point.h>
#endif

namespace ORNL {
/*!
 * \class Point
 * \brief Point class that converts to variable other 2d objects
 */
class Point {
  public:
    //! \brief Constructor
    Point();

    //! \brief Constructor
    Point(const float x, const float y, const float z);

    //! \brief Conversion Constructor
    Point(const Distance& x, const Distance& y, const Distance& z = 0);

    //! \brief Conversion Constructor
    Point(const Distance2D& d);

    //! \brief Conversion Constructor
    Point(const Distance3D& d);

    //! \brief Conversion Constructor
    Point(const ClipperLib2::IntPoint& p);

    //! \brief Conversion Constructor
    Point(const MeshTypes::Point_3& p);
    Point(const MeshTypes::Point_2& p);
    static Point FromCGALPoint(MeshTypes::Point_3 p);

    //! \brief Conversion Constructor
    Point(const QPoint& p);

    //! \brief Conversion Constructor
    Point(const QPointF& p);

    Point(const QVector3D& p);

#ifdef HAVE_SINGLE_PATH
    //! \brief Conversion constructor
    Point(SinglePath::Point& point);

    //! \brief Conversion operator
    operator SinglePath::Point() const;
#endif

    /*!
     * \brief Function for going from QVector2D to Point
     *
     * \note The reason this is not a constructor is that it causes
     * ambiguous function calls which cause errors
     */
    static Point fromQVector2D(const QVector2D& p);

    /*!
     * \brief Function for going from QVector3D to Point
     *
     * \note The reason this is not a constructor is that it causes
     * ambiguous function calls which cause errors
     */
    static Point fromQVector3D(const QVector3D& p);

    //! \brief Copy Constructor
    Point(const Point& p);

    //! \brief Rounds the coordinates of the passed point
    //! \param p: the point whose coordinates will be rounded
    //! \return Returns the rounded point
    static Point round(Point p);

    //! \brief Returns the distances from (0, 0, 0) to this point
    Distance distance() const;

    //! \brief Returns distance from this point to Point `p`
    Distance distance(const Point& rhs) const;

    //! \brief compute the dot product
    static float dot(const Point& lhs, const Point& rhs);

    //! \brief compute the dot product
    float dot(const Point& rhs) const;

    //! \brief compute the cross product
    Point cross(const Point& rhs) const;

    //! \brief rotates the point around the origin
    Point rotate(Angle angle, QVector3D axis = {0, 0, 1});

    //! \brief Rotates the point about a specified point
    Point rotateAround(Point center, Angle angle, QVector3D axis = {0, 0, 1});

    //! \brief moves the point along a vector towards another point
    //! \param target: the target
    //! \param dist: the distance to move along the vector
    void moveTowards(const Point& target, const Distance dist);

    /*!
     * \brief Returns whether the length between (0, 0, 0) and this point is
     * shorter than \p d
     *
     * Note: by subtracting points then one can determine if the distance
     * between them is shorter than \p d
     */
    bool shorterThan(Distance rhs) const;

    Point normal(Distance len);

    //! \brief apply matrix to the point and return the resulting point
    Point apply(QMatrix4x4& matrix);

    //! \brief Converts this point to the ClipperLib version
    ClipperLib2::IntPoint toIntPoint() const;

    //! \brief Converts this point to the Distance2D version
    Distance2D toDistance2D() const;

    //! \brief Converts this point to the Distance3D version
    Distance3D toDistance3D() const;

    //! \brief Converts this point to a QPoint
    QPoint toQPoint() const;

    //! \brief Conversion operator to QPoint
    operator QPointF() const;

    //! \brief Converts this point to a QVector2D
    QVector2D toQVector2D() const;

    //! \brief Converts this point to a QVector3D
    QVector3D toQVector3D() const;

    //! \brief Converts this point to a CGAL 3D Point using Cartesian coordinates
    MeshTypes::Kernel::Point_3 toCartesian3D() const;

    //! \brief Converts this point to a CGAL 3D vector type
    MeshTypes::Vector_3 toVector_3() const;

    //! \brief addition operator
    Point operator+(const Point& point);

    //! \brief addition equals operator
    Point operator+=(const Point& rhs);

    //! \brief subtraction operator
    Point operator-(const Point& rhs);

    //! \brief subtraction equals operator
    Point operator-=(const Point& rhs);

    //! \brief constant multiplication operator
    Point operator*(const float rhs) const;

    //! \brief multiplication operator
    Point operator*(const float rhs);

    //! \brief multiplication equals operator
    Point operator*=(const float rhs);

    //! \brief division operator
    Point operator/(const float rhs);

    //! \brief division equals operator
    Point operator/=(const float rhs);

    //! \brief
    bool operator==(const Point& rhs);

    //! \brief not equals operator
    bool operator!=(const Point& point);

    //! \brief Returns the x component of the point
    float x();

    //! \brief Returns the x component of the point
    float x() const;

    //! \brief Sets the x component of the point
    void x(float x);

    //! \brief Sets the x component of the point
    void x(const Distance& x);

    //! \brief Returns the y component of the point
    float y();

    //! \brief Returns the y component of the point
    float y() const;

    //! \brief Sets the y component of the point
    void y(float y);

    //! \brief Sets the y component of the point
    void y(const Distance& y);

    //! \brief Returns the z component of the point
    float z();

    //! \brief Returns the z component of the point
    float z() const;

    //! \brief Sets the z component of the point
    void z(float z);

    //! \brief Sets the z component of the point
    void z(const Distance& z);

    //! \brief sets the settings to apply at this point
    //! \param sb: the settings base
    void setSettings(QSharedPointer<SettingsBase> sb);

    //! \brief gets the settings at this point
    //! \return a settings base
    QSharedPointer<SettingsBase> getSettings();

    //! \brief Sets the normals at this point
    //! \param normal: vector of normals to set
    void setNormals(QVector<QVector3D> normals);

    //! \brief gets the normals at this point
    //! \return vector of normals
    QVector<QVector3D> getNormals() const;

    //! \brief Reverses the order of normals at this point
    void reverseNormals();

    //! \brief Reverses the direction of normals at this point
    void reverseNormalDirections();

    //! \brief returns this points with it X,Y, and Z values as a CSV string
    //! \return a string
    QString toCSVString();

  private:
    float m_x;

    float m_y;

    float m_z;

    //! \brief Vector of normals
    QVector<QVector3D> m_normals;

    //! \brief settings to apply at this point. Used in settings polygons/ regions
    QSharedPointer<SettingsBase> m_sb;

}; // class Point

Point operator*(const double lhs, const Point& rhs);
Point operator*(const QMatrix4x4& lhs, const Point& rhs);
Point operator+(const Point& lhs, const Point& rhs);
Point operator-(const Point& lhs, const Point& rhs);
bool operator==(const Point& lhs, const Point& rhs);
bool operator!=(const Point& lhs, const Point& rhs);
bool operator<(const Point& lhs, const Point& rhs);
} // namespace ORNL

namespace std {
template <> struct hash<ORNL::Point> {
    size_t operator()(const ORNL::Point& pp) const {
        static int prime = 31;
        int result = 89;
        result = result * prime + pp.x();
        result = result * prime + pp.y();
        return result;
    }
};
} // namespace std
