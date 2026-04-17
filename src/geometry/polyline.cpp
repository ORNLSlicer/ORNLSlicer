#include "geometry/polyline.h"

#include <cmath>
#include <deque>
#include <iterator>
#include <limits>
#include <vector>

#include <clipper.hpp>
#include <psimpl.h>
#include <qcontainerfwd.h>
#include <qmatrix4x4.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "units/unit.h"

namespace ORNL {
Polyline::Polyline(const QVector<Point>& path) { QVector<Point>::operator+=(path); }

Polyline::Polyline(const QVector<ClipperLib2::IntPoint>& path) {
    for (ClipperLib2::IntPoint p : path) {
        append(Point(p));
    }
}

Polyline::Polyline(const ClipperLib2::Path& path) {
    for (ClipperLib2::IntPoint p : path) {
        append(Point(p));
    }
}

Polyline::Polyline(const std::vector<MeshTypes::Point_3>& cgal_polyline) {
    for (auto p : cgal_polyline) {
        append(Point(p));
    }
}

Polyline::Polyline(const QVector<QPair<double, double>>& pts) {
    for (auto pt : pts) {
        append(Point(pt.first, pt.second));
    }
}

Distance Polyline::length() const {
    Distance this_length = Distance(0);
    Point p0 = operator[](0);
    for (int i = 1; i < size(); i++) {
        Point p1 = operator[](i);
        this_length += p1.distance(p0);
        p0 = p1;
    }
    return this_length;
}

Polyline Polyline::concatenate(Polyline rhs, bool this_end_point, bool rhs_end_point) {
    Polyline concatenated_polyline = *this;
    if (this_end_point != rhs_end_point) {
        if (this_end_point == true) {
            for (const Point& point : *this) {
                rhs.append(point);
                concatenated_polyline = rhs;
            }
        }
        else {
            for (const Point& point : rhs) {
                concatenated_polyline.append(point);
            }
        }
    } //! if this_end_point != rhs_end_point
    else {
        if (this_end_point == true) {
            for (const Point& point : rhs) {
                concatenated_polyline.prepend(point);
            }
        }
        else {
            for (int i = rhs.size() - 1; i >= 0; --i) {
                concatenated_polyline.append(rhs[i]);
            }
        }
    } //! if this_end_point == rhs_end_point

    return concatenated_polyline;
}

Polyline Polyline::reverse() {
    Polyline reversed_polyline;
    for (Point& point : *this) {
        reversed_polyline.prepend(point);
    }
    return reversed_polyline;
}

bool Polyline::shorterThan(Distance check_length) const {
    Distance this_length = length();
    return this_length < check_length;
}

Point Polyline::closestPointTo(const Point& rhs) const {
    Point ret = rhs;
    Distance bestDist = Distance(std::numeric_limits<float>::max());
    for (Point point : (*this)) {
        Distance dist = rhs.distance(point);
        if (dist < bestDist) {
            ret = point;
            bestDist = dist;
        }
    }
    return ret;
}

Polyline Polyline::simplify(Distance tol) {
    std::deque<double> polyline;
    for (Point& p : *this) {
        polyline.push_back(p.x());
        polyline.push_back(p.y());
    }

    std::vector<double> result;
    psimpl::simplify_douglas_peucker<2>(polyline.begin(), polyline.end(), tol(), std::back_inserter(result));

    Polyline simplified;
    for (uint n = 0, end = result.size(); n < end; n += 2)
        simplified.append(Point(result[n], result[n + 1]));

    return simplified;
}

Polygon Polyline::close() {
    Polygon ret(operator()());
    if (ret.back() == ret.front()) {
        ret.pop_back();
    }
    return ret;
}

Polyline Polyline::cleanPolygon(const Distance distance) {
    ClipperLib2::Path path;
    ClipperLib2::CleanPolygon((*this)(), path, distance());
    return Polyline(path);
}

Polygon Polyline::makeReal(const Distance& bead_width) {
    Point A = this->first();
    Point B = this->last();

    Distance x_dist = A.x() - B.x();
    Distance y_dist = A.y() - B.y();
    Distance length = A.distance(B);

    Point p1, p2, p3, p4;

    p1.x(B.x() + ((x_dist) / length) * (bead_width / 2));
    p1.y(B.y() + ((y_dist) / length) * (bead_width / 2));
    p1 = p1.rotateAround(B, 90 * deg);
    p1.x(round(p1.x()));
    p1.y(round(p1.y()));

    p2.x(B.x() - ((x_dist) / length) * (bead_width / 2));
    p2.y(B.y() - ((y_dist) / length) * (bead_width / 2));
    p2 = p2.rotateAround(B, 90 * deg);
    p2.x(round(p2.x()));
    p2.y(round(p2.y()));

    p3.x(A.x() - ((x_dist) / length) * (bead_width / 2));
    p3.y(A.y() - ((y_dist) / length) * (bead_width / 2));
    p3 = p3.rotateAround(A, 90 * deg);
    p3.x(round(p3.x()));
    p3.y(round(p3.y()));

    p4.x(A.x() + ((x_dist) / length) * (bead_width / 2));
    p4.y(A.y() + ((y_dist) / length) * (bead_width / 2));
    p4 = p4.rotateAround(A, 90 * deg);
    p4.x(round(p4.x()));
    p4.y(round(p4.y()));

    return Polygon({p1, p2, p3, p4});
}

Polyline Polyline::rotate(Angle rotation_angle, QVector3D axis) {
    return rotateAround({0, 0, 0}, rotation_angle, axis);
}

Polyline Polyline::rotateAround(Point center, Angle rotation_angle, QVector3D axis) {
    Polyline polyline;
    QVector3D c = center.toQVector3D();
    QMatrix4x4 m;
    m.rotate(-rotation_angle.to(deg), axis);
    for (int i = 0; i < size(); i++) {
        QVector3D p = operator[](i).toQVector3D();
        p -= c;
        p = m * p;
        p += c;
        polyline.append(Point::fromQVector3D(p));
    }
    return polyline;
}

bool Polyline::inside(const Point& point, bool border_result) const {
    if (this->first() == this->last()) {
        int res = ClipperLib2::PointInPolygon(point.toIntPoint(), (*this)());
        if (res == -1) {
            return border_result;
        }
        return res == 1;
    }
    return false;
}

bool Polyline::orientation() const { return ClipperLib2::Orientation((*this)()); }

Polyline Polyline::operator+(Polyline rhs) {
    if (last() == rhs.first()) {
        rhs.removeFirst();
    }
    Polyline polyline(*this);
    for (int i = 0; i < rhs.size(); i++) {
        polyline.push_back(rhs[i]);
    }
    return polyline;
}

Polyline Polyline::operator+(const Point& rhs) {
    Polyline rv(*this);
    rv += rhs;
    return rv;
}

Polyline& Polyline::operator+=(Polyline rhs) {
    if (this->size() > 0 && last() == rhs.first()) {
        rhs.removeFirst();
    }
    for (int i = 0; i < rhs.size(); i++) {
        push_back(rhs[i]);
    }
    return *this;
}

Polyline& Polyline::operator+=(const Point& rhs) {
    push_back(rhs);
    return *this;
}

QVector<Polyline> Polyline::operator-(const Polygon& rhs) {
    ClipperLib2::PolyTree poly_tree;
    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPath(rhs(), ClipperLib2::ptClip, true);
    clipper.AddPath(operator()(), ClipperLib2::ptSubject, false);
    clipper.Execute(ClipperLib2::ctDifference, poly_tree, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    ClipperLib2::OpenPathsFromPolyTree(poly_tree, paths);

    QVector<Polyline> rv;
    for (ClipperLib2::Path path : paths) {
        rv += Polyline(path);
    }
    return rv;
}

QVector<Polyline> Polyline::operator-(const PolygonList& rhs) {
    ClipperLib2::PolyTree poly_tree;
    ClipperLib2::Paths paths;
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(rhs(), ClipperLib2::ptClip, true);
    clipper.AddPath(operator()(), ClipperLib2::ptSubject, false);
    clipper.Execute(ClipperLib2::ctDifference, poly_tree, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);
    ClipperLib2::OpenPathsFromPolyTree(poly_tree, paths);

    QVector<Polyline> rv;
    for (ClipperLib2::Path path : paths) {
        rv += Polyline(path);
    }
    return rv;
}

Point Polyline::min() const {
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

Point Polyline::max() const {
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

QVector<Point> Polyline::operator&(const Polyline& rhs) {
    // TODO
    return QVector<Point>();
}

ClipperLib2::Path Polyline::operator()() const {
    ClipperLib2::Path path;
    for (Point p : (*this)) {
        path.push_back(p.toIntPoint());
    }
    return path;
}
} // namespace ORNL
