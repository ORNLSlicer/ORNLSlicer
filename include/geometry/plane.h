#pragma once

#include <QQuaternion>
#include <QVector3D>

#include "geometry/mesh/advanced/mesh_types.h"
#include "point.h"

namespace ORNL {
//! \brief an object for the point, normal representation of a plane
class Plane {
  public:
    //! \brief Default Constructor for point/normal representation
    Plane();

    //! \brief Constructor for point/normal representation
    //! \param point: the point defining the location of the plane
    //! \param normal: vector defining the angle of the plane
    Plane(const Point& point, const QVector3D& normal);

    //! \brief Constructor for point/normal representation using 3 points
    //! \param p0: first point
    //! \param p1: second point
    //! \param p2: third point
    Plane(Point p0, Point p1, Point p2);

    //! \brief Conversion constructor
    //! \param plane: a CGAL 3D plane
    Plane(MeshTypes::Plane_3& plane);

    //! \brief Returns the point used to define the plane
    Point point();

    //! \brief Returns the point used to define the plane
    Point point() const;

    //! \brief Returns the vector normal to the plane
    QVector3D normal();

    //! \brief Returns the vector normal to the plane
    QVector3D normal() const;

    //! \brief Sets the point of the plane
    //! \param point to set the location of the plane
    void point(const Point& point);

    //! \brief Sets the normal vector of the plane
    //! \param 3d vector to set the angle of the plane
    void normal(const QVector3D normal);

    //! \brief Shifts the plane along the x-axis
    //! \param distance to shift by
    void shiftX(double x);

    ///! \brief Shifts the plane along the y-axis
    /// \param distance to shift by
    void shiftY(double y);

    //! \brief Shifts the plane along the z-axis
    //! \param distance to shift by
    void shiftZ(double z);

    //! \brief Shifts the plane along its own normal vector
    //! \param distance to shift by
    void shiftAlongNormal(double d);

    //! \brief Rotates the plane by the quaternion
    //! \param quanternion defining rotation
    void rotate(const QQuaternion& quaternion);

    //! \brief Returns the value of the point evaluated in the plane equation
    //! \param point to be evaluated
    double evaluatePoint(Point point);

    //! \brief Returns the distance bewteen the plane and the given point
    double distanceToPoint(Point point);

    //! \brief coverts this plane to the CGAL type
    //! \return a CGAL plane in cartesian space
    MeshTypes::Plane_3 toCGALPlane();

    //! \brief Checks if two planes are equivalent to within a specified tolerance
    //! \param rhs: plane to compare this plane to
    //! \param epsilon: equivalence tolerance - the allowable distance between the two planes
    //! \return the determination of equivalence
    bool isEqual(const Plane& rhs, double epsilon);

    //! \brief Checks if two planes are equivalent
    bool operator==(const Plane& rhs);

  private:
    Point m_point;
    QVector3D m_normal_vector;

    //! \brief error threshold when comparing multiple slicing planes
    double m_epsilon = .01;
};

} // namespace ORNL
