
#include "utilities/mathutils.h"

#include <math.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <tuple>
#include <utility>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qmath.h>
#include <qmatrix4x4.h>
#include <qminmax.h>
#include <qnumeric.h>
#include <qquaternion.h>
#include <qvectornd.h>

#include "geometry/plane.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
namespace MathUtils {
bool equals(double a, double b) {
    return (a == b) || (std::abs(a - b) < std::abs(std::min(a, b)) * std::numeric_limits<double>::epsilon());
}

bool equals(double a, double b, double eps) { return (a == b) || (std::abs(a - b) < eps); }

bool notEquals(double a, double b) { return !equals(a, b); }

Point center(QVector<Point> points) {
    float x = 0, y = 0, z = 0;

    for (Point point : points) {
        x += point.x();
        y += point.y();
        z += point.z();
    }

    return Point(x / points.size(), y / points.size(), z / points.size());
}

Angle internalAngle(Point A, Point B, Point C) {
    Distance c = A.distance(B);
    Distance b = A.distance(C);
    Distance a = B.distance(C);

    return std::acos((std::pow(c(), 2) + std::pow(a(), 2) - std::pow(b(), 2)) / (2 * c() * a()));
}

Angle signedInternalAngle(Point A, Point B, Point C) {
    double a = qAtan2(B.x() - A.x(), B.y() - A.y());
    double b = qAtan2(B.x() - C.x(), B.y() - C.y());

    return Angle(b - a);
}

double angleFromXAxis(Point A, Point B) {
    double result;
    Distance dx = B.x() - A.x();
    Distance dy = B.y() - A.y();

    if (dx > 0 && dy > 0)
        result = qAtan(dy() / dx());
    else if (dx < 0 && dy > 0)
        result = M_PI + qAtan(dy() / dx());
    else if (dx < 0 && dy < 0)
        result = M_PI + qAtan(dy() / dx());
    else if (dx > 0 && dy < 0)
        result = (2.0 * M_PI) + qAtan(dy() / dx());
    else if (qFuzzyCompare(dx(), 0)) {
        if (dy() >= 0)
            result = M_PI / 2.0;
        else
            result = (3.0 * M_PI) / 2.0;
    }
    else if (qFuzzyCompare(dy(), 0)) {
        if (dx() >= 0)
            result = 0.00;
        else
            result = M_PI;
    }
    else
        result = NAN;

    return result;
}

bool intersect(Point p1, Point q1, Point p2, Point q2) {
    short o1 = orientation(p1, q1, p2);
    short o2 = orientation(p1, q1, q2);
    short o3 = orientation(p2, q2, p1);
    short o4 = orientation(p2, q2, q1);

    if (o1 != o2 && o3 != o4)
        return true;

    if (o1 == 0 && onSegment(p1, p2, q1))
        return true;
    if (o2 == 0 && onSegment(p1, q2, q1))
        return true;
    if (o3 == 0 && onSegment(p2, p1, q2))
        return true;
    if (o4 == 0 && onSegment(p2, q1, q2))
        return true;

    return false;
}

bool onSegment(Point p, Point q, Point r) {
    return q.x() <= qMax(p.x(), r.x()) && q.x() >= qMin(p.x(), r.x()) && q.y() <= qMax(p.y(), r.y()) &&
           q.y() >= qMin(p.y(), r.y());
}

short orientation(Point A, Point B, Point C) {
    float val = (C.x() - A.x()) * (B.y() - A.y()) - (C.y() - A.y()) * (B.x() - A.x());

    //! Point C is colinear to line AB
    if (qFuzzyIsNull(val))
        return 0;

    //! Point C lies to the left of line AB -> orientation of ABC is ccw
    if (val < 0)
        return -1;

    //! Point C lies to the right of line AB -> orientation of ABC is cw
    return 1;
}

Point lineIntersection(Point A, Point B, Point C, Point D) {
    // Line AB represented as a1x + b1y = c1
    double a1 = B.y() - A.y();
    double b1 = A.x() - B.x();
    double c1 = a1 * (A.x()) + b1 * (A.y());

    // Line CD represented as a2x + b2y = c2
    double a2 = D.y() - C.y();
    double b2 = C.x() - D.x();
    double c2 = a2 * C.x() + b2 * C.y();

    double determinant = a1 * b2 - a2 * b1;

    if (determinant == 0)
        return Point();
    else
        return Point((b2 * c1 - b1 * c2) / determinant, (a1 * c2 - a2 * c1) / determinant);
}

Point linePlaneIntersection(Point line_pt, QVector3D line_direction, Plane plane) {
    QVector3D N = plane.normal();
    Point plane_pt = plane.point();

    double t =
        (QVector3D::dotProduct(N, (plane_pt - line_pt).toQVector3D()) / QVector3D::dotProduct(N, line_direction));

    Point result = Point(line_pt.x() + (line_direction.x() * t), line_pt.y() + (line_direction.y() * t),
                         line_pt.z() + (line_direction.z() * t));
    return result;
}

bool nearCollinear(Point A, Point B, Point C, Angle threshold) {
    Distance c = A.distance(B);
    Distance b = A.distance(C);
    Distance a = B.distance(C);

    // if any of the two points are the same
    if (a == 0 || b == 0 || c == 0)
        return true;

    // Or if any of the two points are close enough, 1.0e-6 is hardcoded for now
    Distance d_max = qMax(a, b);
    d_max = qMax(d_max, c);
    Distance d_min = qMin(a, b);
    d_min = qMin(d_min, c);
    if (d_min / d_max < 1.0e-6)
        return true;

    // checking two of three angles is enough. If B is the mid point of collinear ABC
    //   Angle ABC = PI, not 0. But the other two angles are 0
    Angle ABC(std::acos((std::pow(a(), 2) + std::pow(c(), 2) - std::pow(b(), 2)) / (2 * a() * c())));
    Angle BCA(std::acos((std::pow(b(), 2) + std::pow(a(), 2) - std::pow(c(), 2)) / (2 * b() * a())));

    return (ABC < threshold) || (BCA < threshold);
}

bool slopesNearCollinear(Point pt1, Point pt2, Point pt3, Distance distSqrd) {
    if (std::abs(pt1.x() - pt2.x()) > std::abs(pt1.y() - pt2.y())) {
        if ((pt1.x() > pt2.x()) == (pt1.x() < pt3.x())) {
            return distanceFromLineSqrd(pt1, pt2, pt3) < distSqrd;
        }
        else if ((pt2.x() > pt1.x()) == (pt2.x() < pt3.x())) {
            return distanceFromLineSqrd(pt2, pt1, pt3) < distSqrd;
        }
        else {
            return distanceFromLineSqrd(pt3, pt1, pt2) < distSqrd;
        }
    }
    else {
        if ((pt1.y() > pt2.y()) == (pt1.y() < pt3.y())) {
            return distanceFromLineSqrd(pt1, pt2, pt3) < distSqrd;
        }
        else if ((pt2.y() > pt1.y()) == (pt2.y() < pt3.y())) {
            return distanceFromLineSqrd(pt2, pt1, pt3) < distSqrd;
        }
        else {
            return distanceFromLineSqrd(pt3, pt1, pt2) < distSqrd;
        }
    }
}

Distance distanceFromLineSqrd(Point pt, Point ln1, Point ln2) {
    float A = ln1.y() - ln2.y();
    float B = ln2.x() - ln1.x();
    float C = A * ln1.x() + B * ln1.y();
    C = A * pt.x() + B * pt.y() - C;
    return (C * C) / (A * A + B * B);
}

float distanceFromLineSegSqrd(Point pt, Point ln1, Point ln2) {
    float x = ln1.x();
    float y = ln1.y();
    float dx = ln2.x() - x;
    float dy = ln2.y() - y;

    if (dx != 0 || dy != 0) {
        float t = ((pt.x() - x) * dx + (pt.y() - y) * dy) / (dx * dx + dy * dy);

        if (t > 1) {
            x = ln2.x();
            y = ln2.y();
        }
        else if (t > 0) {
            x += dx * t;
            y += dy * t;
        }
    }

    dx = pt.x() - x;
    dy = pt.y() - y;

    return dx * dx + dy * dy;
}

bool pointsAreClose(Point pt1, Point pt2, Distance distSqrd) { return std::pow(pt1.distance(pt2)(), 2) <= distSqrd(); }

uint32_t cantorPair(uint32_t a, uint32_t b) { return (a + b) * (a + b + 1) / 2 + a; }

std::pair<Point, double> nearestPointOnSegment(const Point& a, const Point& b, const Point& p) {
    double t = (b - a).dot(p - a) / (b - a).dot(b - a);

    if (t < 0) {
        return std::make_pair(a, p.distance(a)());
    }
    else if (t > 1) {
        return std::make_pair(b, p.distance(b)());
    }
    else {
        Point nearest = a * (1.0 - t) + b * t;
        return std::make_pair(nearest, p.distance(nearest)());
    }
}

QVector<Point> chamferCorner(const Point& A, const Point& B, const Point& C, Distance chamferDistance) {
    Distance x_dist = A.x() - C.x();
    Distance y_dist = A.y() - C.y();
    Distance length = A.distance(C);

    Point B1(B.x() + (x_dist / length) * chamferDistance, B.y() + (y_dist / length) * chamferDistance);
    Point B2(B.x() - (x_dist / length) * chamferDistance, B.y() - (y_dist / length) * chamferDistance);

    return QVector<Point> {B1, B2};
}

// 90061.23 -> 25:01:01
// 1000.51  ->  0:16:41
// 59.45    --> 0:00:59
QString formattedTimeSpanHHMMSS(double sec) {
    int seconds = int(sec);
    int hours = (seconds / 60 / 60);
    int minutes = (seconds / 60) % 60;
    int remainingSeconds = seconds % 60;
    int milliseconds = round((sec - seconds) * 1000);
    return QString::number(hours) + ":" + QString::number(minutes).rightJustified(2, '0') + ":" +
           QString::number(remainingSeconds).rightJustified(2, '0') + "." +
           QString::number(milliseconds).leftJustified(3, '0');
}

QString formattedTimeSpanHHMMSS(Time time) {
    int seconds = int(time());
    return formattedTimeSpanHHMMSS(seconds);
}

// This function returns a string format "x hr y min z sec" of a time span in seconds
// 3660.4 -> 1 hr 1 min 0 sec
// 1000.5 -> 16 min 41 sec
// 43.23  -> 43 sec
QString formattedTimeSpan(double sec) {
    int seconds = int(sec);
    int hours = (seconds / 60 / 60);
    int minutes = (seconds / 60) % 60;
    int remainingSeconds = seconds % 60;
    if (hours > 0) {
        return QString::number(hours) + " hr " + QString::number(minutes) + " min " +
               QString::number(remainingSeconds) + " sec";
    }
    else if (minutes > 0) {
        return QString::number(minutes) + " min " + QString::number(remainingSeconds) + " sec";
    }
    else {
        return QString::number(remainingSeconds) + " sec";
    }
}

QString formattedTimeSpan(Time sec) {
    int seconds = int(sec());
    int hours = (seconds / 60 / 60);
    int minutes = (seconds / 60) % 60;
    int remainingSeconds = seconds % 60;
    if (hours > 0) {
        return QString::number(hours) + " hr " + QString::number(minutes) + " min " +
               QString::number(remainingSeconds) + " sec";
    }
    else if (minutes > 0) {
        return QString::number(minutes) + " min " + QString::number(remainingSeconds) + " sec";
    }
    else {
        return QString::number(remainingSeconds) + " sec";
    }
}

QQuaternion CreateQuaternion(double x, double y, double z, QuaternionOrder order) {
    double pitch = qDegreesToRadians(x);
    double yaw = qDegreesToRadians(y);
    double roll = qDegreesToRadians(z);

    QQuaternion result;
    QVector3D vx(1, 0, 0), vy(0, 1, 0), vz(0, 0, 1);
    QQuaternion qx, qy, qz, qt;
    qx = AxisAngleToQuat(vx, pitch);
    qy = AxisAngleToQuat(vy, yaw);
    qz = AxisAngleToQuat(vz, roll);
    if (order == QuaternionOrder::kXYZ) {
        qt = QuatMult(qx, qy);
        result = QuatMult(qt, qz);
    }
    else if (order == QuaternionOrder::kZYX) {
        qt = QuatMult(qz, qy);
        result = QuatMult(qt, qx);
    }

    return result;
}

QQuaternion CreateQuaternion(Angle x, Angle y, Angle z, QuaternionOrder order) {
    QQuaternion result;
    QVector3D vx(1, 0, 0), vy(0, 1, 0), vz(0, 0, 1);
    QQuaternion qx, qy, qz, qt;
    qx = AxisAngleToQuat(vx, x());
    qy = AxisAngleToQuat(vy, y());
    qz = AxisAngleToQuat(vz, z());
    if (order == QuaternionOrder::kXYZ) {
        qt = QuatMult(qx, qy);
        result = QuatMult(qt, qz);
    }
    else if (order == QuaternionOrder::kZYX) {
        qt = QuatMult(qz, qy);
        result = QuatMult(qt, qx);
    }

    return result;
}

QQuaternion AxisAngleToQuat(QVector3D axis, double angle) {
    QQuaternion result;
    axis.normalize();

    double sin_a = std::sin(angle / 2.0);
    double cos_a = std::cos(angle / 2.0);
    result.setX(axis.x() * sin_a);
    result.setY(axis.y() * sin_a);
    result.setZ(axis.z() * sin_a);
    result.setScalar(cos_a);

    return result;
}

QQuaternion QuatMult(QQuaternion qa, QQuaternion qb) {
    QQuaternion result;

    result.setScalar(qa.scalar() * qb.scalar() - QVector3D::dotProduct(qa.vector(), qb.vector()));
    QVector3D va = QVector3D::crossProduct(qa.vector(), qb.vector());

    QVector3D vb;
    vb.setX(qa.x() * qb.scalar());
    vb.setY(qa.y() * qb.scalar());
    vb.setZ(qa.z() * qb.scalar());

    QVector3D vc;
    vc.setX(qb.x() * qa.scalar());
    vc.setY(qb.y() * qa.scalar());
    vc.setZ(qb.z() * qa.scalar());

    va = va + vb;
    result.setVector(va + vc);
    result.normalize();

    return result;
}

QQuaternion CreateQuaternion(QVector3D vector_start, QVector3D vector_end) {
    QQuaternion result;
    vector_start.normalize();
    vector_end.normalize();

    QVector3D cross_product = QVector3D::crossProduct(vector_start, vector_end);

    // if cross_product is 0, then either the vectors are the same or they are exactly opposite
    // if they're the same, the zero-value result is acceptable
    // if they're opposite, create some non-zero quaternion to give 180 degree rotation. This quanternion is not unique
    if (qFuzzyCompare(cross_product.lengthSquared(), 0.0f) && vector_start != vector_end) {
        // hard code the axis-aligned cases for now
        if (vector_start == QVector3D(0, 0, 1) || vector_end == QVector3D(0, 0, 1)) // input vectors are z-axis aligned
        {
            cross_product = QVector3D(1, 0, 0);
        }
        else if (vector_start == QVector3D(0, 1, 0) ||
                 vector_end == QVector3D(0, 1, 0)) // input vectors are y-axis aligned
        {
            cross_product = QVector3D(1, 0, 0);
        }
        else if (vector_start == QVector3D(1, 0, 0) ||
                 vector_end == QVector3D(1, 0, 0)) // input vectors are x-axis aligned
        {
            cross_product = QVector3D(0, 1, 0);
        }
        else {
            // TODO: eventually solve the general case
        }
    }

    result.setVector(cross_product);

    // add 1 because vectors were normalized
    // would otherwise be vec_start.length * vec_end.length
    float scalar = 1 + QVector3D::dotProduct(vector_start, vector_end);
    result.setScalar(scalar);

    result.normalize();
    return result;
}

void EulerAngles(QQuaternion q, double* pitch, double* roll, double* yaw) {
    double sinr_cosp = 2 * (q.scalar() * q.x() + q.y() * q.z());
    double cosr_cosp = 1 - 2 * (q.x() * q.x() + q.y() * q.y());
    *pitch = std::atan2(sinr_cosp, cosr_cosp);

    double sinp = 2 * (q.scalar() * q.y() - q.z() * q.x());
    if (std::abs(sinp) >= 1)
        *roll = std::copysign(M_PI / 2, sinp); // use 90 degrees if out of range
    else
        *roll = std::asin(sinp);

    double siny_cosp = 2 * (q.scalar() * q.z() + q.x() * q.y());
    double cosy_cosp = 1 - 2 * (q.y() * q.y() + q.z() * q.z());
    *yaw = std::atan2(siny_cosp, cosy_cosp);
}

double findBinomialCoefficients(int n, int k) {
    double coefficient = 1;

    for (int i = n - k + 1; i <= n; i++)
        coefficient *= i;

    for (int i = 1; i <= k; i++)
        coefficient /= i;

    return coefficient;
}

std::tuple<QVector3D, QQuaternion, QVector3D> decomposeTransformMatrix(QMatrix4x4 mtrx) {
    // Extract translation
    QVector3D translation = QVector3D(mtrx(0, 3), mtrx(1, 3), mtrx(2, 3));

    // Zero out the translation components
    mtrx(0, 3) = 0.0f;
    mtrx(1, 3) = 0.0f;
    mtrx(2, 3) = 0.0f;

    // Extract scale
    float scale_x = mtrx.column(0).length();
    float scale_y = mtrx.column(1).length();
    float scale_z = mtrx.column(2).length();
    QVector3D scale = QVector3D(scale_x, scale_y, scale_z);

    // Divide out the scales
    mtrx.setColumn(0, mtrx.column(0) / scale_x);
    mtrx.setColumn(1, mtrx.column(1) / scale_y);
    mtrx.setColumn(2, mtrx.column(2) / scale_z);

    // What's left is the rotation
    QQuaternion rotation = QQuaternion::fromRotationMatrix(mtrx.toGenericMatrix<3, 3>());

    return std::make_tuple(translation, rotation, scale);
}

QMatrix4x4 composeTransformMatrix(QVector3D t, QQuaternion r, QVector3D s) {
    QMatrix4x4 m;
    m.setToIdentity();

    m.translate(t);
    m.rotate(r);
    m.scale(s);

    return m;
}

double clamp(double min, double val, double max) {
    if (val < min)
        return min;
    if (val > max)
        return max;

    return val;
}

double snap(double val, double interval) {
    double half_interval = interval / 2;
    if (val < 0) {
        half_interval = -half_interval;
    }

    return (((int)(val + half_interval)) / (int)interval) * interval;
}

QVector3D sphericalToCartesian(float rho, float theta, float phi) {
    QVector3D ret;

    // Convert to radians
    theta *= (pi() / 180);
    phi *= (pi() / 180);

    ret.setX(rho * std::cos(theta) * std::sin(phi));
    ret.setY(rho * std::sin(theta) * std::sin(phi));
    ret.setZ(rho * std::cos(phi));

    return ret;
}

bool glEquals(double a, double b) { return (std::abs(a - b) < 1e-5); }
} // namespace MathUtils
} // namespace ORNL
