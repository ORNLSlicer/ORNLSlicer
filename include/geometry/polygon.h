#pragma once

#include <cstdint>

#include <clipper.hpp>
#include <qcontainerfwd.h>
#include <qvectornd.h>

#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"

#ifdef HAVE_SINGLE_PATH
    #include <single_path/geometry/point.h>
    #include <single_path/geometry/polygon.h>
#endif

namespace ORNL {
class PolygonList;
class Polyline;

/*!
 * \class Polygon
 * \brief An individual polygon
 *
 * An individual polygon represented as a vector of points
 */
class Polygon : public QVector<Point> {
  public:
    // QVector constructors
    using QVector<Point>::QVector;

    //! \brief Constructor
    Polygon() = default;

    //! \brief Conversion Constructor
    Polygon(const QVector<Point>& path);

    //! \brief Conversion Constructor
    Polygon(const ClipperLib2::Path& path);

    //! \brief Conversion Constructor
    Polygon(const Path& path);

#ifdef HAVE_SINGLE_PATH
    //! \brief Conversion constructor
    Polygon(SinglePath::Polygon& polygon);

    //! \brief Conversion operator
    operator SinglePath::Polygon() const;
#endif

    //! \brief Checks the orientation of the Polygon
    //! \details Checks the orientation of the polygon based on the area.
    //! \return True if the polygon is counterclockwise, false if it is clockwise
    bool orientation() const;

    //! \brief Offsets the polygon by the given distance
    //! \details Offsets the polygon by moving the vertices along their angle bisectors.
    //! \param distance: the distance to offset the polygon
    //! \param joinType: the type of join to use
    //! \return the offset polygon
    PolygonList offset(const Distance& distance, const ClipperLib2::JoinType& joinType = ClipperLib2::jtMiter) const;

    //! \brief The length of each of sides of the polygon summed
    int64_t polygonLength() const;

    //! \brief Returns the center point of the polygon's bounding rect
    Point boundingRectCenter() const;

    //! \brief Smallest x, y, and z coordinates (may not be from the same
    //! point)
    Point min() const;

    //! \brief Largest x, y, and z coordinates (may not be from the same
    //! point)
    Point max() const;

    //! \brief Rotate the polygon around a specified point
    Polygon rotateAround(const Point& center, const Angle& angle, const QVector3D& axis = {0, 0, 1}) const;

    //! \brief Restores point normals to geometry after it has been modified
    //! \param all_polys: List of Polygons to retrieve normals from
    //! \param offset: Whether normals are being restored after an offset operation.
    //! If true: normals will be restored from the closest point found within all_polys.
    //! If false: normals will be restored from exact matching point found within all_polys.
    //! If no exact match can be found, the bisecting normal will be computed and assigned.
    void restoreNormals(QVector<Polygon> all_polys, bool offset = false);

    //! \brief Reverses the direction of normals for the points of this polygon
    Polygon reverseNormalDirections();

    //! \brief shifts every point in the polygon by the vector
    //! \param vector of direction/length to shift
    Polygon translate(const QVector3D& shift);

    //! \brief Returns whether the point is inside the polygon
    bool inside(const Point& point, const bool& border_result = false) const;

    //! \brief tests two polygons to see if they overlap
    //! \param p: the second polygon
    //! \return if the polygons overlap
    bool overlaps(const Polygon& p);

    //! \brief Computes the area of the polygon
    //! \details Computes the area of the polygon by summing the area of the
    //! triangles formed by the polygon's vertices and the origin. Outer
    //! polygons have a positive area, while hole polygons have a negative
    //! area.
    //! \return the area of the polygon
    Area area() const;

    //! \brief Returns the closest point to `rhs`
    Point closestPointTo(const Point& rhs) const;

    //! \brief Remove all mid collinear points
    Polygon simplify(const Angle& tolerance);

    //! \brief Converts polygon to ClipperLib2 Path
    //! \return ClipperLib2 Path that represents the underlying polygon
    ClipperLib2::Path getPath();

    //! \brief Converts polygon to polyline
    //! \return Polyline that represents the underlying polygon
    Polyline toPolyline();

    /**
     * @brief Returns the edges of the polygon as a vector of polylines.
     * @return Vector of Polylines representing the edges of the polygon.
     */
    QVector<Polyline> getEdges() const;

    /*!
     * \brief Add an instance of Polygon to this Polygon to make Polygons.
     *        Any overlapping Polygon are merged into one.
     */
    PolygonList operator+(const PolygonList& rhs);

    /*!
     * \brief Add an instance of Polygon to this Polygon to make Polygons.
     *        Any overlapping Polygon are merged into one.
     */
    PolygonList operator+(const Polygon& rhs);

    //! \brief Subtracts an instance of Polygon from this Polygon to make
    //! Polygons.
    PolygonList operator-(const PolygonList& rhs);

    //! \brief Subtracts an instance of Polygon from this Polygon to make
    //! Polygons.
    PolygonList operator-(const Polygon& rhs);

    /*!
     * \brief Unions an instance of Polygon to this Polygon to make
     * Polygons. Any overlapping Polygon are merged into one.
     */
    PolygonList operator|(const PolygonList& rhs);

    /*!
     * \brief Union an instance of Polygon to this Polygon to make Polygons.
     *        Any overlapping Polygon are merged into one.
     */
    PolygonList operator|(const Polygon& rhs);

    /*!
     * \brief Returns the intersection of this Polygon and other Polygons
     *        as 0 or more Polygon's
     */
    PolygonList operator&(const PolygonList& rhs);

    /*!
     * \brief Returns the intersection of this Polygon and other Polygons
     *        as 0 or more Polygons
     */
    PolygonList operator&(const Polygon& rhs);

    /*!
     * \brief Returns the intersection of this polygon and a polygon as
     *        0 or more polylines
     */
    QVector<Polyline> operator&(const Polyline& rhs);

    /*!
     * \brief Returns the xor of this Polygon and other Polygons
     *        as 0 or more Polygons
     */
    PolygonList operator^(const PolygonList& rhs);

    /*!
     * \brief Returns the xor of this Polygon and other Polygons
     *        as 0 or more Polygons
     */
    PolygonList operator^(const Polygon& rhs);

    //! \brief Checks if two polygons are identical
    bool operator==(const Polygon& rhs) const;

    // friend classes can use each other's private functions/members
    friend class PolygonList;
    friend class Polyline;

    //! \brief operator for use with ClipperLib2
    ClipperLib2::Path operator()() const;

}; // Class Polygon
} // namespace ORNL
