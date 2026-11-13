#include "optimizers/travel_router.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include <clipper.hpp>

namespace ORNL {
namespace {
constexpr double kTolerance      = 1.0e-7;
constexpr double kPlaneTolerance = 1.0;

double endpointTolerance(const Point& point) {
    return 1.0 + std::max(std::abs(point.x()), std::abs(point.y())) * std::numeric_limits<float>::epsilon();
}

double cross(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

double distanceSquared(const Point& point, const Point& start, const Point& end) {
    const double dx             = end.x() - start.x();
    const double dy             = end.y() - start.y();
    const double length_squared = dx * dx + dy * dy;
    const double t =
        length_squared > 0
            ? std::clamp(((point.x() - start.x()) * dx + (point.y() - start.y()) * dy) / length_squared, 0.0, 1.0)
            : 0.0;
    return std::pow(point.x() - start.x() - t * dx, 2) + std::pow(point.y() - start.y() - t * dy, 2);
}

//! \brief Collects boundary crossings, including both ends of collinear overlaps.
void intersections(const Point& start, const Point& end, const Point& a, const Point& b,
                   std::vector<double>& parameters) {
    const double dx = end.x() - start.x(), dy = end.y() - start.y();
    const double ex = b.x() - a.x(), ey = b.y() - a.y();
    const double ax = a.x() - start.x(), ay = a.y() - start.y();
    const double denominator = cross(dx, dy, ex, ey);
    if (std::abs(denominator) > kTolerance) {
        const double t = cross(ax, ay, ex, ey) / denominator;
        const double u = cross(ax, ay, dx, dy) / denominator;
        if (t >= -kTolerance && t <= 1 + kTolerance && u >= -kTolerance && u <= 1 + kTolerance) {
            parameters.push_back(std::clamp(t, 0.0, 1.0));
        }
    }
    else if (std::abs(cross(ax, ay, dx, dy)) <= kTolerance) {
        const double length_squared = dx * dx + dy * dy;
        if (length_squared <= kTolerance) { return; }
        double first = (ax * dx + ay * dy) / length_squared;
        double last  = ((b.x() - start.x()) * dx + (b.y() - start.y()) * dy) / length_squared;
        if (first > last) { std::swap(first, last); }
        if (first <= 1 && last >= 0) {
            parameters.push_back(std::max(0.0, first));
            parameters.push_back(std::min(1.0, last));
        }
    }
}

//! \brief Even-odd containment in the union boundaries; exact boundary contact is permitted.
bool inside(double x, double y, const QVector<QVector<Point>>& boundaries, bool border_result = false) {
    bool result = false;
    for (const auto& boundary : boundaries) {
        for (int i = 0, j = boundary.size() - 1; i < boundary.size(); j = i++) {
            const auto& a   = boundary[j];
            const auto& b   = boundary[i];
            const double dx = b.x() - a.x(), dy = b.y() - a.y();
            if (std::abs(cross(x - a.x(), y - a.y(), dx, dy)) <= kTolerance * std::max(1.0, std::hypot(dx, dy)) &&
                x >= std::min(a.x(), b.x()) - kTolerance && x <= std::max(a.x(), b.x()) + kTolerance &&
                y >= std::min(a.y(), b.y()) - kTolerance && y <= std::max(a.y(), b.y()) + kTolerance) {
                return border_result;
            }
            if ((a.y() > y) != (b.y() > y) && x < a.x() + (y - a.y()) * dx / dy) { result = !result; }
        }
    }
    return result;
}
}  // namespace

void TravelRouter::addObstacle(const Point& start, const Point& end, Distance width) {
    if (std::abs(start.z() - end.z()) > kPlaneTolerance || (start.x() == end.x() && start.y() == end.y())) { return; }
    // Cover integer rounding and subsequent conversion of Clipper boundaries back to float coordinates.
    const double float_rounding =
        std::max({std::abs(start.x()), std::abs(start.y()), std::abs(end.x()), std::abs(end.y())}) *
        std::numeric_limits<float>::epsilon();
    m_obstacles.push_back({start, end, std::max(1.0, width() / 2.0), 1.0 + float_rounding});
}

QVector<Point> TravelRouter::route(const Point& start, const Point& end, Distance standoff) const {
    if (std::abs(start.z() - end.z()) > kPlaneTolerance || (start.x() == end.x() && start.y() == end.y())) {
        return {};
    }

    const double extra_clearance = std::max(0.0, standoff());
    QVector<const Obstacle*> obstacles;
    bool near_obstacle = false;
    for (const auto& obstacle : m_obstacles) {
        if (std::abs(obstacle.start.z() - start.z()) > kPlaneTolerance) { continue; }
        obstacles.push_back(&obstacle);
        std::vector<double> hits;
        intersections(start, end, obstacle.start, obstacle.end, hits);
        const double separation = std::min(
            {distanceSquared(start, obstacle.start, obstacle.end), distanceSquared(end, obstacle.start, obstacle.end),
             distanceSquared(obstacle.start, start, end), distanceSquared(obstacle.end, start, end)});
        const double clearance = obstacle.half_width + obstacle.rounding + extra_clearance;
        near_obstacle |= !hits.empty() || separation <= clearance * clearance;
    }
    if (!near_obstacle) { return {}; }

    ClipperLib2::Paths strokes;
    for (qsizetype first = 0; first < obstacles.size();) {
        const auto* obstacle = obstacles[first];
        auto toInteger       = [](const Point& point) {
            return ClipperLib2::IntPoint(std::llround(point.x()), std::llround(point.y()));
        };
        ClipperLib2::Path chain {toInteger(obstacle->start), toInteger(obstacle->end)};
        double rounding = obstacle->rounding;
        qsizetype last  = first + 1;
        // Join a printed contour before offsetting so segment caps do not create corner protrusions.
        while (last < obstacles.size() && obstacles[last]->half_width == obstacle->half_width &&
               obstacles[last - 1]->end.x() == obstacles[last]->start.x() &&
               obstacles[last - 1]->end.y() == obstacles[last]->start.y() && chain.front() != chain.back()) {
            const auto point = toInteger(obstacles[last]->end);
            if (point != chain.back()) chain.push_back(point);
            rounding = std::max(rounding, obstacles[last]->rounding);
            ++last;
        }
        const bool closed = chain.front() == chain.back();
        if (closed) chain.pop_back();
        ClipperLib2::ClipperOffset offset;
        const double radius   = obstacle->half_width + rounding + extra_clearance;
        bool use_segment_caps = false;
        // Clipper 5.1.2's one-unit normal shortcut can cut through a nearly reversing corner.
        // Keep these rare joins covered with separate segment caps.
        for (size_t i = closed ? 0 : 1; i < (closed ? chain.size() : chain.size() - 1); ++i) {
            const auto& a   = chain[(i + chain.size() - 1) % chain.size()];
            const auto& b   = chain[i];
            const auto& c   = chain[(i + 1) % chain.size()];
            const double ax = b.X - a.X, ay = b.Y - a.Y, bx = c.X - b.X, by = c.Y - b.Y;
            if (ax * bx + ay * by < 0 &&
                std::abs(cross(ax, ay, bx, by)) * radius < std::hypot(ax, ay) * std::hypot(bx, by)) {
                use_segment_caps = true;
                break;
            }
        }
        if (use_segment_caps) {
            for (size_t i = 0; i < (closed ? chain.size() : chain.size() - 1); ++i) {
                offset.AddPath({chain[i], chain[(i + 1) % chain.size()]}, ClipperLib2::jtMiter,
                               ClipperLib2::etOpenSquare);
            }
        }
        else {
            offset.AddPath(chain, ClipperLib2::jtMiter,
                           closed && chain.size() >= 3 ? ClipperLib2::etClosedLine : ClipperLib2::etOpenSquare);
        }
        ClipperLib2::Paths stroke;
        offset.Execute(stroke, radius);
        strokes.insert(strokes.end(), stroke.begin(), stroke.end());
        first = last;
    }
    ClipperLib2::Clipper clipper;
    clipper.AddPaths(strokes, ClipperLib2::ptSubject, true);
    ClipperLib2::PolyTree united;
    clipper.Execute(ClipperLib2::ctUnion, united, ClipperLib2::pftNonZero, ClipperLib2::pftNonZero);

    // Each hole is a separate free region; nested obstacles share their enclosing hole's component.
    QVector<QVector<Point>> boundaries;
    QVector<int> components;
    QVector<bool> holes;
    int next_component                                       = 0;
    std::function<void(ClipperLib2::PolyNode*, int)> collect = [&](ClipperLib2::PolyNode* node, int component) {
        for (auto* child : node->Childs) {
            const int child_component = child->IsHole() ? ++next_component : component;
            QVector<Point> boundary;
            for (const auto& point : child->Contour) { boundary.push_back(Point(point.X, point.Y, start.z())); }
            boundaries.push_back(boundary);
            components.push_back(child_component);
            holes.push_back(child->IsHole());
            collect(child, child_component);
        }
    };
    collect(&united, 0);

    auto visible = [&](const Point& a, const Point& b, bool allow_start, bool allow_end) {
        const double length          = std::hypot(b.x() - a.x(), b.y() - a.y());
        const double start_tolerance = allow_start ? endpointTolerance(a) : 0.0;
        const double end_tolerance   = allow_end ? endpointTolerance(b) : 0.0;
        for (const auto* obstacle : obstacles) {
            std::vector<double> hits;
            intersections(a, b, obstacle->start, obstacle->end, hits);
            for (double t : hits) {
                // Wipe interpolation can leave an original endpoint slightly off its source centerline.
                // Bound that contact in microns so the allowance does not grow with travel length.
                if (!((allow_start && t * length <= start_tolerance) ||
                      (allow_end && (1.0 - t) * length <= end_tolerance))) {
                    return false;
                }
            }
            // A collinear overlap has two hits; endpoint exceptions cannot permit following a printed centerline.
            if (hits.size() == 2 && (hits.back() - hits.front()) * length > kTolerance) { return false; }
        }

        std::vector<double> parameters {0.0, 1.0};
        for (const auto& boundary : boundaries) {
            for (int i = 0, j = boundary.size() - 1; i < boundary.size(); j = i++) {
                intersections(a, b, boundary[j], boundary[i], parameters);
            }
        }
        std::sort(parameters.begin(), parameters.end());
        parameters.erase(std::unique(parameters.begin(), parameters.end(),
                                     [](double lhs, double rhs) { return std::abs(lhs - rhs) <= kTolerance; }),
                         parameters.end());
        for (size_t i = 1; i < parameters.size(); ++i) {
            const double t = (parameters[i - 1] + parameters[i]) / 2.0;
            if (inside(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t, boundaries) &&
                !(allow_start && i == 1) && !(allow_end && i + 1 == parameters.size())) {
                return false;
            }
        }
        return true;
    };
    if (visible(start, end, true, true)) { return {}; }

    struct Endpoint {
        Point point;
        int component;
    };
    auto freeComponent = [&](const Point& point) {
        if (inside(point.x(), point.y(), boundaries)) { return -1; }
        int component = 0;
        for (int i = 0; i < boundaries.size(); ++i) {
            if (holes[i] && inside(point.x(), point.y(), {boundaries[i]}, true)) { component = components[i]; }
        }
        return component;
    };
    auto endpoints = [&](const Point& original) {
        QVector<Endpoint> result;
        const int component = freeComponent(original);
        if (component >= 0) {
            result.push_back({original, component});
            return result;
        }
        struct Projection {
            Point point;
            int component;
            double distance;
        };
        std::vector<Projection> projections;
        for (int i = 0; i < boundaries.size(); ++i) {
            const auto& boundary = boundaries[i];
            for (int j = 0, k = boundary.size() - 1; j < boundary.size(); k = j++) {
                const double dx = boundary[j].x() - boundary[k].x(), dy = boundary[j].y() - boundary[k].y();
                const double t =
                    std::clamp(((original.x() - boundary[k].x()) * dx + (original.y() - boundary[k].y()) * dy) /
                                   (dx * dx + dy * dy),
                               0.0, 1.0);
                Point point(boundary[k].x() + t * dx, boundary[k].y() + t * dy, start.z());
                const double distance = original.distance(point)();
                if (distance <= kTolerance) { continue; }
                // Clear float rounding at a projected boundary point by one internal micron.
                point.x(point.x() + (point.x() - original.x()) / distance);
                point.y(point.y() + (point.y() - original.y()) / distance);
                projections.push_back({point, components[i], original.distance(point)()});
            }
        }
        std::sort(projections.begin(), projections.end(),
                  [](const Projection& lhs, const Projection& rhs) { return lhs.distance < rhs.distance; });
        for (const auto& projection : projections) {
            if (std::any_of(result.begin(), result.end(),
                            [&](const Endpoint& endpoint) { return endpoint.component == projection.component; })) {
                continue;
            }
            if (freeComponent(projection.point) == projection.component &&
                visible(original, projection.point, true, false)) {
                result.push_back({projection.point, projection.component});
            }
        }
        return result;
    };

    struct Hit {
        double t;
        int boundary;
        int edge;
    };
    auto followBoundaries = [&](const Endpoint& a, const Endpoint& b) {
        // Replace blocked straight intervals by the shorter chain along an already clear union boundary.
        QVector<Point> result {a.point};
        std::vector<Hit> hits {{0, -1, -1}, {1, -1, -1}};
        for (int i = 0; i < boundaries.size(); ++i) {
            if (components[i] != a.component) { continue; }
            const auto& boundary = boundaries[i];
            for (int j = 0; j < boundary.size(); ++j) {
                std::vector<double> parameters;
                intersections(a.point, b.point, boundary[j], boundary[(j + 1) % boundary.size()], parameters);
                for (double t : parameters) { hits.push_back({t, i, j}); }
            }
        }
        std::sort(hits.begin(), hits.end(), [](const Hit& lhs, const Hit& rhs) { return lhs.t < rhs.t; });
        auto pointAt = [&](double t) {
            return Point(a.point.x() + (b.point.x() - a.point.x()) * t, a.point.y() + (b.point.y() - a.point.y()) * t,
                         start.z());
        };
        auto append = [](QVector<Point>& points, const Point& point) {
            if (points.isEmpty() || points.back().x() != point.x() || points.back().y() != point.y()) {
                points.push_back(point);
            }
        };
        for (size_t i = 1; i < hits.size(); ++i) {
            if (hits[i].t - hits[i - 1].t <= kTolerance) { continue; }
            const Point middle = pointAt((hits[i].t + hits[i - 1].t) / 2.0);
            if (freeComponent(middle) == a.component) { continue; }
            size_t entry = i - 1;
            while (entry > 0 && hits[entry].boundary < 0 && std::abs(hits[entry - 1].t - hits[entry].t) <= kTolerance) {
                --entry;
            }
            if (hits[entry].boundary < 0) { return QVector<Point>(); }
            const int boundary_index = hits[entry].boundary;
            size_t exit              = hits.size();
            while (exit > i && hits[exit - 1].boundary != boundary_index) { --exit; }
            if (exit <= i || hits[exit - 1].t - hits[entry].t <= kTolerance) { return QVector<Point>(); }
            --exit;
            const auto& boundary    = boundaries[boundary_index];
            const Point entry_point = pointAt(hits[entry].t), exit_point = pointAt(hits[exit].t);
            QVector<Point> forward {entry_point}, backward {entry_point};
            int vertex = (hits[entry].edge + 1) % boundary.size();
            while (vertex != (hits[exit].edge + 1) % boundary.size()) {
                append(forward, boundary[vertex]);
                vertex = (vertex + 1) % boundary.size();
            }
            append(forward, exit_point);
            vertex = hits[entry].edge;
            while (vertex != hits[exit].edge) {
                append(backward, boundary[vertex]);
                vertex = (vertex + boundary.size() - 1) % boundary.size();
            }
            append(backward, exit_point);
            auto length = [](const QVector<Point>& points) {
                double distance = 0;
                for (int j = 1; j < points.size(); ++j) { distance += points[j - 1].distance(points[j])(); }
                return distance;
            };
            for (const auto& point : length(forward) < length(backward) ? forward : backward) { append(result, point); }
            i = exit;
        }
        append(result, b.point);
        return result;
    };

    QVector<Point> best;
    double best_length = std::numeric_limits<double>::infinity();
    const auto starts = endpoints(start), ends = endpoints(end);
    for (const auto& a : starts) {
        for (const auto& b : ends) {
            if (a.component != b.component) { continue; }
            auto points = followBoundaries(a, b);
            if (points.isEmpty()) { continue; }
            double length = start.distance(points.front())() + points.back().distance(end)();
            for (int i = 1; i < points.size(); ++i) { length += points[i - 1].distance(points[i])(); }
            if (length < best_length) {
                best_length = length;
                best        = points;
            }
        }
    }
    if (best.isEmpty() && extra_clearance > 0) { return route(start, end); }

    QVector<Point> simplified;
    for (const auto& point : best) {
        simplified.push_back(point);
        while (simplified.size() >= 3) {
            const int middle      = simplified.size() - 2;
            const Point& a        = simplified[middle - 1];
            const Point& b        = simplified[middle];
            const Point& c        = simplified[middle + 1];
            const bool collinear  = distanceSquared(b, a, c) <= kTolerance * kTolerance;
            const double rounding = 2.0 * std::max({endpointTolerance(a), endpointTolerance(b), endpointTolerance(c)});
            const bool tiny       = std::min(a.distance(b)(), b.distance(c)()) <= rounding;
            // Preserve the first and last envelope points, which connect to the immutable travel endpoints.
            if (!collinear && !(tiny && visible(a, c, false, false))) { break; }
            simplified.removeAt(middle);
        }
    }
    best = simplified;
    if (!best.isEmpty() && best.front().x() == start.x() && best.front().y() == start.y()) { best.removeFirst(); }
    if (!best.isEmpty() && best.back().x() == end.x() && best.back().y() == end.y()) { best.removeLast(); }
    return best;
}
}  // namespace ORNL
