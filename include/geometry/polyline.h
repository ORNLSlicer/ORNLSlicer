#pragma once

#include <vector>

#include <QVector3D>
#include <QVector>
#include <clipper.hpp>
#include <qcontainerfwd.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "units/unit.h"

namespace ORNL {
class Point;
class Polygon;
class PolygonList;

/*!
 * \class Polyline
 *
 * \brief List of line segments that form an open path
 */
class Polyline : public QVector<Point> {
  public:
    // QVector constructors.
    using QVector<Point>::QVector;

    /*!
     * \brief Constructor
     */
    Polyline() = default;

    /*!
     * \brief Conversion Constructor
     */
    Polyline(const QVector<Point>& path);

    /*!
     * \brief Conversion Constructor
     */
    Polyline(const QVector<ClipperLib2::IntPoint>& path);

    /*!
     * \brief Conversion Constructor
     */
    Polyline(const ClipperLib2::Path& path);

    /*!
     * \brief Conversion Constructor
     */
    Polyline(const std::vector<MeshTypes::Point_3>& cgal_polyline);

    /*!
     * \brief Conversion Constructor
     */
    Polyline(const QVector<QPair<double, double>>& pts);

    /*!
     * \brief Returns the length of the polyline
     */
    Distance length() const;

    /*!
     * \brief Concatenates this polyline with rhs, linking specified end point together
     * \param rhs
     * \param this_end_point    true: the first point; false: the last point
     * \param rhs_end_point     true: the first point; false: the last point
     * \returns The new concatenated polyline
     * \note Keeps the points' order of this polyline (will not reverse)
     */
    Polyline concatenate(Polyline rhs, bool this_end_point, bool rhs_end_point);

    //! \brief Returns the reversed polyline that makes the last point first, and the first point last.
    Polyline reverse();

    /*!
     * \brief Returns whether the polyline is shorter than \p check_length
     *
     * \param check_length The threshold length
     */
    bool shorterThan(Distance check_length) const;

    /*!
     * \brief Returns the closest point to \p rhs
     *
     * \param rhs The point
     * \returns The point on this polyline closest to \p rhs
     */
    Point closestPointTo(const Point& rhs) const;

    /*!
     * \brief Returns a simplified polyline
     * \param tol: simplification tolerance
     * \returns The simplified polyline
     */
    Polyline simplify(Distance tol);

    Polyline cleanPolygon(const Distance distance = 10);

    /*!
     * \brief Close this polyline into a polygon
     * \returns The closed polygon
     */
    Polygon close();

    /*!
     * \brief Converts a polyline to its real-world polygon representation
     * \param bead_width
     * \returns The real-world polygon representation
     */
    Polygon makeReal(const Distance& bead_width);

    /*!
     * \brief Rotates the polyline around the origin
     *
     * \param rotation_angle Angle to rotate
     * \param axis The axis to rotate around
     * \returns The rotated polyline
     */
    Polyline rotate(Angle rotation_angle, QVector3D axis = {0, 0, 1});

    /*!
     * \brief Rotates the polyline around a point
     *
     * \param center The point to rotate around
     * \param rotation_angle Angle to rotate
     * \param axis The axis to rotate around
     * \returns The rotated polyline
     */
    Polyline rotateAround(Point center, Angle rotation_angle, QVector3D axis = {0, 0, 1});

    /*!
     * \brief Returns whether the point is inside the polyline if the polyline is closed
     */
    bool inside(const Point& point, bool border_result = false) const;

    /*!
     * \brief Returns whether the polyline is clockwise or counter clockwise if the polyline is closed
     */
    bool orientation() const;

    /*!
     * \brief joins the two polylines together
     *
     * \param rhs The other polyline to join
     * \returns The two polylines joined together
     */
    Polyline operator+(Polyline rhs);

    /*!
     * \brief Creates a polyline that is the current one with \p rhs add on
     *
     * \param rhs The point to add to the polyline
     * \returns A polyline that is the point added to the current polyline
     */
    Polyline operator+(const Point& rhs);

    /*!
     * \brief Joins another polyline with this one
     *
     * \param rhs The other polyline to join
     * \returns A reference to this polyline
     */
    Polyline& operator+=(Polyline rhs);

    /*!
     * \brief Add \p rhs to this polyline
     * \param rhs Point to add
     * \return A reference to this polyline
     */
    Polyline& operator+=(const Point& rhs);

    /*!
     * \brief clips the polyline using the polygon
     *
     * \param rhs The polygon to clip this polyline with
     * \returns A list of polylines
     */
    QVector<Polyline> operator-(const Polygon& rhs);

    /*!
     * \brief clips the polyline using the list of polygons (holes are used
     * correctly)
     *
     * \param rhs The polygon list to clip this polyline with
     * \returns A list of polylines
     */
    QVector<Polyline> operator-(const PolygonList& rhs);

    /*!
     * \brief Returns the intersection of this polyline with another
     *        polyline as a list of points
     *
     * \note: when an entire line segment overlaps, still needs to be
     * determined...
     */
    QVector<Point> operator&(const Polyline& rhs);

    //! \brief Smallest x, y, and z coordinates (may not be from the same
    //! point)
    Point min() const;

    //! \brief Largest x, y, and z coordinates (may not be from the same
    //! point)
    Point max() const;

    friend class Polygon;
    friend class PolygonList;

  private:
    //! \brief Internal function used for Clipper to get a Clipper::Path
    //! from the current polyline
    ClipperLib2::Path operator()() const;
};
} // namespace ORNL
