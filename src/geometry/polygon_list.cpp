
#include "geometry/polygon_list.h"

#include <cstdint>
#include <limits>

#include <QPolygon>
#include <clipper.hpp>
#include <qpoint.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/polygon.h"
#include "geometry/polyline.h"
#include "units/unit.h"

namespace ORNL {
PolygonList::PolygonList() {}

PolygonList PolygonList::offset(Distance distance, Distance real_offset, ClipperLib2::JoinType joinType) const {
    ClipperLib2::Paths paths;
    ClipperLib2::ClipperOffset clipper;
    clipper.AddPaths((*this)(), joinType, ClipperLib2::etClosedPolygon);
    clipper.Execute(paths, distance());
    PolygonList polygons(paths);

    polygons.restoreNormals(*this, true);

    //! Save any lost geometry
    if (distance() < 0) {
        polygons.lost_geometry = this->lost_geometry;

        paths.clear();
        clipper.Clear();
        clipper.AddPaths((*this)(), joinType, ClipperLib2::etClosedPolygon);
        clipper.Execute(paths, real_offset());
        PolygonList original_geometry(paths);

        original_geometry.restoreNormals(*this, true);

        paths.clear();
        clipper.Clear();
        clipper.AddPaths(polygons(), joinType, ClipperLib2::etClosedPolygon);
        clipper.Execute(paths, -distance() + 10); //! +10 buffer
        PolygonList reversed_offset_geometry(paths);

        reversed_offset_geometry.restoreNormals(polygons, true);

        PolygonList lost_geometry = original_geometry - reversed_offset_geometry;
        polygons.lost_geometry += lost_geometry;
    }
    return polygons;
}

bool PolygonList::inside(Point p, bool border_result) const {
    int poly_count_inside = 0;
    for (ClipperLib2::Path poly : (*this)()) {
        const int is_inside_this_poly = ClipperLib2::PointInPolygon(p.toIntPoint(), poly);
        if (is_inside_this_poly == -1) {
            return border_result;
        }
        poly_count_inside += is_inside_this_poly;
    }
    return (poly_count_inside % 2) == 1;
}

PolygonList PolygonList::simplify(const Angle tolerance) {
    PolygonList polygons;
    polygons.reserve(size());

    for (Polygon p : (*this)) {
        Polygon sp = p.simplify(tolerance);
        polygons.append(sp);
    }

    return polygons;
}

PolygonList PolygonList::cleanPolygons(const Distance distance) {
    ClipperLib2::Paths paths;
    ClipperLib2::CleanPolygons((*this)(), paths, distance());
    PolygonList cleaned_polygons(paths);

    //! Needed to remove 0 point polygons
    QVector<Polygon>::iterator it = cleaned_polygons.begin();
    while (it != cleaned_polygons.end()) {
        if (it->isEmpty())
            it = cleaned_polygons.erase(it);
        else
            ++it;
    }

    cleaned_polygons.restoreNormals(*this);

    return cleaned_polygons;
}

PolygonList PolygonList::externalPolygonBoundaries() const {
    PolygonList boundaries;

    for (const PolygonList& part : splitIntoParts()) {
        if (!part.isEmpty()) {
            boundaries.append(part.first());
        }
    }

    return boundaries;
}

PolygonList PolygonList::internalPolygonBoundaries() const {
    PolygonList boundaries;

    for (const PolygonList& part : splitIntoParts()) {
        for (int i = 1; i < part.size(); ++i) {
            boundaries.append(part[i]);
        }
    }

    return boundaries;
}

QVector<PolygonList> PolygonList::splitIntoParts(bool unionAll) const {
    QVector<PolygonList> result;
    ClipperLib2::Clipper clipper(clipper_init);
    ClipperLib2::PolyTree resultPolyTree;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    if (unionAll) {
        clipper.Execute(ClipperLib2::ctUnion, resultPolyTree, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    }
    else {
        clipper.Execute(ClipperLib2::ctUnion, resultPolyTree);
    }

    splitIntoParts_processPolyTreeNode(&resultPolyTree, result);

    for (PolygonList& poly_list : result)
        poly_list.restoreNormals(*this);

    return result;
}

void PolygonList::restoreNormals(QVector<Polygon> all_polys, bool offset) {
    if (offset) //! Offset operation: assign normals of closest point
    {
        for (Polygon& subject : *this) {
            for (Point& p1 : subject) {
                Distance min_dist = Distance(std::numeric_limits<float>::max());

                for (Polygon& poly : all_polys) {
                    Point p2 = poly.closestPointTo(p1);

                    if (p1.distance(p2) < min_dist) {
                        min_dist = p1.distance(p2);
                        p1.setNormals(p2.getNormals());
                    }
                }
            }
        }
    }
    else //! Clipping operation: assign normals of exact point. If point can't be found, compute bisecting normal.
    {
        auto index = [](uint i, uint last) {
            uint ret = i;
            if (i > last)
                ret = 0;

            return ret;
        };

        for (Polygon& subject : *this) {
            for (uint i = 0, size = subject.size(); i < size; ++i) {
                bool found = false;
                for (Polygon& poly : all_polys) {
                    for (Point& p : poly) {
                        if (subject[i] == p) {
                            subject[i].setNormals(p.getNormals());
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        break;
                }

                if (!found) //! Compute bisecting normal
                {
                    uint last = subject.size() - 1;

                    QVector3D unit_z {0, 0, 1};
                    QVector3D prev = (subject[index(i - 1, last)] - subject[i]).toQVector3D().normalized();
                    QVector3D next = (subject[index(i + 1, last)] - subject[i]).toQVector3D().normalized();

                    QVector3D normal =
                        (QVector3D::crossProduct(unit_z, prev) + QVector3D::crossProduct(next, unit_z)).normalized();
                    subject[i].setNormals(QVector<QVector3D> {normal, normal});
                }
            }
        }
    }
}

PolygonList PolygonList::reverseNormalDirections() {
    for (Polygon& poly : *this)
        poly.reverseNormalDirections();

    return *this;
}

void PolygonList::splitIntoParts_processPolyTreeNode(ClipperLib2::PolyNode* node, QVector<PolygonList>& ret) const {
    for (int n = 0; n < node->ChildCount(); n++) {
        ClipperLib2::PolyNode* child = node->Childs[n];
        PolygonList part;
        part += child->Contour;
        for (int i = 0; i < child->ChildCount(); i++) {
            part += child->Childs[i]->Contour;
            splitIntoParts_processPolyTreeNode(child->Childs[i], ret);
        }
        if (part.size() > 0)
            ret.push_back(part);
    }
}

PolygonList PolygonList::removeDegenerateVertices() {
    PolygonList ret(*this);
    for (int poly_idx = 0; poly_idx < ret.size(); poly_idx++) {
        Polygon poly = ret[poly_idx];
        Polygon result;

        auto isDegenerate = [](Point& last, Point& now, Point& next) {
            Point last_line = now - last;
            Point next_line = next - now;
            return Point::dot(last_line, next_line) == -1 * last_line.distance() * next_line.distance();
        };
        bool isChanged = false;
        for (int idx = 0; idx < poly.size(); idx++) {
            Point& last = (result.size() == 0) ? poly.back() : result.back();
            if (idx + 1 == poly.size() && result.size() == 0) {
                break;
            }
            Point& next = (idx + 1 == poly.size()) ? result[0] : poly[idx + 1];
            // lines are in the opposite direction
            if (isDegenerate(last, poly[idx], next)) {
                // don't add vert to the result
                isChanged = true;
                while (result.size() > 1 && isDegenerate(result[result.size() - 2], result.back(), next)) {
                    result.pop_back();
                }
            }
            else {
                result += (poly[idx]);
            }
        }

        if (isChanged) {
            if (result.size() > 2) {
                ret[poly_idx] = result;
            }
            else {
                ret.remove(poly_idx);
                poly_idx--; // effectively the next iteration has the same
                            // poly_idx (referring to a new poly which is
                            // not yet processed)
            }
        }
    }
    return ret;
}

Point PolygonList::min() const {
    Point rv(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    for (const Polygon& p : *this) {
        // ignore holes as the minimum values will always come from an
        // exterior
        if (p.area()() > 0) {
            Point temp_min = p.min();

            if (temp_min.x() < rv.x()) {
                rv.x(temp_min.x());
            }

            if (temp_min.y() < rv.y()) {
                rv.y(temp_min.y());
            }

            if (temp_min.z() < rv.z()) {
                rv.z(temp_min.z());
            }
        }
    }
    return rv;
}

Point PolygonList::max() const {
    Point rv(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest());
    for (const Polygon& p : *this) {
        // ignore holes as the maximum values will always come from an
        // exterior
        if (p.area()() > 0) {
            Point temp_min = p.max();

            if (temp_min.x() > rv.x()) {
                rv.x(temp_min.x());
            }

            if (temp_min.y() > rv.y()) {
                rv.y(temp_min.y());
            }

            if (temp_min.z() > rv.z()) {
                rv.z(temp_min.z());
            }
        }
    }
    return rv;
}

Point PolygonList::boundingRectCenter() const { return (max() + min()) / 2.0; }

PolygonList PolygonList::rotate(const Angle& angle, const QVector3D& axis) {
    return rotateAround({0, 0, 0}, angle, axis);
}

PolygonList PolygonList::rotateAround(const Point& center, const Angle& angle, const QVector3D& axis) {
    PolygonList rv;
    for (const Polygon& polygon : *this) {
        rv += polygon.rotateAround(center, angle, axis);
    }
    return rv;
}

int64_t PolygonList::totalLength() {
    int64_t total_length = 0;
    for (Polygon polygon : (*this)) {
        total_length += polygon.polygonLength();
    }

    return total_length;
}

Area PolygonList::totalArea() {
    Area total_area;
    for (Polygon poly : *this) {
        total_area += poly.area();
    }
    return total_area;
}

bool PolygonList::operator==(const PolygonList& rhs) const {
    PolygonList pl1 = *this;
    PolygonList pl2 = rhs;
    if ((pl1 - pl2).isEmpty() && ((pl2) - (pl1)).isEmpty()) {
        return true;
    }
    else {
        return false;
    }
}

bool PolygonList::operator!=(const PolygonList& rhs) const {
    PolygonList pl1 = *this;
    PolygonList pl2 = rhs;
    if ((pl1 - pl2).isEmpty() && ((pl2) - (pl1)).isEmpty()) {
        return false;
    }
    else {
        return true;
    }
}

PolygonList PolygonList::operator+(const PolygonList& rhs) { return _add(rhs); }

PolygonList PolygonList::operator+(const Polygon& rhs) { return _add(rhs); }

PolygonList PolygonList::operator+=(const PolygonList& rhs) { return _add_to_this(rhs); }

PolygonList PolygonList::operator+=(const Polygon& rhs) { return _add_to_this(rhs); }

PolygonList PolygonList::operator-(const PolygonList& rhs) const { return _subtract(rhs); }

PolygonList PolygonList::operator-(const Polygon& rhs) const { return _subtract(rhs); }

PolygonList PolygonList::operator-=(const PolygonList& rhs) { return _subtract_from_this(rhs); }

PolygonList PolygonList::operator-=(const Polygon& rhs) { return _subtract_from_this(rhs); }

PolygonList PolygonList::operator<<(const PolygonList& rhs) { return _add(rhs); }

PolygonList PolygonList::operator<<(const Polygon& rhs) { return _add(rhs); }

PolygonList PolygonList::operator|(const PolygonList& rhs) { return _add(rhs); }

PolygonList PolygonList::operator|(const Polygon& rhs) { return _add(rhs); }

PolygonList PolygonList::operator|=(const PolygonList& rhs) { return _add_to_this(rhs); }

PolygonList PolygonList::operator|=(const Polygon& rhs) { return _add_to_this(rhs); }

PolygonList PolygonList::operator&(const PolygonList& rhs) { return _intersect(rhs); }

PolygonList PolygonList::operator&(const Polygon& rhs) { return _intersect(rhs); }

QVector<Polyline> PolygonList::operator&(const Polyline& rhs) { return _intersect(rhs); }

PolygonList PolygonList::operator&=(const PolygonList& rhs) { return _intersect_with_this(rhs); }

PolygonList PolygonList::operator&=(const Polygon& rhs) { return _intersect_with_this(rhs); }

PolygonList PolygonList::operator^(const PolygonList& rhs) { return _xor(rhs); }

PolygonList PolygonList::operator^(const Polygon& rhs) { return _xor(rhs); }

PolygonList PolygonList::operator^=(const PolygonList& rhs) { return _xor_with_this(rhs); }

PolygonList PolygonList::operator^=(const Polygon& rhs) { return _xor_with_this(rhs); }

PolygonList::PolygonList(const ClipperLib2::Paths& paths) { clipperLoad(paths); }

void PolygonList::clipperLoad(const ClipperLib2::Paths& paths) {
    clear();
    for (ClipperLib2::Path path : paths) {
        Polygon polygon;
        for (ClipperLib2::IntPoint point : path) {
            polygon += Point(point);
        }
        append(polygon);
    }
}

ClipperLib2::Paths PolygonList::operator()() const {
    ClipperLib2::Paths paths;
    for (const Polygon& polygon : *this) {
        ClipperLib2::Path path;
        for (Point point : polygon) {
            path.push_back(point.toIntPoint());
        }
        paths.push_back(path);
    }
    return paths;
}

PolygonList PolygonList::_add(const Polygon& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList PolygonList::_add(const PolygonList& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList PolygonList::_add_to_this(const Polygon& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    restoreNormals(all_polys);

    return (*this);
}

PolygonList PolygonList::_add_to_this(const PolygonList& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    restoreNormals(all_polys);

    return (*this);
}

PolygonList PolygonList::_subtract(const Polygon& other) const {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {Polygon(other).reverseNormalDirections()};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctDifference, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList PolygonList::_subtract(const PolygonList& other) const {
    QVector<Polygon> all_polys =
        QVector<Polygon> {*this} + QVector<Polygon> {PolygonList(other).reverseNormalDirections()};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctDifference, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList PolygonList::_subtract_from_this(const Polygon& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {Polygon(other).reverseNormalDirections()};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctDifference, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    restoreNormals(all_polys);

    return (*this);
}

PolygonList PolygonList::_subtract_from_this(const PolygonList& other) {
    QVector<Polygon> all_polys =
        QVector<Polygon> {*this} + QVector<Polygon> {PolygonList(other).reverseNormalDirections()};

    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctDifference, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    restoreNormals(all_polys);

    return (*this);
}

PolygonList PolygonList::_intersect(const Polygon& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctIntersection, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

PolygonList PolygonList::_intersect(const PolygonList& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctIntersection, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    result.restoreNormals(all_polys);

    return result;
}

QVector<Polyline> PolygonList::_intersect(const Polyline& polyline) {
    ClipperLib2::PolyTree poly_tree;
    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptClip, true);
    clipper.AddPath(polyline(), ClipperLib2::ptSubject, false);
    clipper.Execute(ClipperLib2::ctIntersection, poly_tree, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    ClipperLib2::OpenPathsFromPolyTree(poly_tree, paths);

    QVector<Polyline> rv;
    for (ClipperLib2::Path path : paths) {
        rv += Polyline(path);
    }
    return rv;
}

PolygonList PolygonList::_intersect_with_this(const Polygon& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctIntersection, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    restoreNormals(all_polys);

    return (*this);
}

PolygonList PolygonList::_intersect_with_this(const PolygonList& other) {
    QVector<Polygon> all_polys = QVector<Polygon> {*this} + QVector<Polygon> {other};

    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctIntersection, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    restoreNormals(all_polys);

    return (*this);
}

PolygonList PolygonList::_xor(const Polygon& other) {
    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctXor, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    return result;
}

PolygonList PolygonList::_xor(const PolygonList& other) {
    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctXor, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    PolygonList result(paths);

    return result;
}

PolygonList PolygonList::_xor_with_this(const Polygon& other) {
    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPath(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctXor, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    return (*this);
}

PolygonList PolygonList::_xor_with_this(const PolygonList& other) {
    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    clipper.AddPaths(operator()(), ClipperLib2::ptSubject, true);
    clipper.AddPaths(other(), ClipperLib2::ptClip, true);
    clipper.Execute(ClipperLib2::ctXor, paths, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    clipperLoad(paths);

    return (*this);
}

Area PolygonList::netArea() {
    Area a;
    for (Polygon p : *this) {
        a = a + p.area();
    }
    return a;
}

void PolygonList::addAll(QVector<Polygon> polygons) {
    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    for (Polygon poly : polygons)
        clipper.AddPath(poly.getPath(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftEvenOdd, ClipperLib2::pftEvenOdd);
    clipperLoad(paths);

    restoreNormals(polygons);
}

QVector<QPolygon> PolygonList::toQPolygons() const {
    QVector<QPolygon> ret;

    for (const Polygon& poly : *this) {
        QPolygon cpoly;
        for (const Point& point : poly) {
            cpoly.push_back(point.toQPoint());
        }
        ret.push_back(cpoly);
    }

    return ret;
}

QRect PolygonList::boundingRect() const {
    QPoint min = this->min().toQPoint();
    QPoint max = this->max().toQPoint();

    return QRect(min, max);
}

QVector<Polyline> PolygonList::getEdges() const {
    QVector<Polyline> edges;

    for (const Polygon& poly : *this) {
        edges += poly.getEdges();
    }

    return edges;
}
} // namespace ORNL
