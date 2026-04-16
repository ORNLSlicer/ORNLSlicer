#pragma once

#include <vector>

#include <qhashfunctions.h>
#include <qquaternion.h>
#include <qsharedpointer.h>

#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "units/unit.h"

namespace ORNL {
/*!
 *  \class ArcSegment
 *  \brief Segment type for arc movements.
 */
class ArcSegment : public SegmentBase {
  public:
    //! \brief Constructor
    ArcSegment(Point start, Point end, Point center, Angle angle, bool ccw);

    //! \brief fits an arc that starts and ends at two points, passing though or near to a middle point
    //! \note middle might not lay on the arc
    //! \pre start, middle and end cannot be co-linear
    //! \param start the start of the arc
    //! \param middle the middle point to target
    //! \param end the end point of the arc
    //! \param ccw if the arc is drawn counter-clockwise
    ArcSegment(Point start, Point middle, Point end);

    //! Constructor that automatically computes angle
    //! \param start the start point
    //! \param end the end point
    //! \param center the center point
    //! \param ccw the direction
    ArcSegment(Point start, Point end, Point center, bool ccw);

    //! \brief Populates the passed OpenGL buffers with float data for an arc.
    //! \param vertices: OpenGL vertex array to append to.
    //! \param normals: OpenGL normal array to append to.
    //! \param colors: OpenGL color array to append to.
    void createGraphic(std::vector<float>& vertices, std::vector<float>& normals, std::vector<float>& colors) override;

    //! \brief Clone
    QSharedPointer<SegmentBase> clone() const override;

    //! \brief Get the center of the arc.
    Point center() const;

    //! \brief Get the angle of the arc.
    Angle angle() const;

    //! \brief sets the angle of this segment
    //! \param angle the angle in radians
    void setAngle(const Angle& angle);

    //! \brief Get the orientation of the arc.
    bool counterclockwise() const;

    //! \brief Write the gcode for an arc.
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief returns minimum z-coordinate of the arc
    float getMinZ() override;

    //! \brief calculates the arc length of this segment
    //! \return the arc length
    Distance length() override;

    //! \brief calculates the center point of a circle from three points on the circle
    //! \param a first point
    //! \param b second point
    //! \param c third point
    //! \return the center point of the circle
    static Point CalculateCenter(const Point& a, const Point& b, const Point& c);

    //! \brief calculates the radius of a circle from three points on it
    //! \param a first point
    //! \param b second point
    //! \param c third point
    //! \return the radius of the circle
    static Distance Radius(const Point& a, const Point& b, const Point& c);

    //! \brief (1 / radius) of three points on a curve
    //! \param a first point
    //! \param b second point
    //! \param c third point
    //! \return the signed curvature
    static double SignedCurvature(const Point& a, const Point& b, const Point& c);

    //! \brief (1 / radius) of two segments on a curve
    //! \param first first segment
    //! \param second second segment
    //! \return the signed curvature
    static double SignedCurvature(QSharedPointer<SegmentBase> first, QSharedPointer<SegmentBase> second);

    //! \brief Override for rotate.  Must rotate center in addition to start/end
    //! \param rotation: quant to rotate by
    virtual void rotate(QQuaternion rotation) override;

    //! \brief Override for shift.  Must shift center in addition to start/end
    //! \param shift: amount to shift by
    virtual void shift(Point shift) override;

  private:
    //! \brief recalculates angles based on points
    void updateAngle();

    // Center
    Point m_center;
    // Angle
    Angle m_angle;
    // CCW
    bool m_ccw;
};
} // namespace ORNL
