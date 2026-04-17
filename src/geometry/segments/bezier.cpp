#include "geometry/segments/bezier.h"

#include <vector>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qmath.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "geometry/b_spline.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "graphics/support/shape_factory.h"
#include "units/unit.h"

namespace ORNL {
BezierSegment::BezierSegment() : SegmentBase(Point(0, 0, 0), Point(0, 0, 0)) {}

BezierSegment::BezierSegment(const Point& start, const Point& control_a, const Point& control_b, const Point& end)
    : SegmentBase(start, end), m_control_a(control_a), m_control_b(control_b) {}

void BezierSegment::createGraphic(std::vector<float>& vertices, std::vector<float>& normals,
                                  std::vector<float>& colors) {
    ShapeFactory::createSplineCylinder(m_display_width, m_start, m_control_a, m_control_b, m_end, m_color, vertices,
                                       colors, normals);
}

Point BezierSegment::getPointAlong(double t) {
    m_control_a.z(m_start.z());
    m_control_b.z(m_start.z());
    return m_start * qPow(1.0 - t, 3) + m_control_a * 3 * t * qPow(1.0 - t, 2) +
           m_control_b * 3 * qPow(t, 2) * (1.0 - t) + m_end * qPow(t, 3);
}

Distance BezierSegment::length() {
    const int approx_segments = 1000;
    Distance length = 0.0;
    Point last = m_start;
    Point next;

    for (double increment = (1.0 / approx_segments), t = increment; t <= 1.0; t += increment) {

        next = this->getPointAlong(t);
        length += last.distance(next);
        last = next;
    }
    return length;
}

float BezierSegment::getMinZ() {
    float min = m_start.z();

    if (min < m_control_a.z())
        min = m_control_a.z();
    if (min < m_control_b.z())
        min = m_control_b.z();
    if (min < m_end.z())
        min = m_end.z();

    return min;
}

QString BezierSegment::writeGCode(QSharedPointer<WriterBase> writer) {
    // return writer->writeSpline(m_start, m_control_a, m_control_b, m_end, this->getSb());
    return {};
}

QSharedPointer<SegmentBase> BezierSegment::clone() const { return QSharedPointer<BezierSegment>::create(*this); }

QPair<BezierSegment, Distance> BezierSegment::Fit(int start_index, int end_index, Path& path) {
    Point p1 = path[start_index]->start();
    Point p4 = path[end_index]->end();

    // Compute the ground truth
    BSpline spline(p1);
    for (int i = start_index; i <= end_index; ++i)
        spline.append(path[i]->end());

    // Determine control points for new curve
    double t0 = 0.49;
    double t1 = 0.51;

    auto p_t0 = spline.samplePoint(t0);
    auto p_t1 = spline.samplePoint(t1);

    double t01 = qPow(1 - t0, 3);
    double t11 = qPow(1 - t1, 3);

    double t02 = 3 * qPow(1 - t0, 2) * t0;
    double t12 = 3 * qPow(1 - t1, 2) * t1;

    double t03 = 3 * (1 - t0) * qPow(t0, 2);
    double t13 = 3 * (1 - t1) * qPow(t1, 2);

    double t04 = qPow(t0, 3);
    double t14 = qPow(t1, 3);

    Point numer_part = p_t1 - (p1 * t11) - (p4 * t14);

    Point p3_numerator = (p_t0 - (p1 * t01) - (p4 * t04)) - (numer_part * (t02 / t12));
    Point p3 = p3_numerator / (t03 - ((t02 * t13) / t12));

    Point p2 = (numer_part - (p3 * t13)) / t12;

    // Build new bezier from computed values
    BezierSegment bezier(p1, p2, p3, p4);

    // Compute error
    Distance total_error = 0.0;
    const int resolution = 100;

    for (int i = 0; i <= resolution; ++i) {
        double t = double(i) / double(resolution);

        total_error += bezier.getPointAlong(t).distance(spline.samplePoint(t));
    }

    return QPair<BezierSegment, Distance>(bezier, total_error);
}

QPair<Point, Point> BezierSegment::ComputeControlPoints(Point& a, Point& b, Point& c, double smoothing) {
    Distance d_ab = a.distance(b);
    Distance d_bc = b.distance(c);

    double first_scale = smoothing * (d_ab() / (d_ab + d_bc)());
    double second_scale = smoothing * (d_bc() / (d_ab + d_bc)());

    Point first_control((b.x() - first_scale * (c.x() - a.x())), (b.y() - first_scale * (c.y() - a.y())));

    Point second_control((b.x() + second_scale * (c.x() - a.x())), (b.y() + second_scale * (c.y() - a.y())));

    return QPair<Point, Point>(first_control, second_control);
}

void BezierSegment::setControlA(Point& control) { m_control_a = control; }

void BezierSegment::setControlB(Point& control) { m_control_b = control; }

void BezierSegment::rotate(QQuaternion rotation) {
    // rotate each point
    QVector3D control_a_vec = m_control_a.toQVector3D();
    QVector3D result_control_a = rotation.rotatedVector(control_a_vec);
    m_control_a = Point(result_control_a);

    QVector3D control_b_vec = m_control_b.toQVector3D();
    QVector3D result_control_b = rotation.rotatedVector(control_b_vec);
    m_control_b = Point(result_control_b);

    SegmentBase::rotate(rotation);
}

void BezierSegment::shift(Point shift) {
    m_control_a = m_control_a + shift;
    m_control_b = m_control_b + shift;

    SegmentBase::shift(shift);
}
} // namespace ORNL
