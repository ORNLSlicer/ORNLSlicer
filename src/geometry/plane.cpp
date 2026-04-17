#include "geometry/plane.h"

#include <qnumeric.h>
#include <qquaternion.h>
#include <qvectornd.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/point.h"

namespace ORNL {

Plane::Plane() {}

Plane::Plane(const Point& point, const QVector3D& normal) {
    m_point = point;
    m_normal_vector = normal;
}

Plane::Plane(Point p0, Point p1, Point p2) {
    m_point = p0;
    m_normal_vector = QVector3D::crossProduct((p1 - p0).toQVector3D(), ((p2 - p0).toQVector3D()));
    m_normal_vector.normalize();
}

Plane::Plane(MeshTypes::Plane_3& plane) {
    m_point = Point(plane.point());
    m_normal_vector =
        QVector3D(plane.orthogonal_vector().x(), plane.orthogonal_vector().y(), plane.orthogonal_vector().z());
    m_normal_vector.normalize();
}

Point Plane::point() { return m_point; }

Point Plane::point() const { return m_point; }

QVector3D Plane::normal() { return m_normal_vector; }

QVector3D Plane::normal() const { return m_normal_vector; }

void Plane::point(const Point& point) { m_point = point; }

void Plane::normal(const QVector3D normal) { m_normal_vector = normal; }

void Plane::rotate(const QQuaternion& quaternion) {
    m_normal_vector = quaternion.rotatedVector(m_normal_vector);
    m_normal_vector.normalize();
}

void Plane::shiftX(double x) { m_point.x(m_point.x() + x); }

void Plane::shiftY(double y) { m_point.y(m_point.y() + y); }

void Plane::shiftZ(double z) { m_point.z(m_point.z() + z); }

void Plane::shiftAlongNormal(double d) {
    QVector3D n = m_normal_vector;
    n.normalize();
    n *= d;

    m_point = m_point + n;
}

double Plane::evaluatePoint(Point point) {
    /*
       for normal vector <a, b, c> and point (x0, y0, z0)
       the equation for a plane is
       a(x - x0) + b(y - y0) + c(z - z0) = 0
       returns the value of the left side of the equ when
       point is substituted for x, y, z
    */
    double dx = point.x() - m_point.x();
    double dy = point.y() - m_point.y();
    double dz = point.z() - m_point.z();
    return (m_normal_vector.x() * dx) + (m_normal_vector.y() * dy) + (m_normal_vector.z() * dz);
}

double Plane::distanceToPoint(Point point) { return evaluatePoint(point) / m_normal_vector.length(); }

MeshTypes::Plane_3 Plane::toCGALPlane() {
    m_normal_vector.normalize();
    return MeshTypes::Plane_3(
        m_point.toCartesian3D(),
        MeshTypes::Kernel::Vector_3(m_normal_vector.x(), m_normal_vector.y(), m_normal_vector.z()));
}

bool Plane::isEqual(const Plane& rhs, double epsilon) {
    Point diff = rhs.point() - m_point;

    return qFuzzyCompare(this->normal().x(), rhs.normal().x()) && qFuzzyCompare(this->normal().y(), rhs.normal().y()) &&
           qFuzzyCompare(this->normal().z(), rhs.normal().z()) &&
           qAbs(QVector3D::dotProduct(m_normal_vector, QVector3D(diff.x(), diff.y(), diff.z()))) <= epsilon;
}

bool Plane::operator==(const Plane& rhs) { return isEqual(rhs, 0.01); }
} // namespace ORNL
