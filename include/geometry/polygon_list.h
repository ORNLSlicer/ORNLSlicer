#pragma once

#include <cstdint>

#include <QVector>
#include <clipper.hpp>
#include <qcontainerfwd.h>
#include <qpolygon.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/polygon.h"
#include "geometry/polyline.h"
#include "units/unit.h"

// Single Path Lib
#ifdef HAVE_SINGLE_PATH
    #include <single_path/geometry/polygon_list.h>
#endif

namespace ORNL {
class Point;

const static int clipper_init = (0);
#define NO_INDEX (std::numeric_limits<uint>::max())

/*!
 * \class PolygonList
 * \brief A class that contains multiple individual Polygons.
 *
 * The class contains multiple individual Polygon and various
 * operations (including binary, e.g. union, intersection, difference, xor).
 * Based in part on [CuraEngine version by
 * Ultimaker](https://github.com/Ultimaker/CuraEngine/blob/master/src/utils/polygon.h)
 */
class PolygonList : public QVector<Polygon> {
  public:
    // QVector constructors.
    using QVector<Polygon>::QVector;

    PolygonList();

    PolygonList(QVector<QVector<QPair<double, double>>> raw_poly_list);
    /*!
     * \brief getRawPoints - returns points of all polygons in list as raw values
     * \return Vector of polygons comprised of pairs (x, y)
     */
    QVector<QVector<QPair<double, double>>> getRawPoints();

    //! The total number of points in all the individual polygons
    uint pointCount() const;

    /*!
     * \brief Offsets each of the polygons.
     * Offsets each of the Polygon by moving the vertices along their angle
     * bisectors.
     * Individual Polygon's can merge if they overlap or seperate into
     * multiple Polygon's.
     * \param distance: distance to offset the polygons by
     * \param real_offset: offset distance to be applied to original_geometry
     * for lost geometry calculation
     */
    PolygonList offset(Distance distance, Distance real_offset = 0,
                       ClipperLib2::JoinType joinType = ClipperLib2::jtMiter) const;

    /*!
     * \brief checks if the point is inside of the polygon
     *
     * We do this by counting the number of polygons inside which this point
     * lies. An odd number is inside, while an even number is outside.
     *
     * Returns false if outside, true if inside; if the point lies exactly
     * on the border, will return \p border_result.
     */
    bool inside(Point p, bool border_result = false) const;

    /*!
     * \brief Implements monotone chain convex hull algorithm
     *
     * See <a
     * href="https://en.wikibooks.org/wiki/Algorithm_Implementation/Geometry/Convex_hull/Monotone_chain">Monotone
     * Chain</a>
     */
    Polygon convexHull() const;

    /*!
     * \brief Smooth out small perpendicular segments
     *
     * Smoothing is performed by removing the inner most vertex of a line
     * segment smaller than \p remove_length which has an angle with the
     * next and previous line segment smaller than roughly 150*
     *
     * Note that in its current implementation this function doesn't remove
     * line segments with an angle smaller than 30* Such would be the case
     * for an N shape.
     */
    PolygonList smooth(const Distance& remove_length);

    /*!
     * \brief removes all middle collinear points judged by the specified tolerance angle
     * tolerance is defaulted to 0.01 degree
     */
    PolygonList simplify(const Angle tolerance = 0.0001745329);

    /*!
     * \brief Removes vertices:
     * that join co-linear edges, or join edges that are almost co-linear
     * that are within the specified distance of an adjacent vertex
     * that are within the specified distance of a semi-adjacent vertex together with their out-lying vertices
     */
    PolygonList cleanPolygons(const Distance distance = 10);

    /*!
     * Remove all but the polygons on the very outside.
     * Exclude holes and parts within holes.
     * \return the resulting polygons.
     */
    PolygonList getOutsidePolygons() const;

    /*!
     * Exclude holes which have no parts inside of them.
     * \return the resulting polygons.
     */
    PolygonList removeEmptyHoles() const;

    /*!
     * Return hole polygons which have no parts inside of them.
     * \return the resulting polygons.
     */
    PolygonList getEmptyHoles() const;

    // Jingyang-optimizer
    /*!
     * \brief Returns the sum of parimeters of polygons
     */
    int64_t totalLength();

    // Nitish-support
    //! \brief Calculates the area of a the boundary of the polygonList
    Area outerArea();

    //! \brief Calcultes the area of a PolygonList accounting for holes
    Area netArea();

    PolygonList shift(Point shift);

    // intersection over union of two polygonlist
    float commonArea(PolygonList cur_layer_outline);

