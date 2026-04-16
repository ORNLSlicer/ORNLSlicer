#include "geometry/polygon.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include <clipper.hpp>
#include <qcontainerfwd.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/mathutils.h"

namespace ORNL {
Polygon::Polygon(const QVector<Point>& path) : QVector<Point>(path) {}

Polygon::Polygon(const ClipperLib2::Path& path) {
    reserve(path.size());
    for (const ClipperLib2::IntPoint& point : path) {
        append(point);
    }
}

Polygon::Polygon(const Path& path) {
    reserve(path.size());
    for (int i = 0; i < path.size(); i++) {
        append(path.at(i)->start());
    }
}

#ifdef HAVE_SINGLE_PATH
Polygon::Polygon(SinglePath::Polygon& polygon) {
    for (SinglePath::Point point : polygon) {
        append(point);
    }
}

ORNL::Polygon::operator SinglePath::Polygon() const {
    SinglePath::Polygon polygon;
    for (Point point : *this) {
        polygon.append(point);
    }
    return polygon;
}
#endif

bool Polygon::orientation() const { return ClipperLib2::Orientation((*this)()); }

PolygonList Polygon::offset(const Distance& distance, const ClipperLib2::JoinType& joinType) const {
    ClipperLib2::Paths paths;
    ClipperLib2::ClipperOffset clipper;
    clipper.AddPath(operator()(), joinType, ClipperLib2::etClosedPolygon);
    clipper.Execute(paths, distance());
    PolygonList polygons(paths);

    polygons.restoreNormals(QVector<Polygon> {*this}, true);

    return polygons;
}

int64_t Polygon::polygonLength() const {
    int64_t rv = 0;
    Point prev = operator[](size() - 1);
    for (int i = 0; i < size(); i++) {
        Point current = (*this)[i];
        rv += current.distance(prev)();
    }
    return rv;
}

Point Polygon::boundingRectCenter() const { return (this->min() + this->max()) / 2.0; }

Point Polygon::min() const {
    Point rv(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    for (const Point& point : *this) {
        if (point.x() < rv.x()) {
            rv.x(point.x());
        }

        if (point.y() < rv.y()) {
            rv.y(point.y());
        }

        if (point.z() < rv.z()) {
            rv.z(point.z());
        }
    }
    return rv;
}

Point Polygon::max() const {
    Point rv(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest());
    for (const Point& point : *this) {
        if (point.x() > rv.x()) {
            rv.x(point.x());
        }

        if (point.y() > rv.y()) {
            rv.y(point.y());
        }

        if (point.z() > rv.z()) {
            rv.z(point.z());
        }
    }
    return rv;
}

Polygon Polygon::rotateAround(const Point& center, const Angle& rotation_angle, const QVector3D& axis) const {
    Polygon polygon;
    polygon.reserve(this->size());

    for (Point point : *this) {
        polygon.append(point.rotateAround(center, rotation_angle, axis));
    }
    return polygon;
}

void Polygon::restoreNormals(QVector<Polygon> all_polys, bool offset) {
    if (offset) //! Offset operation: assign normals of closest point
    {
        for (Point& p1 : *this) {
            Distance min_dist = Distance(std::numeric_limits<float>::max());

            for (const Polygon& poly : all_polys) {
                Point p2 = poly.closestPointTo(p1);

                if (p1.distance(p2) < min_dist) {
                    min_dist = p1.distance(p2);
                    p1.setNormals(p2.getNormals());
                }
            }
        }
    }
    else //! Clipping operation: assign normals of exact point. If point can't be found, compute bisecting normal.
    {
        auto index = [](uint i, uint last) {
            uint ret = i;
            if (i < 0)
                ret = last;
            else if (i > last)
                ret = 0;

            return ret;
        };

        for (uint i = 0, size = this->size(); i < size; ++i) {
            bool found = false;
            for (Polygon& poly : all_polys) {
                for (Point& p : poly) {
                    if ((*this)[i] == p) {
                        (*this)[i].setNormals(p.getNormals());
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }

            if (!found) //! Compute bisecting normal
            {
                uint last = (*this).size() - 1;

                QVector3D unit_z {0, 0, 1};
                QVector3D prev = ((*this)[index(i - 1, last)] - (*this)[i]).toQVector3D().normalized();
                QVector3D next = ((*this)[index(i + 1, last)] - (*this)[i]).toQVector3D().normalized();

                QVector3D normal =
                    (QVector3D::crossProduct(unit_z, prev) + QVector3D::crossProduct(next, unit_z)).normalized();
                (*this)[i].setNormals(QVector<QVector3D> {normal, normal});
            }
        }
    }
}

Polygon Polygon::reverseNormalDirections() {
    for (Point& point : *this)
        point.reverseNormalDirections();

    return *this;
}

Polygon Polygon::translate(const QVector3D& shift) {
    Polygon shifted_polygon;
    shifted_polygon.reserve(size());
    for (const Point& point : *this) {
        shifted_polygon.append(point + shift.toPoint());
    }
    return shifted_polygon;
}

bool Polygon::inside(const Point& point, const bool& border_result) const {
    int res = ClipperLib2::PointInPolygon(point.toIntPoint(), (*this)());
    if (res == -1) {
        return border_result;
    }
    return res == 1;
}

bool Polygon::overlaps(const Polygon& p) {
    ClipperLib2::Clipper clipper;

    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPath(p(), ClipperLib2::ptClip, true);

    ClipperLib2::Paths solution;
    clipper.Execute(ClipperLib2::ctIntersection, solution, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);

    return !solution.empty();
}

Area Polygon::area() const { return Area(ClipperLib2::Area((*this)())); }

Point Polygon::closestPointTo(const Point& rhs) const {
    Point ret = rhs;
    Distance bestDist = Distance(std::numeric_limits<float>::max());
    for (const Point& point : (*this)) {
        Distance dist = rhs.distance(point);
        if (dist < bestDist) {
            ret = point;
            bestDist = dist;
        }
    }
    return ret;
}

Polygon Polygon::simplify(const Angle& tolerance) {
    Polygon polygon;
    polygon.reserve(size());
    Point current, next;
    Point prev = operator[](size() - 1);
    for (int i = 0; i < size(); i++) {
        current = (*this)[i];
        next = (*this)[(i + 1) % size()];
        if (!MathUtils::nearCollinear(prev, current, next, tolerance)) {
            polygon.append(current);
        }
        prev = current;
    }
    polygon.squeeze();
    return polygon;
}

Polyline Polygon::toPolyline() {
    Polyline newLine;
    newLine.reserve(this->size());

    for (auto it = this->begin(), end = this->end(); it != end; ++it)
        newLine.push_back(*it);

    newLine.push_back(this->at(0));

    return newLine;
}

QVector<Polyline> Polygon::getEdges() const {
    QVector<Polyline> edges;

    for (size_t i = 0; i < size(); ++i) {
        const Point& start = this->at(i);
        const Point& end = this->at((i + 1) % size());
        edges += Polyline({start, end});
    }

    return edges;
}

PolygonList Polygon::operator+(const PolygonList& rhs) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {rhs};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(rhs(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList Polygon::operator+(const Polygon& rhs) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {rhs};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPath(rhs(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList Polygon::operator-(const PolygonList& other) {
    QVector<Polygon> all_polys =
        QVector<Polygon> {*this} + QVector<Polygon> {PolygonList(other).reverseNormalDirections()};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctDifference, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList Polygon::operator-(const Polygon& rhs) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {Polygon(rhs).reverseNormalDirections()};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPath(rhs(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctDifference, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList Polygon::operator|(const PolygonList& rhs) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {rhs};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(rhs(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList Polygon::operator|(const Polygon& rhs) {
    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPath(rhs(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals({*this, rhs});

    return result;
}

PolygonList Polygon::operator&(const PolygonList& rhs) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {rhs};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(rhs(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctIntersection, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList Polygon::operator&(const Polygon& rhs) {
    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPath(rhs(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctIntersection, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals({*this, rhs});

    return result;
}

QVector<Polyline> Polygon::operator&(const Polyline& rhs) {
    ClipperLib2::PolyTree poly_tree;
    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath(rhs(), ClipperLib2::ptSubject, false);
    clipper.AddPath(operator()(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctIntersection, poly_tree, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    ClipperLib2::OpenPathsFromPolyTree(poly_tree, paths);

    QVector<Polyline> rv;
    for (ClipperLib2::Path path : paths) {
        rv += Polyline(path);
    }
    return rv;
}

PolygonList Polygon::operator^(const PolygonList& rhs) {
    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(rhs(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctXor, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    return result;
}

PolygonList Polygon::operator^(const Polygon& rhs) {
    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath((*this)(), ClipperLib2::ptSubject, true);
    clipper.AddPath(rhs(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctXor, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    return result;
}

ClipperLib2::Path Polygon::operator()() const {
    ClipperLib2::Path path;
    for (Point point : (*this)) {
        path.push_back(point.toIntPoint());
    }
    return path;
}

bool Polygon::operator==(const Polygon& rhs) const {
    if (rhs.size() != this->size()) // they both must be the same size
        return false;

    for (int index = 0; index < this->size(); index++) {
        if (this->at(index) != rhs[index]) // The points differ
            return false;
    }
    return true;
}

ClipperLib2::Path Polygon::getPath() { return this->operator()(); }
} // namespace ORNL
