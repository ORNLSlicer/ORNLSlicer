#pragma once

#include <cstdint>
#include <tuple>
#include <utility>

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qmatrix4x4.h>
#include <qquaternion.h>
#include <qvectornd.h>

#include "geometry/plane.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
//! \brief Commonly used math functions
namespace MathUtils {
/*!
 * \brief a function that compares two doubles
 *
 * \note Much better than a == b which can result
 *      in them being deemed unequal, even when they are logically
 * equivalent
 */
bool equals(double a, double b);

bool equals(double a, double b, double eps);

/*!
 * \brief Compares two doubles but with a higher epsilon for
 * \param a
 * \param b
 * \return
 */
bool glEquals(double a, double b);

/*!
 * \brief a function that compares two doubles
 *
 * \note Much better than a != b which can result
 *      in them being deemed unequal, even when they are logically
 * equivalent
 */
bool notEquals(double a, double b);

//! \brief Finds the center of a group of points
Point center(QVector<Point> points);

/*! \brief Calculates angle at origin between two points.  Given A, B(origin), and C
 *  \note this is unsigned, \see signedInternalAngle for signed calculation
 *  \param A: first point
 *  \param B: second point (origin)
 *  \param C: third point
 */
Angle internalAngle(Point A, Point B, Point C);

/*! \brief Calculates signed angle at origin between two points.  Given A, B(origin), and C
 *  \param A: first point
 *  \param B: second point (origin)
 *  \param C: third point
 */
Angle signedInternalAngle(Point A, Point B, Point C);

/*!
 * \brief angle from x-axis and two points
 * \param A first point
 * \param B second point
 * \return the angle
 */
double angleFromXAxis(Point A, Point B);

/*!
 * \brief Function that takes 4 points belonging to 2 lines and determines whether or not they intersect.
 * Lines sharing a point are classified as intersecting
 */
bool intersect(Point a1, Point a2, Point b1, Point b2);

/*!
 * \brief Determines if point q lies on line segment pr. Helper function for intersect
 */
bool onSegment(Point p, Point q, Point r);

//! \brief Determines the orientation of point C with respect to the line AB
//! from the perspective of being located at A and looking toward B
//! \param A: start point of line segment AB
//! \param B: end point of line segment AB
//! \param C: point for which the orientation is being determined
//! \return The orientation of C with respect to the line AB
//! 0 = C is colinear to line AB
//! -1 = C lies to the left of the line AB from the perspective outlined above (ABC is ccw)
//! 1 = C lies to the right of the line AB from the perspective outlined above (ABC is cw)
short orientation(Point A, Point B, Point C);

/*!
 * \brief Calculates intersection point for two, infinite lines defined by points A,B and C,D respectively
 * Function assumes lines have been previously tested for collinearity/parallel cases.  It will return (0, 0)
 * for those cases.  Unlike intersect function above, this intersection is for lines, not line segments.
 * \param A: first point for line 1
 * \param B: second point for line 1
 * \param C: first point for line 2
 * \param D: second point for line 2
 * \return point of intersection
 */
Point lineIntersection(Point A, Point B, Point C, Point D);

//! \brief Calculates the point of intersection between an infinite line and
//!        an infinite plane. Assumes that the line is not parallel to the
//!        plane (would be no intersection) and that the line is not contained
//!        within the plane (intersection would be entire line)
//! \param line_pt: point to define the line
//! \param line_direction: vector to define the line
//! \param plane
//! \return point of intersection
Point linePlaneIntersection(Point line_pt, QVector3D line_direction, Plane plane);

//! \brief Determines if two line segments which share a point (B) are near collinear
//! Uses law of cosines
bool nearCollinear(Point A, Point B, Point C, Angle threshold);

//! \brief Determines if two line segments which share a point (pt2) are near collinear
//! Uses distance threshold
bool slopesNearCollinear(Point pt1, Point pt2, Point pt3, Distance distSqrd);

//! \brief Returns the distance squared of a point (pt) from a line formed by points (ln1) & (ln2)
//! Helper function for slopesNearCollinear
Distance distanceFromLineSqrd(Point pt, Point ln1, Point ln2);

//! \brief Returns the distance squared of a point (pt) from a line segment formed by the points (ln1) & (ln2)
//! Helper function for poleOfInaccessibility
float distanceFromLineSegSqrd(Point pt, Point ln1, Point ln2);

//! \brief Determines if two points are close to one another based on distance threshold
bool pointsAreClose(Point pt1, Point pt2, Distance distSqrd);

/*!
 * \brief Function that maps two intergers to a unique result.
 * \note See: https://stackoverflow.com/questions/919612/
 */
uint32_t cantorPair(uint32_t a, uint32_t b);

/**
 * @brief Finds the nearest point on a segment to a given point and the distance to that point.
 * @param[in] a Start point of the segment.
 * @param[in] b End point of the segment.
 * @param[in] p Point to find the nearest point to.
 * @return A pair containing the nearest point on the segment and its distance to the point p.
 */
std::pair<Point, double> nearestPointOnSegment(const Point& a, const Point& b, const Point& p);

/*!
 * \brief chamferCorner
 * \param A - exterior point of the corner to be chamfered
 * \param B - interior point of the corner to be chamfered
 * \param C - exterior point of the corner to be chamfered
 * \returns The corner ABC chamfered returned as a vector of Points {B1,B2}
 */
QVector<Point> chamferCorner(const Point& A, const Point& B, const Point& C, Distance chamferDistance);

/*!
 * \brief Format time span to x hr y min z sec
 * \param seconds in Time unit
 */
QString formattedTimeSpan(Time seconds);

/*!
 * \brief Format time span in seconds to x hr y min z sec
 * \param seconds in double
 */
QString formattedTimeSpan(double seconds);

/*!
 * \brief Format time span in seconds to HH:MM:SS
 * \param seconds in double
 */
QString formattedTimeSpanHHMMSS(double seconds);

/*!
 * \brief Format time span to HH:MM:SS
 * \param seconds in Time unit
 */
QString formattedTimeSpanHHMMSS(Time time);

/*!
 * \brief creates quaternion from 3 angles
 * \param angle of rotation around x-axis in degrees
 * \param angle of rotation around y-axis in degrees
 * \param angle of rotation around z-axis in degrees
 * \param order: Order of axis application (XYZ by default)
 */
QQuaternion CreateQuaternion(double x, double y, double z, QuaternionOrder order = QuaternionOrder::kXYZ);

/*!
 * \brief creates quaternion from 3 angles
 * \param angle of rotation around x-axis
 * \param angle of rotation around y-axis
 * \param angle of rotation around z-axis
 * \param order: Order of axis application (XYZ by default)
 */
QQuaternion CreateQuaternion(Angle x, Angle y, Angle z, QuaternionOrder order = QuaternionOrder::kXYZ);

/*!
 * \brief creates quaternion dscribing the shortest
 *        rotation between 2 vectors
 * \param vector 1, beginning of rotation
 * \param vector 2, end of rotation
 */
QQuaternion CreateQuaternion(QVector3D vector_start, QVector3D vector_end);

/*!
 * \brief creates quaternion from 1 angle and Axis
 * \param vector defining the axis to be rotated around
 * \param angle of rotation in radians
 */
QQuaternion AxisAngleToQuat(QVector3D axis, double angle);

/*!
 * \brief multiplies 2 quaternions
 * \param quaternion to be multiplicand
 * \param quanternion to be multiplier
 */
QQuaternion QuatMult(QQuaternion qa, QQuaternion qb);

void EulerAngles(QQuaternion q, double* pitch, double* roll, double* yaw);

/*!
 * \brief finds the binomial coefficients for n and k according to the binomial theorem
 * \param first integer
 * \param second integer
 */
double findBinomialCoefficients(int n, int k);

/*!
 * \brief Create a transform matrix from the components.
 * \param t The translation vector.
 * \param r The rotation vector.
 * \param s The scale vector.
 * \return The transform matrix.
 */
QMatrix4x4 composeTransformMatrix(QVector3D t, QQuaternion r, QVector3D s);

/*!
 * \brief Given a 4x4 matrix, decomposes it into translation, rotation, and scale.
 * \param mtrx The matrix to decompose.
 * \return Translation, rotation, and scale - in that order.
 */
std::tuple<QVector3D, QQuaternion, QVector3D> decomposeTransformMatrix(QMatrix4x4 mtrx);

/*!
 * \brief Compares two doubles but with a higher epsilon for comparison. Useful for OpenGL comparisons.
 * \param a Value
 * \param b Value
 * \return If the doubles are considered 'equal'.
 */
bool glEquals(double a, double b);

/*!
 * \brief Snaps the passed value to the nearest interval value.
 * \param val Value to snap
 * \param interval The interval to snap
 * \return The snapped value.
 */
double snap(double val, double interval);

/*!
 * \brief Simple function to bring value between min and max.
 * \param min Min value
 * \param val Value to check
 * \param max Max value
 * \return Value if between min and max, min or max if outside of this range.
 */
double clamp(double min, double val, double max);

/*!
 * \brief Converts a set of spherical coordinates to cartesian coordinates. All angles are degrees.
 * \param rho Radial distance.
 * \param theta Azmuthal angle (angle from x axis).
 * \param phi Polar angle (angle from z axis).
 * \return Cartesian coordinates.
 */
QVector3D sphericalToCartesian(float rho, float theta, float phi);
} // namespace MathUtils
} // namespace ORNL