    // Peform union by even-odd on all polygons at once.
    void addAll(QVector<Polygon> polygons);

    // Convert to QPolygon
    QVector<QPolygon> toQPolygons() const;

    // Get the bounding box for all polygons in list list.
    QRect boundingRect() const;

    /**
     * @brief Returns the edges of the polygons in the list.
     * @return QVector of Polyline representing the edges of the polygons.
     */
    QVector<Polyline> getEdges() const;

  private:
    /*!
     * recursive part of \ref Polygons::removeEmptyHoles and \ref
     * Polygons::getEmptyHoles
     * \param node The node of the polygons part to process
     * \param remove_holes Whether to remove empty holes or everything but
     * the empty holes \param ret Where to store polygons which are not
     * empty holes
     */
    void removeEmptyHoles_processPolyTreeNode(const ClipperLib2::PolyNode& node, const bool remove_holes,
                                              PolygonList& ret) const;

  public:
    /*!
     * Split up the polygons into groups according to the even-odd rule.
     * Each PolygonsPart in the result has an outline as first polygon,
     * whereas the rest are holes.
     */
    QVector<PolygonList> splitIntoParts(bool unionAll = false) const;

    //! \brief Restores point normals to geometry after it has been modified
    //! \param all_polys: List of Polygons to retrieve normals from
    //! \param offset: Whether normals are being restored after an offset operation.
    //! If true: normals will be restored from the closest point found within all_polys.
    //! If false: normals will be restored from exact matching point found within all_polys.
    //! If no exact match can be found, the bisecting normal will be computed and assigned.
    void restoreNormals(QVector<Polygon> all_polys, bool offset = false);

    //! \brief Reverses the direction of normals for the points of this polygon list
    PolygonList reverseNormalDirections();

    //! \brief Returns the closest point to `rhs`
    //! \param rhs: the point in question
    //! \return The closest point
    Point closestPointTo(const Point& rhs);

  private:
    void splitIntoParts_processPolyTreeNode(ClipperLib2::PolyNode* node, QVector<PolygonList>& ret) const;

  public:
    //! \brief Removes polygons with area smaller than \p minAreaSize.
    PolygonList removeSmallAreas(Area minAreaSize);

    //! \brief Removes overlapping consecutive line segments which don't
    //! delimit a positive area.
    PolygonList removeDegenerateVertices();

    //! \brief Returns a point containing the minimum x, y, and z values
    Point min() const;

    //! \brief Returns a point containing the maximum x, y, and z values
    Point max() const;

    //! \brief Returns a point at the center of the polygon list
    Point boundingRectCenter() const;

    //! \brief Rotates around the origin with the specific axis
    PolygonList rotate(const Angle& angle, const QVector3D& axis = {0, 0, 1});

    //! \brief Rotates around a specified point
    PolygonList rotateAround(const Point& center, const Angle& angle, const QVector3D& axis = {0, 0, 1});

    //! \brief computes total, signed area of all polygons in the list
    Area totalArea();

    //! \brief Returns whether the two Polygons objects are equal as
    //! determined by each of the individual Polygon objects
    bool operator==(const PolygonList& other) const;

    //! \brief Returns whether the two Polygons objects are equal as
    //! determined by each of the individual Polygon objects
    bool operator!=(const PolygonList& other) const;

    //! \brief Equivalent to | (union)
    PolygonList operator+(const PolygonList& rhs);

    //! \brief Equivalent to | (union)
    PolygonList operator+(const Polygon& rhs);

    //! \brief Equivalent to |= (union equals)
    PolygonList operator+=(const PolygonList& rhs);

    //! \brief Equivalent to |= (union equals)
    PolygonList operator+=(const Polygon& rhs);

    //! \brief difference
    PolygonList operator-(const PolygonList& rhs);

    //! \brief difference
    PolygonList operator-(const Polygon& rhs);

    //! \brief difference
    PolygonList operator-=(const PolygonList& rhs);

    //! \brief difference
    PolygonList operator-=(const Polygon& rhs);

    //! \brief Equivalent to | (union)
    PolygonList operator<<(const PolygonList& rhs);

    //! \brief Equivalent to | (union)
    PolygonList operator<<(const Polygon& rhs);

    //! \brief union
    PolygonList operator|(const PolygonList& rhs);

    //! \brief union
    PolygonList operator|(const Polygon& rhs);

    //! \brief union equals
    PolygonList operator|=(const PolygonList& rhs);

    //! \brief union equals
    PolygonList operator|=(const Polygon& rhs);

