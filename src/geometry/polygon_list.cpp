
#include "geometry/polygon_list.h"

#include "QPolygon"
#include "utilities/mathutils.h"

namespace ORNL {
PolygonList::PolygonList() {}

PolygonList::PolygonList(QVector<QVector<QPair<double, double>>> raw_poly_list) {
    for (QVector<QPair<double, double>> raw_poly : raw_poly_list) {
        Polygon poly;
        for (QPair<double, double> pt : raw_poly)
            poly.push_back(Point(pt.first, pt.second));

        append(poly);
    }
}

QVector<QVector<QPair<double, double>>> PolygonList::getRawPoints() {
    QVector<QVector<QPair<double, double>>> polylist;

    for (Polygon poly : *this) {
        QVector<QPair<double, double>> pts;
        for (Point p : poly) {
            pts.push_back(QPair<double, double>(p.x(), p.y()));
        }
        polylist.push_back(pts);
    }
    return polylist;
}

uint PolygonList::pointCount() const {
    uint rv = 0;
    for (Polygon p : (*this)) {
        rv += p.size();
    }
    return rv;
}

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

Polygon PolygonList::convexHull() const {
    uint num_points = 0;
    for (const ClipperLib2::Path& path : (*this)()) {
        num_points += path.size();
    }

    QVector<ClipperLib2::IntPoint> all_points;
    for (const ClipperLib2::Path& path : (*this)()) {
        for (const ClipperLib2::IntPoint& point : path) {
            all_points.push_back(point);
        }
    }

    struct HullSort {
        bool operator()(const ClipperLib2::IntPoint& a, const ClipperLib2::IntPoint& b) {
            return (a.X < b.X) || (a.X == b.X && a.Y < b.Y);
        }
    };

    std::sort(all_points.begin(), all_points.end(), HullSort());

    // positive for left turn, 0 for straight, negative for right turn
    auto ccw = [](const ClipperLib2::IntPoint& p0, const ClipperLib2::IntPoint& p1,
                  const ClipperLib2::IntPoint& p2) -> int64_t {
        ClipperLib2::IntPoint v01(p1.X - p0.X, p1.Y - p0.Y);
        ClipperLib2::IntPoint v12(p2.X - p1.X, p2.Y - p1.Y);

        return static_cast<int64_t>(v01.X) * v12.Y - static_cast<int64_t>(v01.Y) * v12.X;
    };

    Polygon hull_poly;
    ClipperLib2::Path hull_points = hull_poly();
    hull_points.resize(num_points + 1);

    // index to insert next hull point, also number of valid hull points
    uint hull_idx = 0;

    // Build lower hull
    for (uint pt_idx = 0; pt_idx < num_points; pt_idx++) {
        while (hull_idx >= 2 && ccw(hull_points[hull_idx - 2], hull_points[hull_idx - 1], all_points[pt_idx]) <= 0) {
            hull_idx--;
        }
        hull_points[hull_idx] = all_points[pt_idx];
        hull_idx++;
    }

    // Build upper hull
    uint min_upper_hull_chain_end_idx = hull_idx + 1;
    for (int pt_idx = num_points - 2; pt_idx >= 0; pt_idx--) {
        while (hull_idx >= min_upper_hull_chain_end_idx &&
               ccw(hull_points[hull_idx - 2], hull_points[hull_idx - 1], all_points[pt_idx]) <= 0) {
            hull_idx--;
        }
        hull_points[hull_idx] = all_points[pt_idx];
        hull_idx++;
    }

    // assert(hull_idx <= hull_points.size());

    // Last point is duplicted with first.  It is removed in the resize.
    hull_points.resize(hull_idx - 2);

    return hull_poly;
}

PolygonList PolygonList::smooth(const Distance& removeLength) {
    //! \todo psmooth
    return PolygonList();
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

PolygonList PolygonList::getOutsidePolygons() const {
    PolygonList polygons;
    int polygonId = 0;

    for (Polygon p : (*this)) {
        // based on an assumption: the first polygon is the most exterior one
        if (polygonId == 0)
            polygons.append(p);
        else {
            // this should not happen in ORNLSlicer as a PolyhonList only contains a polygon and some interior polygons
            if (!polygons.first().inside(p.boundingRectCenter())) {
                polygons.append(p);
            }
        }
        polygonId++;
    }
    return polygons;
}

PolygonList PolygonList::removeEmptyHoles() const {
    PolygonList ret;
    ClipperLib2::Clipper clipper(clipper_init);
    ClipperLib2::PolyTree poly_tree;
    constexpr bool paths_are_closed_polys = true;
    clipper.AddPaths((*this)(), ClipperLib2::ptSubject, paths_are_closed_polys);
    clipper.Execute(ClipperLib2::ctUnion, poly_tree);

    bool remove_holes = true;
    removeEmptyHoles_processPolyTreeNode(poly_tree, remove_holes, ret);

    ret.restoreNormals(*this);

    return ret;
}

PolygonList PolygonList::getEmptyHoles() const {
    PolygonList ret;
    ClipperLib2::Clipper clipper(clipper_init);
    ClipperLib2::PolyTree poly_tree;
    constexpr bool paths_are_closed_polys = true;
    clipper.AddPaths((*this)(), ClipperLib2::ptSubject, paths_are_closed_polys);
    clipper.Execute(ClipperLib2::ctUnion, poly_tree);

    bool remove_holes = false;
    removeEmptyHoles_processPolyTreeNode(poly_tree, remove_holes, ret);

    ret.restoreNormals(*this);

    return ret;
}

void PolygonList::removeEmptyHoles_processPolyTreeNode(const ClipperLib2::PolyNode& node, const bool remove_holes,
                                                       PolygonList& ret) const {
    for (int outer_poly_idx = 0; outer_poly_idx < node.ChildCount(); outer_poly_idx++) {
        ClipperLib2::PolyNode* child = node.Childs[outer_poly_idx];
        if (remove_holes) {
            ret += child->Contour;
        }
        for (int hole_node_idx = 0; hole_node_idx < child->ChildCount(); hole_node_idx++) {
            ClipperLib2::PolyNode& hole_node = *child->Childs[hole_node_idx];
            if ((hole_node.ChildCount() > 0) == remove_holes) {
                ret += hole_node.Contour;
                removeEmptyHoles_processPolyTreeNode(hole_node, remove_holes, ret);
            }
        }
    }
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
            if (i < 0)
                ret = last;
            else if (i > last)
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

PolygonList PolygonList::removeSmallAreas(Area minAreaSize) {
    PolygonList ret(*this);
    for (uint i = 0; i < ret.size(); i++) {
        Area area = Area(fabs(ret[i].area()()));
        if (area < minAreaSize) // Only create an up/down skin if the area
                                // is large enough. So you do not create
                                // tiny blobs of "trying to fill"
        {
            ret.remove(i);
            i -= 1;
        }
    }
    return ret;
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

Point PolygonList::closestPointTo(const Point& rhs) {
    Distance min_dist = Distance(std::numeric_limits<float>::max());
    Point closest, candidate;

    for (Polygon& poly : *this) {
        candidate = poly.closestPointTo(rhs);

        if (rhs.distance(candidate) < min_dist) {
            closest = candidate;
            min_dist = rhs.distance(closest);
        }
    }

    return closest;
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

PolygonList PolygonList::operator-(const PolygonList& rhs) { return _subtract(rhs); }

PolygonList PolygonList::operator-(const Polygon& rhs) { return _subtract(rhs); }

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

float PolygonList::distanceTo(Point point) // used by poleOfInaccessibility
{
    float rv = FLT_MAX;
    for (const Polygon& ring : *this) {
        for (int i = 0, len = ring.length(), j = len - 1; i < len; j = i++) {
            Point a = ring[i];
            Point b = ring[j];

            rv = qMin(rv, MathUtils::distanceFromLineSegSqrd(point, a, b));
        }
    }
    return (this->inside(point) ? 1 : -1) * qSqrt(rv);
}

PolygonList PolygonList::operator^=(const Polygon& rhs) { return _xor_with_this(rhs); }

#ifdef HAVE_SINGLE_PATH
PolygonList::PolygonList(SinglePath::PolygonList& poly_list) {
    for (SinglePath::Polygon polygon : poly_list) {
        append(polygon);
    }
}

ORNL::PolygonList::operator SinglePath::PolygonList() const {
    SinglePath::PolygonList poly_list;
    for (Polygon polygon : *this) {
        poly_list.append(polygon);
    }
    return poly_list;
}
#endif

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

PolygonList PolygonList::_subtract(const Polygon& other) {
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

PolygonList PolygonList::_subtract(const PolygonList& other) {
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

Area PolygonList::outerArea() {
    for (Polygon p : *this) {
        if (p.orientation()) {
            return p.area();
        }
    }

    return Area();
}

Area PolygonList::netArea() {
    Area a;
    for (Polygon p : *this) {
        a = a + p.area();
    }
    return a;
}

PolygonList PolygonList::shift(Point shift) {
    PolygonList pl;
    for (Polygon pgon : *this) {
        // std::cout << "shifting\n";
        Polygon pgon2;
        for (Point p : pgon) {
            p = p + shift;
            pgon2.append(p);
        }
        pl.append(pgon2);
    }
    return pl;
}

float PolygonList::commonArea(PolygonList cur_layer_outline) {
    PolygonList temp = *this;
    PolygonList p = temp & cur_layer_outline;
    PolygonList n_intrsctn = temp - p;
    Area a = p.outerArea();
    Area b = temp.outerArea();
    float ratio = a() / b();

    return ratio;
}

void PolygonList::addAll(QVector<Polygon> polygons) {
    ClipperLib2::Clipper clipper;
    ClipperLib2::Paths paths;
    for (Polygon poly : polygons)
        clipper.AddPath(poly.getPath(), ClipperLib2::ptSubject, true);
    clipper.Execute(ClipperLib2::ctUnion, paths, ClipperLib2::pftEvenOdd, ClipperLib2::pftEvenOdd);
    clipperLoad(paths);

    restoreNormals(polygons);

    return;
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
