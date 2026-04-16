#pragma once

#include <vector>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qquaternion.h>
#include <qsharedpointer.h>

#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "units/unit.h"

namespace ORNL {
//! \class BezierSegment
//! \brief A segment type for a cubic bezier curve
class BezierSegment : public SegmentBase {
  public:
    //! \brief Default Constructor
    BezierSegment();

    //! \brief Constructor
    //! \param start the start point of this segment
    //! \param control_a the first control point
    //! \param control_b the second control point
    //! \param end the end point
    BezierSegment(const Point& start, const Point& control_a, const Point& control_b, const Point& end);

    //! \brief Populates the passed OpenGL buffers with float data for a spline.
    //! \param vertices: OpenGL vertex array to append to.
    //! \param normals: OpenGL normal array to append to.
    //! \param colors: OpenGL color array to append to.
    void createGraphic(std::vector<float>& vertices, std::vector<float>& normals, std::vector<float>& colors) override;

    //! \brief samples a point along this curve for the parametric value t
    //! \param t parametric value on the interval 0 to 1
    //! \return a point on this curve
    Point getPointAlong(double t);

    //! \brief determine the length of this curve
    //! \note this is an approximation due to the difficulty of computing the exact curve length
    //! \return the approximate length
    Distance length() override;

    //! \brief determines the min z-value for this segment
    //! \return the min z value
    float getMinZ() override;

    //! \brief Write the gcode for a curve
    //! \param writer the writer to use
    //! \return the gcode to save to file
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Clone
    //! \return a pointer to a copy of this segment
    QSharedPointer<SegmentBase> clone() const override;

    //! \brief a static method that fits and returns a bezier curve by optimizing the smoothing factor
    //! \param start_index the start point in the parent's path
    //! \param middle_index the middle point to fit by in the parent's path
    //! \param end_index the end point in the parent's path
    //! \param path the path segments to fit to
    //! \return the best fit bezier curve and its error
    static QPair<BezierSegment, Distance> Fit(int start_index, int end_index, Path& path);

    //! \brief fits a set of three points to a curve using a smoothing factor and returns control points
    //! \note all three points are guaranteed to be on the curve
    //! \param a the first point
    //! \param b the middle point
    //! \param c the end point
    //! \param smoothing the factor to use when determining the control points
    //! \return a pair of control points
    static QPair<Point, Point> ComputeControlPoints(Point& a, Point& b, Point& c, double smoothing);

    //! \brief sets the first control point
    //! \param control the new value
    void setControlA(Point& control);

    //! \brief sets the second control point
    //! \param control the new value
    void setControlB(Point& control);

    //! \brief Override for rotate.  Must rotate control points in addition to start/end
    //! \param rotation: quant to rotate by
    virtual void rotate(QQuaternion rotation) override;

    //! \brief Override for shift.  Must shift control points in addition to start/end
    //! \param shift: amount to shift by
    virtual void shift(Point shift) override;

  private:
    //! \brief Control points used to draw the curve
    Point m_control_a;
    Point m_control_b;
};
} // namespace ORNL