    //! \brief intersection
    PolygonList operator&(const PolygonList& rhs);

    //! \brief intersection
    PolygonList operator&(const Polygon& rhs);

    //! \brief intersection
    QVector<Polyline> operator&(const Polyline& rhs);

    //! \brief intersection equals
    PolygonList operator&=(const PolygonList& rhs);

    //! \brief intersection equals
    PolygonList operator&=(const Polygon& rhs);

    //! \brief xor
    PolygonList operator^(const PolygonList& rhs);

    //! \brief xor
    PolygonList operator^(const Polygon& rhs);

    //! \brief xor equals
    PolygonList operator^=(const PolygonList& rhs);

    //! \brief xor equals
    PolygonList operator^=(const Polygon& rhs);

#ifdef HAVE_SINGLE_PATH
    //! \brief Conversion constructor
    PolygonList(SinglePath::PolygonList& poly_list);

    //! \brief Conversion operator
    operator SinglePath::PolygonList() const;
#endif

    friend class Polygon;
    friend class Polyline;

    //! \brief Tracks geometry that is lost as a result of inward offsetting
    QVector<Polygon> lost_geometry;

    //! \brief Returns distance from between point and edge of island
    float distanceTo(Point point);

  protected:
    //! \brief Private operator for use with internal clipper functions
    ClipperLib2::Paths operator()() const;

  private:
    //! \brief Private constructor for use with internal clipper functions
    PolygonList(const ClipperLib2::Paths& paths);

    // Replaces the current polygons with whatever is in paths. Used with
    // internal clipper functions
    void clipperLoad(const ClipperLib2::Paths& paths);

    /*!
     * \brief Add an instance of Polygon with this this one.
     *        Any overlapping Polygon are merged into one.
     *        The result is returned.
     */
    PolygonList _add(const Polygon& rhs);

    /*!
     * \brief Merge another instance of Polygons with this one.
     *        Any overlapping Polygon are merged into one.
     *        The result is returned.
     */
    PolygonList _add(const PolygonList& rhs);

    /*!
     * \brief Add an instance of Polygon into this Polygons.
     *        Any overlapping Polygon are merged into one.
     */
    PolygonList _add_to_this(const Polygon& rhs);

    /*!
     * \brief Merge another instance of Polygons into this Polygons.
     *        Any overlapping Polygon are merged into one.
     */
    PolygonList _add_to_this(const PolygonList& rhs);

    /*!
     * \brief Remove any regions of this that overlap with other
     *        The result is returned.
     */
    PolygonList _subtract(const Polygon& rhs);

    /*!
     * \brief Remove any regions of the polygon that overlap with other's
     * Polygons The result is returned.
     */
    PolygonList _subtract(const PolygonList& rhs);

    /*!
     * \brief Remove any regions of this that overlap with other
     */
    PolygonList _subtract_from_this(const Polygon& rhs);

    /*!
     * \brief Remove any regions of this polygon that overlap with other's
     * Polygons
     */
    PolygonList _subtract_from_this(const PolygonList& rhs);

    //! \brief Remove any region of the Polygons that do not overlap (Result
    //! is returned)
    PolygonList _intersect(const Polygon& rhs);

    //! \brief Remove any region of the Polygons that do not overlap (Result
    //! is returned)
    PolygonList _intersect(const PolygonList& rhs);

    //! \brief Remove any part of the Polyline that does not overlap
    QVector<Polyline> _intersect(const Polyline& rhs);

    //! \brief Remove any region of this Polygons that do not overlap
    PolygonList _intersect_with_this(const Polygon& rhs);

    //! \brief Remove any region of this Polygons that do not overlap
    PolygonList _intersect_with_this(const PolygonList& rhs);

    //! \brief Remove any region of the Polygons that overlaps (Result is
    //! returned)
    PolygonList _xor(const PolygonList& rhs);

    //! \brief Remove any region of the Polygons that overlaps (Result is
    //! returned)
    PolygonList _xor(const Polygon& rhs);

    //! \brief Remove any region of the Polygons that overlaps
    PolygonList _xor_with_this(const PolygonList& rhs);

    //! \brief Remove any region of the Polygons that overlaps
    PolygonList _xor_with_this(const Polygon& rhs);

    // Make all functions that add an element private so that they funnel
    // through _add/_union
    using QVector<Polygon>::append;
    using QVector<Polygon>::prepend;
    using QVector<Polygon>::push_back;
    using QVector<Polygon>::push_front;
    using QVector<Polygon>::insert;

}; // class PolygonList

} // namespace ORNL
