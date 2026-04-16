#pragma once

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "geometry/point.h"
#include "geometry/segments/bezier.h"

namespace ORNL {
/*!
 *  \class BSpline
 *  \brief Geometry type for BSplines.
 */
class BSpline {
  public:
    //! \brief Constructor
    //! \param start the first point on the spline
    explicit BSpline(const Point& start);

    //! \brief adds a knot to the spline and updates control points
    //! \param knot the knot point to add
    void append(const Point& knot);

    //! \brief closes the spline to become a closed loop
    void close();

    //! \brief gets a point at a certain time along the spline
    //! \note this is NOT a constant speed spline
    //! \param t the parametrized time value
    //! \return a point along the spline
    Point samplePoint(double t);

    //! \brief converts the spline into individual bezier segments
    //! \return a list of bezier segments that comprise this spline
    QVector<QSharedPointer<BezierSegment>> toBezierSegments();

  private:
    //! \brief knots and controls that define curves
    QVector<Point> m_control_points;
    QVector<Point> m_knot_points;

    //! \brief if this spline is a closed loop
    bool m_is_closed = false;
};
} // namespace ORNL
