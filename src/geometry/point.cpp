#include "geometry/point.h"

#include <cmath>

#include <clipper.hpp>
#include <qassert.h>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qmath.h>
#include <qmatrix4x4.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/mesh/advanced/mesh_types.h"
#include "units/derivative_units.h"
#include "units/unit.h"
#include "utilities/mathutils.h"

namespace ORNL {
Point::Point() {
    m_x = 0;
    m_y = 0;
    m_z = 0;
}

Point::Point(const float x, const float y, const float z) {
    m_x = x;
    m_y = y;
    m_z = z;
}

Point::Point(const Distance& x, const Distance& y, const Distance& z) {
    m_x = x();
    m_y = y();
    m_z = z();
}

Point::Point(const Distance2D& d) {
    m_x = d.x();
    m_y = d.y();
    m_z = 0;
}

Point::Point(const Distance3D& d) {
    m_x = d.x();
    m_y = d.y();
    m_z = d.z();
}

Point::Point(const ClipperLib2::IntPoint& p) {
    m_x = p.X;
    m_y = p.Y;
    m_z = 0;
}

Point::Point(const MeshTypes::Point_3& p) {
    m_x = p.x();
    m_y = p.y();
    m_z = p.z();
}

Point::Point(const MeshTypes::Point_2& p) {
    m_x = p.x();
    m_y = p.y();
    m_z = 0;
}

Point Point::FromCGALPoint(MeshTypes::Point_3 p) {
    double x = p.x();
    double y = p.y();
    double z = p.z();
    return Point(x, y, z);
}

Point::Point(const QPoint& p) {
    m_x = p.x();
    m_y = p.y();
    m_z = 0;
}

Point::Point(const QPointF& p) {
    m_x = p.x();
    m_y = p.y();
    m_z = 0.0;
}

Point Point::fromQVector2D(const QVector2D& p) {
    Point point;
    point.m_x = p.x();
    point.m_y = p.y();
    return point;
}

Point Point::fromQVector3D(const QVector3D& p) {
    Point point;
    point.m_x = p.x();
    point.m_y = p.y();
    point.m_z = p.z();
    return point;
}

Point::Point(const Point& p) {
    m_x = p.m_x;
    m_y = p.m_y;
    m_z = p.m_z;
    m_sb = p.m_sb;
    m_normals = p.m_normals;
}

Point::Point(const QVector3D& p) {
    m_x = p.x();
    m_y = p.y();
    m_z = p.z();
}

#ifdef HAVE_SINGLE_PATH
Point::Point(SinglePath::Point& point) {
    m_x = point.x();
    m_y = point.y();
    m_z = point.z();
}

ORNL::Point::operator SinglePath::Point() const {
    SinglePath::Point p;
    p.x(m_x);
    p.y(m_y);
    p.z(m_z);
    return p;
}
#endif

Point Point::round(Point p) {
    p.m_x = std::round(p.m_x);
    p.m_y = std::round(p.m_y);
    p.m_z = std::round(p.m_z);

    return p;
}

Distance Point::distance() const { return qSqrt(qPow(m_x, 2) + qPow(m_y, 2) + qPow(m_z, 2)); }

// normalizes the x,y and z values of the point to be between 0 and 1
Point Point::normal(Distance len) {
    Distance _len = (*this).distance();
    if (_len < 1 * micron)
        return Point(len, 0);
    return (*this) * len.to(micron) / _len.to(micron);
}

Distance Point::distance(const Point& rhs) const {
    return qSqrt(qPow(rhs.m_x - m_x, 2) + qPow(rhs.m_y - m_y, 2) + qPow(rhs.m_z - m_z, 2));
}

float Point::dot(const Point& rhs) const { return m_x * rhs.m_x + m_y * rhs.m_y + m_z * rhs.m_z; }

float Point::dot(const Point& lhs, const Point& rhs) {
    return lhs.m_x * rhs.m_x + lhs.m_y * rhs.m_y + lhs.m_z * rhs.m_z;
}

Point Point::cross(const Point& rhs) const {
    return Point(m_y * rhs.m_z - m_z * rhs.m_y, m_z * rhs.m_x - m_x * rhs.m_z, m_x * rhs.m_y - m_y * rhs.m_x);
}

Point Point::rotate(Angle angle, QVector3D axis) { return rotateAround({0, 0, 0}, angle, axis); }

Point Point::rotateAround(Point center, Angle angle, QVector3D axis) {
    QVector3D c = center.toQVector3D();
    QMatrix4x4 m;
    m.rotate(-angle.to(deg), axis);
    QVector3D p = toQVector3D();
    p -= c;
    p = m * p;
    p += c;
    Point result = Point::fromQVector3D(p);

    if (!m_normals.isEmpty()) {
        QVector<QVector3D> normals = m_normals;
        normals[0] = (m * normals[0]).normalized();
        normals[1] = (m * normals[1]).normalized();
        result.setNormals(normals);
    }

    return result;
}

void Point::moveTowards(const Point& target, const Distance dist) {
    QVector3D vector = (target - *this).toQVector3D().normalized();
    vector *= dist();
    *this += vector;
}

bool Point::shorterThan(Distance rhs) const { return distance() < rhs; }

ClipperLib2::IntPoint Point::toIntPoint() const {
    return ClipperLib2::IntPoint(static_cast<long long>(m_x), static_cast<long long>(m_y));
}

Distance2D Point::toDistance2D() const { return Distance2D(m_x, m_y); }

Distance3D Point::toDistance3D() const { return Distance3D(m_x, m_y, m_z); }

QPoint Point::toQPoint() const { return QPoint(static_cast<int>(m_x), static_cast<int>(m_y)); }

ORNL::Point::operator QPointF() const { return QPointF(m_x, m_y); }

QVector2D Point::toQVector2D() const { return QVector2D(m_x, m_y); }

QVector3D Point::toQVector3D() const { return QVector3D(m_x, m_y, m_z); }

MeshTypes::Kernel::Point_3 Point::toCartesian3D() const { return MeshTypes::Kernel::Point_3(m_x, m_y, m_z); }

MeshTypes::Vector_3 Point::toVector_3() const { return MeshTypes::Vector_3(m_x, m_y, m_z); }

Point Point::operator+(const Point& rhs) {
    Point result = Point(m_x + rhs.m_x, m_y + rhs.m_y, m_z + rhs.m_z);
    result.setNormals(this->getNormals());
    return result;
}

Point Point::operator+=(const Point& rhs) {
    m_x += rhs.m_x;
    m_y += rhs.m_y;
    m_z += rhs.m_z;
    return *this;
}

Point Point::operator-(const Point& rhs) {
    Point result = Point(m_x - rhs.m_x, m_y - rhs.m_y, m_z - rhs.m_z);
    result.setNormals(this->getNormals());
    return result;
}

Point Point::operator-=(const Point& rhs) {
    m_x -= rhs.m_x;
    m_y -= rhs.m_y;
    m_z -= rhs.m_z;
    return *this;
}

Point Point::operator*(const float rhs) const {
    Point result = Point(rhs * m_x, rhs * m_y, rhs * m_z);
    result.setNormals(this->getNormals());
    return result;
}

Point Point::operator*(const float rhs) {
    Point result = Point(rhs * m_x, rhs * m_y, rhs * m_z);
    result.setNormals(this->getNormals());
    return result;
}

Point Point::operator*=(const float rhs) {
    m_x *= rhs;
    m_y *= rhs;
    m_z *= rhs;
    return *this;
}

Point Point::operator/(const float rhs) {
    Point result = Point(m_x / rhs, m_y / rhs, m_z / rhs);
    result.setNormals(this->getNormals());
    return result;
}

Point Point::operator/=(const float m) {
    m_x /= m;
    m_y /= m;
    m_z /= m;
    return *this;
}

bool Point::operator==(const Point& rhs) {
    return MathUtils::equals(m_x, rhs.m_x) && MathUtils::equals(m_y, rhs.m_y) && MathUtils::equals(m_z, rhs.m_z);
}

bool Point::operator!=(const Point& rhs) {
    return MathUtils::notEquals(m_x, rhs.m_x) || MathUtils::notEquals(m_y, rhs.m_y) ||
           MathUtils::notEquals(m_z, rhs.m_z);
}

float Point::x() { return m_x; }

float Point::x() const { return m_x; }

void Point::x(float x) { m_x = x; }

void Point::x(const Distance& x) { m_x = x(); }

float Point::y() { return m_y; }

float Point::y() const { return m_y; }

void Point::y(float y) { m_y = y; }

void Point::y(const Distance& y) { m_y = y(); }

float Point::z() { return m_z; }

float Point::z() const { return m_z; }

void Point::z(float z) { m_z = z; }

void Point::z(const Distance& z) { m_z = z(); }

void Point::setSettings(QSharedPointer<SettingsBase> sb) { m_sb = sb; }

QSharedPointer<SettingsBase> Point::getSettings() { return m_sb; }

void Point::setNormals(QVector<QVector3D> normals) { m_normals = normals; }

QVector<QVector3D> Point::getNormals() const { return m_normals; }

void Point::reverseNormals() {
    Q_ASSERT(!m_normals.isEmpty());

    QVector3D temp = m_normals[0];
    m_normals[0] = m_normals[1];
    m_normals[1] = temp;
}

void Point::reverseNormalDirections() {
    Q_ASSERT(!m_normals.isEmpty());

    m_normals[0] *= -1;
    m_normals[1] *= -1;
}

QString Point::toCSVString() {
    return QString(QString::number(m_x) + "," + QString::number(m_y) + "," + QString::number(m_z));
}

Point operator*(const QMatrix4x4& lhs, const Point& rhs) {
    QVector3D p = rhs.toQVector3D();
    Point result = Point(lhs * p);

    if (!rhs.getNormals().isEmpty()) {
        QVector<QVector3D> normals = rhs.getNormals();
        normals[0] = (lhs * normals[0]).normalized();
        normals[1] = (lhs * normals[1]).normalized();
        result.setNormals(normals);
    }

    return result;
}

Point operator*(const float lhs, Point& rhs) { return rhs * lhs; }

Point operator+(const Point& lhs, const Point& rhs) {
    return Point(lhs.x() + rhs.x(), lhs.y() + rhs.y(), lhs.z() + rhs.z());
}

Point operator-(const Point& lhs, const Point& rhs) {
    return Point(lhs.x() - rhs.x(), lhs.y() - rhs.y(), lhs.z() - rhs.z());
}

bool operator==(const Point& lhs, const Point& rhs) {
    return MathUtils::equals(lhs.x(), rhs.x()) && MathUtils::equals(lhs.y(), rhs.y()) &&
           MathUtils::equals(lhs.z(), rhs.z());
}

bool operator!=(const Point& lhs, const Point& rhs) {
    return MathUtils::notEquals(lhs.x(), rhs.x()) || MathUtils::notEquals(lhs.y(), rhs.y()) ||
           MathUtils::notEquals(lhs.z(), rhs.z());
}

bool operator<(const Point& lhs, const Point& rhs) {
    if (lhs.x() < rhs.x()) {
        return true;
    }
    else if (lhs.x() > rhs.x()) {
        return false;
    }
    // lhs.location.x() == rhs.location.x()
    else if (lhs.y() < rhs.y()) {
        return true;
    }
    else if (lhs.y() > rhs.y()) {
        return false;
    }
    // lhs.location.y() == rhs.location.y()
    else if (lhs.z() < rhs.z()) {
        return true;
    }
    else if (lhs.z() > rhs.z()) {
        return false;
    }
    // lhs.location.z() == rhs.location.z()
    else {
        return false;
    }
}
} // namespace ORNL
