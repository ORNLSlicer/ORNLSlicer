#include "geometry/b_spline.h"

#include <cassert>
#include <cmath>

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "geometry/point.h"
#include "geometry/segments/bezier.h"
#include "utilities/mathutils.h"

namespace ORNL {
BSpline::BSpline(const Point& start) { m_knot_points.append(start); }

void BSpline::append(const Point& knot) {
    assert(!m_is_closed && "Cannot add point to closed BSpline!");

    if (knot == m_knot_points.first()) // This point will close the BSpline
        close();
    else {
        m_knot_points.append(knot);

        if (m_knot_points.size() >= 3) // Compute new control points
        {
            Point& a = m_knot_points[m_knot_points.size() - 3];
            Point& b = m_knot_points[m_knot_points.size() - 2];
            Point& c = m_knot_points[m_knot_points.size() - 1];

            double t = 0.25;
            auto new_control_points = BezierSegment::ComputeControlPoints(a, b, c, t);

            m_control_points.append(new_control_points.first);
            m_control_points.append(new_control_points.second);
        }
    }
}

void BSpline::close() {
    if (!m_is_closed) {
        double t = 0.25;

        // Compute a new set control points from last two segments
        auto control_points = BezierSegment::ComputeControlPoints(
            m_knot_points[m_knot_points.size() - 2], m_knot_points[m_knot_points.size() - 1], m_knot_points[0], t);
        m_control_points.append(control_points.first);
        m_control_points.append(control_points.second);

        // Also need to compute extra control points for the last and first segments
        control_points = BezierSegment::ComputeControlPoints(m_knot_points[m_knot_points.size() - 1], m_knot_points[0],
                                                             m_knot_points[1], t);
        m_control_points.append(control_points.first);
        m_control_points.prepend(control_points.second); // Prepend this point since it belongs to the first BSpline

        m_is_closed = true;
    }
}

Point BSpline::samplePoint(double time) {
    // Use the entire BSpline a a single n-order bezier curve

    double x = 0;
    double y = 0;
    double z = 0;

    QVector<Point> total_points = m_control_points;
    total_points.prepend(m_control_points.front());
    total_points.append(m_control_points.back());

    int order = total_points.size() - 1;

    //! \brief This calculates the point on the n-th order bezier curve at a given time value: SUM( nCi * (1-t)^(3-i) *
    //! t^i * Point(x/y/z) ) \note T is on the interval 0 to 1 \note The order is the number of points minus 1
    for (int point_index = 0; point_index <= order; point_index++) {
        x += MathUtils::findBinomialCoefficients(order, point_index) * powf((1.0 - time), (order - point_index)) *
             powf(time, point_index) * total_points[point_index].x();
        y += MathUtils::findBinomialCoefficients(order, point_index) * powf((1.0 - time), (order - point_index)) *
             powf(time, point_index) * total_points[point_index].y();
        z += MathUtils::findBinomialCoefficients(order, point_index) * powf((1.0 - time), (order - point_index)) *
             powf(time, point_index) * total_points[point_index].z();
    }

    return Point(x, y, z);
}

QVector<QSharedPointer<BezierSegment>> BSpline::toBezierSegments() {
    QVector<QSharedPointer<BezierSegment>> segments;

    assert(m_knot_points.size() > 2);

    int end = m_knot_points.size();
    if (!m_is_closed) // If this is not closed, then we need to add extra control points on the ends to handle the
                      // boundary conditions
    {
        // Clone first and last control points
        m_control_points.prepend(m_control_points.first());
        m_control_points.append(m_control_points.last());
        end -= 1;
    }

    for (int i = 0; i < end; ++i) {
        Point start_point = m_knot_points[i];
        Point end_point = m_knot_points[(i + 1) % m_knot_points.size()];

        Point control_a = m_control_points[(i * 2)];
        Point control_b = m_control_points[(i * 2) + 1];

        segments.append(QSharedPointer<BezierSegment>::create(start_point, control_a, control_b, end_point));
    }

    return segments;
}
} // namespace ORNL
