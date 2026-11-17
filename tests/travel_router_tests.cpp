#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "optimizers/travel_router.h"
#include "test_utils.h"

namespace {
using ORNL::Point;
using ORNL::TravelRouter;

struct Stroke {
    Point start;
    Point end;
};

double cross(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

//! \brief Independent centerline check, including vertex contacts and overlapping travel segments.
bool avoidsStrokes(const Point& start, const Point& end, const QVector<Point>& waypoints,
                   const QVector<Stroke>& strokes, double endpoint_roundoff = 0) {
    QVector<Point> route {start};
    route.append(waypoints);
    route.push_back(end);
    for (int i = 1; i < route.size(); ++i) {
        const Point& a = route[i - 1];
        const Point& b = route[i];
        if (a.z() != start.z() || b.z() != start.z()) { return false; }
        for (const auto& stroke : strokes) {
            const double dx = b.x() - a.x(), dy = b.y() - a.y();
            const double ex = stroke.end.x() - stroke.start.x(), ey = stroke.end.y() - stroke.start.y();
            const double ax = stroke.start.x() - a.x(), ay = stroke.start.y() - a.y();
            const double denominator = cross(dx, dy, ex, ey);
            if (std::abs(denominator) < 1.0e-8) {
                if (std::abs(cross(ax, ay, dx, dy)) < 1.0e-8) {
                    const double length_squared = dx * dx + dy * dy;
                    double low                  = (ax * dx + ay * dy) / length_squared;
                    double high = ((stroke.end.x() - a.x()) * dx + (stroke.end.y() - a.y()) * dy) / length_squared;
                    if (low > high) { std::swap(low, high); }
                    if (std::min(1.0, high) - std::max(0.0, low) > 1.0e-8) { return false; }
                }
                continue;
            }
            const double t = cross(ax, ay, ex, ey) / denominator;
            const double u = cross(ax, ay, dx, dy) / denominator;
            if (t < -1.0e-8 || t > 1 + 1.0e-8 || u < -1.0e-8 || u > 1 + 1.0e-8) { continue; }
            if (i == 1 && std::abs(t) < 1.0e-8) { continue; }
            if (i + 1 == route.size() && std::abs(t - 1) < 1.0e-8) { continue; }
            const double length = std::hypot(dx, dy);
            if (i == 1 && t * length <= endpoint_roundoff) { continue; }
            if (i + 1 == route.size() && (1 - t) * length <= endpoint_roundoff) { continue; }
            return false;
        }
    }
    return true;
}

double pointSegmentDistance(const Point& point, const Point& a, const Point& b) {
    const double dx = b.x() - a.x(), dy = b.y() - a.y();
    const double t = std::clamp(((point.x() - a.x()) * dx + (point.y() - a.y()) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
    return std::hypot(point.x() - a.x() - t * dx, point.y() - a.y() - t * dy);
}

//! \brief Check complete interior legs; the immutable start/end require separate clearance transitions.
bool interiorClearance(const QVector<Point>& waypoints, const QVector<Stroke>& strokes, double clearance) {
    if (waypoints.size() < 2) return false;
    for (int i = 1; i < waypoints.size(); ++i) {
        const Point& a = waypoints[i - 1];
        const Point& b = waypoints[i];
        if (a.x() == b.x() && a.y() == b.y()) return false;
        for (const auto& stroke : strokes) {
            const double distance = std::min(
                {pointSegmentDistance(a, stroke.start, stroke.end), pointSegmentDistance(b, stroke.start, stroke.end),
                 pointSegmentDistance(stroke.start, a, b), pointSegmentDistance(stroke.end, a, b)});
            if (distance + 0.5 < clearance) return false;
        }
    }
    return true;
}

bool turnsConsistently(const QVector<Point>& waypoints) {
    int turn_direction = 0;
    for (int i = 2; i < waypoints.size(); ++i) {
        const double ax   = waypoints[i - 1].x() - waypoints[i - 2].x();
        const double ay   = waypoints[i - 1].y() - waypoints[i - 2].y();
        const double bx   = waypoints[i].x() - waypoints[i - 1].x();
        const double by   = waypoints[i].y() - waypoints[i - 1].y();
        const double turn = std::atan2(cross(ax, ay, bx, by), ax * bx + ay * by);
        if (std::abs(turn) < 1.0e-4) continue;
        if (std::abs(turn) > ORNL::pi() / 2.0) return false;
        const int direction = turn > 0 ? 1 : -1;
        if (turn_direction != 0 && turn_direction != direction) return false;
        turn_direction = direction;
    }
    return true;
}

QVector<Stroke> closedLoop(const QVector<Point>& points) {
    QVector<Stroke> strokes;
    for (int i = 0; i < points.size(); ++i) { strokes.push_back({points[i], points[(i + 1) % points.size()]}); }
    return strokes;
}

TravelRouter routerFor(const QVector<Stroke>& strokes, double width = 200) {
    TravelRouter router;
    for (const auto& stroke : strokes) { router.addObstacle(stroke.start, stroke.end, ORNL::Distance(width)); }
    return router;
}

bool followsCompletedPerimeterToInset() {
    const QVector<Stroke> hole {{{0, 0, 20}, {12000, 0, 20}},
                                {{12000, 0, 20}, {12000, 10000, 20}},
                                {{12000, 10000, 20}, {0, 10000, 20}},
                                {{0, 10000, 20}, {0, 0, 20}}};
    const Point start(0, 3000, 20), end(12400, 0, 20);
    auto router      = routerFor(hole, 400);
    const auto route = router.routeOnPerimeter(start, end, 400);
    bool passed      = ORNL::Testing::expect(
        route && route->size() == 2 && route->at(0) == Point(0, 0, 20) && route->at(1) == Point(12000, 0, 20),
        "Perimeter-to-inset travel must follow the printed hole walls before attaching.");
    const auto longer = router.routeOnPerimeter(start, end, 400, [](const Point& p) { return p.x() + p.y() >= 1000; });
    passed &=
        ORNL::Testing::expect(longer && longer->contains(Point(0, 10000, 20)) && !longer->contains(Point(0, 0, 20)),
                              "A supported route must try the legal direction around the contour.");
    passed &=
        ORNL::Testing::expect(!router.routeOnPerimeter(start, end, 400, [](const Point& p) { return p.y() >= 1000; }),
                              "A supported route must respect limits on its inserted points.");
    passed &=
        ORNL::Testing::expect(!router.routeOnPerimeter(Point(1000, 3000, 20), end, 400) &&
                                  !router.routeOnPerimeter(start, Point(14000, 0, 20), 400) &&
                                  !router.routeOnPerimeter(start, Point(12400, 0, 22), 400),
                              "Supported routing needs a start on printed material and a nearby planar destination.");
    const auto straight = router.routeOnPerimeter(Point(0, 8000, 20), start, 400);
    passed &= ORNL::Testing::expect(straight && straight->isEmpty(),
                                    "Travel already over a straight printed wall must remain direct.");
    passed &= ORNL::Testing::expect(router.routeOnPerimeter(start, Point(12400, -400, 20), 400).has_value(),
                                    "A mitered inset corner can lie one bead spacing away in both axes.");
    QVector<Stroke> reversed;
    for (auto i = hole.crbegin(); i != hole.crend(); ++i) reversed.push_back({i->end, i->start});
    passed &= ORNL::Testing::expect(routerFor(reversed, 400).routeOnPerimeter(start, end, 400) == route,
                                    "Contour traversal must work with either print direction.");
    const auto open = routerFor(hole.mid(1), 400).routeOnPerimeter(start, end, 400);
    passed &=
        ORNL::Testing::expect(open && open->contains(Point(0, 10000, 20)) && open->contains(Point(12000, 10000, 20)) &&
                                  !open->contains(Point(0, 0, 20)),
                              "An unfinished contour must follow its deposited side without closing an open gap.");
    TravelRouter varying_width;
    for (int i = 0; i < hole.size(); ++i) varying_width.addObstacle(hole[i].start, hole[i].end, i == 0 ? 400 : 300);
    passed &= ORNL::Testing::expect(varying_width.routeOnPerimeter(start, end, 400).has_value(),
                                    "Width changes must not disconnect a printed contour.");
    passed &= ORNL::Testing::expect(router.routeOnPerimeter(start, Point(12600, 5000, 20), 800).has_value() &&
                                        !router.routeOnPerimeter(start, Point(12700, 5000, 20), 800),
                                    "Inset attachment spacing must use half of each actual bead width.");
    const QVector<Stroke> open_u {{{-1000, 1000, 20}, {-1000, -1000, 20}},
                                  {{-1000, -1000, 20}, {1000, -1000, 20}},
                                  {{1000, -1000, 20}, {1000, 1000, 20}}};
    const auto alternative =
        routerFor(open_u, 200).routeOnPerimeter(Point(-1000, 1000, 20), Point(100, 500, 20), 2400, [](const Point& p) {
            return p.y() >= 0;
        });
    passed &= ORNL::Testing::expect(alternative && alternative->contains(Point(-1000, 500, 20)),
                                    "Try another inset attachment when the nearest requires an illegal contour walk.");
    router.addObstacle(Point(12200, -1000, 20), Point(12200, 1000, 20), 100);
    passed &= ORNL::Testing::expect(!router.routeOnPerimeter(start, end, 400),
                                    "The short inset attachment must not cut through another completed wall.");
    return passed;
}

bool constrainedRoutesReportOutcomes() {
    using Outcome   = TravelRouter::Outcome;
    Outcome outcome = Outcome::kUnroutable;
    bool passed     = true;
    const Point start(-2000, 0, 20), end(2000, 0, 20);
    const QVector<Stroke> asymmetric {{Point(0, -1500, 20), Point(0, 500, 20)}};
    const auto lower_bed = [](const Point& point) { return point.y() >= -2000 && point.y() <= 400; };
    const auto detour    = routerFor(asymmetric).route(start, end, 0, lower_bed, &outcome);
    passed &= ORNL::Testing::expect(
        outcome == Outcome::kDetour && !detour.isEmpty() && std::all_of(detour.begin(), detour.end(), lower_bed) &&
            avoidsStrokes(start, end, detour, asymmetric) &&
            std::any_of(detour.begin(), detour.end(), [](const Point& point) { return point.y() < -1500; }),
        "A forbidden short detour must use the longer legal contour side.");

    const QVector<Stroke> wall {{Point(0, -1000, 20), Point(0, 1000, 20)}};
    const auto tight_bed = [](const Point& point) { return std::abs(point.y()) <= 1200; };
    const auto tighter   = routerFor(wall).route(start, end, ORNL::Distance(350), tight_bed, &outcome);
    passed &= ORNL::Testing::expect(
        outcome == Outcome::kReducedClearance && !tighter.isEmpty() &&
            std::all_of(tighter.begin(), tighter.end(), tight_bed) && avoidsStrokes(start, end, tighter, wall),
        "A reduced-clearance retry must retain the travel limits and report the tighter route.");
    const auto narrow_strip   = [](const Point& point) { return point.y() >= 1150 && point.y() <= 1250; };
    const auto tighter_direct = routerFor(wall).route(Point(-2000, 1200, 20), Point(2000, 1200, 20),
                                                      ORNL::Distance(350), narrow_strip, &outcome);
    passed &= ORNL::Testing::expect(tighter_direct.isEmpty() && outcome == Outcome::kReducedClearance,
                                    "A reduced-clearance direct retry must still report the spacing reduction.");
    const auto blocked_bed = [](const Point& point) { return std::abs(point.y()) <= 1050; };
    const auto blocked     = routerFor(wall).route(start, end, ORNL::Distance(350), blocked_bed, &outcome);
    passed &= ORNL::Testing::expect(blocked.isEmpty() && outcome == Outcome::kTravelLimit,
                                    "A route blocked by the bed must report a travel-limit fallback.");

    // A tilted slicing plane maps planar y to printer z; the nearer bottom detour would go below the build volume.
    const auto tilted_volume = [](const Point& point) {
        const double printer_z = 250.0 + 0.5 * point.y();
        return printer_z >= 0 && printer_z <= 1500;
    };
    const QVector<Stroke> tilted_wall {{Point(0, -500, 20), Point(0, 1500, 20)}};
    const auto tilted = routerFor(tilted_wall).route(start, end, 0, tilted_volume, &outcome);
    passed &= ORNL::Testing::expect(
        outcome == Outcome::kDetour && !tilted.isEmpty() && std::all_of(tilted.begin(), tilted.end(), tilted_volume) &&
            avoidsStrokes(start, end, tilted, tilted_wall),
        "A tilted-frame constraint must reject the lower side and retain an in-volume detour.");

    const auto square =
        closedLoop({Point(-1000, -1000, 20), Point(1000, -1000, 20), Point(1000, 1000, 20), Point(-1000, 1000, 20)});
    const auto impossible = routerFor(square).route(Point(0, 0, 20), end, 0, {}, &outcome);
    passed &= ORNL::Testing::expect(impossible.isEmpty() && outcome == Outcome::kUnroutable,
                                    "Closed-wall topology must be distinguished from a clear direct travel.");
    const auto clear = routerFor(wall).route(Point(-2000, 2000, 20), Point(2000, 2000, 20), 0, {}, &outcome);
    passed &= ORNL::Testing::expect(clear.isEmpty() && outcome == Outcome::kClear,
                                    "Unobstructed direct travel must report clear.");
    const auto non_planar = routerFor(wall).route(start, Point(2000, 0, 22), 0, {}, &outcome);
    passed &= ORNL::Testing::expect(non_planar.isEmpty() && outcome == Outcome::kNonPlanar,
                                    "A non-planar travel must report that it was not routed.");
    const auto empty = TravelRouter().route(start, end, 0, [](const Point&) { return false; }, &outcome);
    passed &=
        ORNL::Testing::expect(empty.isEmpty() && outcome == Outcome::kClear && start.x() == -2000 && end.x() == 2000,
                              "Existing direct-travel endpoints must not be moved or rejected by waypoint limits.");
    return passed;
}
}  // namespace

int main() {
    bool passed = true;
    passed &= followsCompletedPerimeterToInset();
    passed &= constrainedRoutesReportOutcomes();
    const Point start(-2000.0f, 0.0f, 20.0f), end(2000.0f, 0.0f, 20.0f);
    passed &= ORNL::Testing::expect(TravelRouter().route(start, end).isEmpty(),
                                    "An empty layer must leave direct travel unchanged.");
    const QVector<Stroke> open_strokes {{Point(0.0f, -1000.0f, 20.0f), Point(0.0f, 1000.0f, 20.0f)}};
    auto open_router       = routerFor(open_strokes);
    const auto open_detour = open_router.route(start, end);
    passed &= ORNL::Testing::expect(!open_detour.isEmpty() && avoidsStrokes(start, end, open_detour, open_strokes),
                                    "Travel must detour around an open perimeter, preserving its endpoints and plane.");
    for (const auto& point : open_detour) {
        const double separation = std::hypot(point.x(), std::max(0.0, std::abs(point.y()) - 1000.0));
        passed &= ORNL::Testing::expect(separation >= 100.0,
                                        "Open-perimeter detours must clear the bead half-width at its endpoints.");
    }
    const Point near_wall(-10.0f, 0.0f, 20.0f), far_end(1000000000.0f, 0.0f, 20.0f);
    const auto long_detour = open_router.route(near_wall, far_end);
    passed &=
        ORNL::Testing::expect(!long_detour.isEmpty() && avoidsStrokes(near_wall, far_end, long_detour, open_strokes),
                              "Long travels must not widen the original endpoint's contact allowance.");
    const Point clear_start(-2000.0f, 2000.0f, 20.0f), clear_end(2000.0f, 2000.0f, 20.0f);
    passed &= ORNL::Testing::expect(open_router.route(clear_start, clear_end).isEmpty(),
                                    "Unobstructed travel must remain direct.");
    passed &=
        ORNL::Testing::expect(open_router.route(start, start).isEmpty(), "Zero-length travel must remain unchanged.");
    passed &= ORNL::Testing::expect(open_router.route(start, Point(2000.0f, 0.0f, 22.0f)).isEmpty(),
                                    "Travel between different planes must remain unchanged.");
    TravelRouter rounded_router;
    rounded_router.addObstacle(Point(0.0f, -1000.0f, 20.01f), Point(0.0f, 1000.0f, 20.02f), ORNL::Distance(200));
    passed &= ORNL::Testing::expect(!rounded_router.route(start, Point(2000.0f, 0.0f, 20.03f)).isEmpty(),
                                    "Submicron plane drift after a layer rotation must not disable avoidance.");

    const auto square        = closedLoop({Point(-1000.0f, -1000.0f, 20.0f), Point(1000.0f, -1000.0f, 20.0f),
                                           Point(1000.0f, 1000.0f, 20.0f), Point(-1000.0f, 1000.0f, 20.0f)});
    auto square_router       = routerFor(square);
    const auto square_detour = square_router.route(start, end);
    passed &= ORNL::Testing::expect(!square_detour.isEmpty() && avoidsStrokes(start, end, square_detour, square),
                                    "Exterior travel must route around a closed perimeter without vertex tunneling.");
    const auto square_standoff = square_router.route(start, end, ORNL::Distance(350));
    passed &= ORNL::Testing::expect(
        !square_standoff.isEmpty() && avoidsStrokes(start, end, square_standoff, square) &&
            interiorClearance(square_standoff, square, 450),
        "Standoff must add the requested clear space outside the bead edge without cutting offset corners.");
    const Point inside(0.0f, 0.0f, 20.0f);
    passed &= ORNL::Testing::expect(square_router.route(inside, Point(2000.0f, 2000.0f, 20.0f)).isEmpty(),
                                    "A closed wall cannot be escaped through an exact perimeter vertex.");
    passed &= ORNL::Testing::expect(square_router.route(inside, end).isEmpty(),
                                    "An impossible inside-to-outside travel must fall back to the original motion.");
    const Point wall(-1000.0f, 0.0f, 20.0f);
    passed &=
        ORNL::Testing::expect(square_router.route(wall, start).isEmpty() && square_router.route(wall, inside).isEmpty(),
                              "Travel must permit unavoidable contact at an endpoint on a printed perimeter.");
    const auto from_wall = square_router.route(wall, end);
    passed &= ORNL::Testing::expect(!from_wall.isEmpty() && avoidsStrokes(wall, end, from_wall, square),
                                    "Endpoint contact must not permit crossing the opposite side of a closed wall.");
    const auto along_wall = square_router.route(Point(-1000.0f, -500.0f, 20.0f), wall);
    passed &= ORNL::Testing::expect(!along_wall.isEmpty(),
                                    "Endpoint exceptions must not allow travel to overlap a completed centerline.");

    // At machine coordinates, interpolation can round a wipe endpoint slightly inside its diagonal centerline.
    const auto translated_hex =
        closedLoop({Point(3246214.0f, 1079500.0f, 20.0f), Point(3147107.0f, 1251158.375f, 20.0f),
                    Point(2948893.0f, 1251158.375f, 20.0f), Point(2849786.0f, 1079500.0f, 20.0f),
                    Point(2948893.0f, 907841.625f, 20.0f), Point(3147107.0f, 907841.625f, 20.0f)});
    const auto translated_router = routerFor(translated_hex, 8636);
    const Point hex_start = translated_hex[3].start, hex_end = translated_hex[3].end;
    const float wipe_fraction = 76199.99715238812 / hex_start.distance(hex_end)();
    const Point wipe_end((1.0 - wipe_fraction) * hex_start.x() + wipe_fraction * hex_end.x(),
                         (1.0 - wipe_fraction) * hex_start.y() + wipe_fraction * hex_end.y(), 20.0f);
    const Point other_island(3299786.0f, 1079500.0f, 20.0f);
    const auto from_rounded_wipe = translated_router.route(wipe_end, other_island);
    passed &= ORNL::Testing::expect(
        !from_rounded_wipe.isEmpty() && avoidsStrokes(wipe_end, other_island, from_rounded_wipe, translated_hex, 1.0),
        "A float-interpolated diagonal wipe endpoint must permit departure around the completed perimeter.");
    const auto to_rounded_wipe = translated_router.route(other_island, wipe_end);
    passed &= ORNL::Testing::expect(
        !to_rounded_wipe.isEmpty() && avoidsStrokes(other_island, wipe_end, to_rounded_wipe, translated_hex, 1.0),
        "A float-interpolated diagonal wipe endpoint must permit arrival around the completed perimeter.");
    const auto bounded_left = [minimum_x = wipe_end.x() - 1000.0f](const Point& point) {
        return point.x() >= minimum_x;
    };
    TravelRouter::Outcome projection_outcome;
    const auto constrained_wipe = translated_router.route(wipe_end, other_island, 0, bounded_left, &projection_outcome);
    passed &=
        ORNL::Testing::expect(!constrained_wipe.isEmpty() && projection_outcome == TravelRouter::Outcome::kDetour &&
                                  std::all_of(constrained_wipe.begin(), constrained_wipe.end(), bounded_left) &&
                                  avoidsStrokes(wipe_end, other_island, constrained_wipe, translated_hex, 1.0),
                              "A forbidden nearest endpoint projection must leave farther legal exits available.");
    const auto spaced_from_wipe = translated_router.route(wipe_end, other_island, ORNL::Distance(8636));
    passed &= ORNL::Testing::expect(
        !spaced_from_wipe.isEmpty() && avoidsStrokes(wipe_end, other_island, spaced_from_wipe, translated_hex, 1.0) &&
            interiorClearance(spaced_from_wipe, translated_hex, 12954),
        "A rounded diagonal wipe endpoint must transition to a full bead-width standoff beyond the printed bead.");
    passed &= ORNL::Testing::expect(
        spaced_from_wipe.size() <= 6 && turnsConsistently(spaced_from_wipe),
        "A convex hexagon detour must not contain tiny reversals or multiple jagged corner motions.");
    const auto spaced_to_wipe = translated_router.route(other_island, wipe_end, ORNL::Distance(8636));
    passed &= ORNL::Testing::expect(
        !spaced_to_wipe.isEmpty() && avoidsStrokes(other_island, wipe_end, spaced_to_wipe, translated_hex, 1.0) &&
            interiorClearance(spaced_to_wipe, translated_hex, 12954),
        "Arrival at a rounded diagonal wipe endpoint must preserve full standoff until its endpoint transition.");
    const Point truly_inside(wipe_end.x() + 10.0f, wipe_end.y() + 10.0f, 20.0f);
    passed &= ORNL::Testing::expect(translated_router.route(truly_inside, other_island).isEmpty(),
                                    "Endpoint roundoff tolerance must not allow escape from inside a closed wall.");

    const auto thin_triangle = closedLoop({Point(1376.4365234375f, 2825.85595703125f, 20.0f),
                                           Point(-1444.55395507812f, -1488.82836914062f, 20.0f),
                                           Point(-2430.73657226562f, -2750.88696289062f, 20.0f)});
    const Point triangle_start(371.01025390625f, 2717.89501953125f, 20.0f);
    const Point triangle_end(1853.05859375f, -508.48388671875f, 20.0f);
    const auto triangle_detour = routerFor(thin_triangle, 20).route(triangle_start, triangle_end);
    passed &= ORNL::Testing::expect(
        !triangle_detour.isEmpty() && avoidsStrokes(triangle_start, triangle_end, triangle_detour, thin_triangle),
        "A near-reversal corner must not let a detour clip the tip of a thin closed perimeter.");

    const auto concave =
        closedLoop({Point(0.0f, 0.0f, 20.0f), Point(4000.0f, 0.0f, 20.0f), Point(4000.0f, 4000.0f, 20.0f),
                    Point(3000.0f, 4000.0f, 20.0f), Point(3000.0f, 1000.0f, 20.0f), Point(1000.0f, 1000.0f, 20.0f),
                    Point(1000.0f, 4000.0f, 20.0f), Point(0.0f, 4000.0f, 20.0f)});
    const Point arm_start(500.0f, 3500.0f, 20.0f), arm_end(3500.0f, 3500.0f, 20.0f);
    const auto concave_detour = routerFor(concave).route(arm_start, arm_end);
    passed &=
        ORNL::Testing::expect(!concave_detour.isEmpty() && avoidsStrokes(arm_start, arm_end, concave_detour, concave),
                              "Travel inside a concave perimeter must stay connected through its interior.");

    const auto narrow =
        closedLoop({Point(0.0f, 0.0f, 20.0f), Point(400.0f, 0.0f, 20.0f), Point(400.0f, 400.0f, 20.0f),
                    Point(210.0f, 400.0f, 20.0f), Point(210.0f, 30.0f, 20.0f), Point(190.0f, 30.0f, 20.0f),
                    Point(190.0f, 400.0f, 20.0f), Point(0.0f, 400.0f, 20.0f)});
    const Point narrow_start(100.0f, 350.0f, 20.0f), narrow_end(300.0f, 350.0f, 20.0f);
    const auto narrow_detour = routerFor(narrow, 10).route(narrow_start, narrow_end);
    passed &= ORNL::Testing::expect(
        !narrow_detour.isEmpty() && avoidsStrokes(narrow_start, narrow_end, narrow_detour, narrow),
        "A narrow passage wider than the bead must remain available for travel.");
    const auto narrow_standoff = routerFor(narrow, 10).route(narrow_start, narrow_end, ORNL::Distance(30));
    passed &= ORNL::Testing::expect(
        !narrow_standoff.isEmpty() && avoidsStrokes(narrow_start, narrow_end, narrow_standoff, narrow),
        "When full standoff closes a narrow passage, routing must retain the tighter crossing-free detour.");

    QVector<Point> dense_points;
    for (int i = 0; i < 128; ++i) {
        const double angle = i * 2.0 * ORNL::pi() / 128;
        dense_points.push_back(Point(100000.0 * std::cos(angle), 100000.0 * std::sin(angle), 20.0f));
    }
    const auto dense = closedLoop(dense_points);
    const Point dense_start(-200000.0f, 0.0f, 20.0f), dense_end(200000.0f, 0.0f, 20.0f);
    const auto dense_detour = routerFor(dense).route(dense_start, dense_end);
    passed &=
        ORNL::Testing::expect(!dense_detour.isEmpty() && avoidsStrokes(dense_start, dense_end, dense_detour, dense),
                              "A dense smooth contour must receive a continuous crossing-free detour.");
    const Point dense_wall     = dense_points[43];
    const auto dense_from_wall = routerFor(dense).route(dense_wall, dense_end);
    passed &= ORNL::Testing::expect(
        !dense_from_wall.isEmpty() && avoidsStrokes(dense_wall, dense_end, dense_from_wall, dense),
        "Endpoint projection on a dense contour must tolerate float rounding.");

    QVector<Stroke> multiple = open_strokes;
    multiple.push_back({Point(1500.0f, -2000.0f, 20.0f), Point(1500.0f, 2000.0f, 20.0f)});
    const auto multiple_detour = routerFor(multiple).route(start, Point(3000.0f, 0.0f, 20.0f));
    passed &= ORNL::Testing::expect(
        !multiple_detour.isEmpty() && avoidsStrokes(start, Point(3000.0f, 0.0f, 20.0f), multiple_detour, multiple),
        "Every detour leg must avoid all completed perimeters.");
    passed &= ORNL::Testing::expect(!routerFor(open_strokes, 0).route(start, end).isEmpty(),
                                    "Perimeters without bead-width metadata must still receive clearance.");
    const QVector<Stroke> large_strokes {{Point(100000000.0f, -1000.0f, 20.0f), Point(100000000.0f, 1000.0f, 20.0f)}};
    const Point large_start(99998000.0f, 0.0f, 20.0f), large_end(100002000.0f, 0.0f, 20.0f);
    const auto large_detour = routerFor(large_strokes, 0).route(large_start, large_end);
    passed &= ORNL::Testing::expect(
        !large_detour.isEmpty() && avoidsStrokes(large_start, large_end, large_detour, large_strokes),
        "Float conversion at large coordinates must not collapse perimeter clearance.");
    passed &=
        ORNL::Testing::expect(open_router.route(Point(-2000.0f, 0.0f, 40.0f), Point(2000.0f, 0.0f, 40.0f)).isEmpty(),
                              "Perimeters in another plane must not obstruct the current layer.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
