#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include <QVector>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "geometry/spiral_path.h"
#include "units/unit.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

bool closeTo(double lhs, double rhs) { return std::abs(lhs - rhs) <= 1.0e-4; }

bool expectPoint(const ORNL::Point& point, double x, double y, const std::string& message) {
    return expect(closeTo(point.x(), x) && closeTo(point.y(), y), message);
}

ORNL::Polyline square(double min_x, double min_y, double max_x, double max_y) {
    ORNL::Polyline line;
    line.push_back(ORNL::Point(min_x, min_y, 0.0f));
    line.push_back(ORNL::Point(max_x, min_y, 0.0f));
    line.push_back(ORNL::Point(max_x, max_y, 0.0f));
    line.push_back(ORNL::Point(min_x, max_y, 0.0f));
    return line;
}

ORNL::Polyline rectangleStartingOnLeftEdge(double min_x, double min_y, double max_x, double max_y, double start_y) {
    ORNL::Polyline line;
    line.push_back(ORNL::Point(min_x, start_y, 0.0f));
    line.push_back(ORNL::Point(min_x, min_y, 0.0f));
    line.push_back(ORNL::Point(max_x, min_y, 0.0f));
    line.push_back(ORNL::Point(max_x, max_y, 0.0f));
    line.push_back(ORNL::Point(min_x, max_y, 0.0f));
    return line;
}
} // namespace

int main() {
    bool passed = true;

    const ORNL::Distance bead_width(1.0);

    QVector<ORNL::Polyline> adjacent_loops;
    adjacent_loops.push_back(square(0.0, 0.0, 10.0, 10.0));
    adjacent_loops.push_back(square(1.0, 1.0, 9.0, 9.0));
    QVector<ORNL::Polyline> adjacent_groups = ORNL::SpiralPath::linkClosedPolylineGroups(adjacent_loops, bead_width);

    passed &= expect(adjacent_groups.size() == 1, "Expected adjacent nested loops to remain in one spiral group.");

    QVector<ORNL::Polyline> disjoint_loops;
    disjoint_loops.push_back(square(0.0, 0.0, 10.0, 10.0));
    disjoint_loops.push_back(square(30.0, 0.0, 40.0, 10.0));
    QVector<ORNL::Polyline> disjoint_groups = ORNL::SpiralPath::linkClosedPolylineGroups(disjoint_loops, bead_width);

    passed &= expect(disjoint_groups.size() == 2, "Expected disjoint loops to be split into separate spiral groups.");
    if (disjoint_groups.size() == 2) {
        passed &= expectPoint(disjoint_groups.front().back(), 0.0, 1.0,
                              "Expected disjoint group to end at the rejected connector start.");
        passed &= expectPoint(disjoint_groups.back().front(), 30.0, 0.0,
                              "Expected next disjoint group to keep its existing start vertex.");
        passed &= expect(disjoint_groups.front().back().distance(disjoint_groups.back().front()) > bead_width * 2.0,
                         "Expected rejected connector to exceed the spiral adjacency threshold.");
    }

    QVector<ORNL::Polyline> nested_gap_loops;
    nested_gap_loops.push_back(square(0.0, 0.0, 50.0, 50.0));
    nested_gap_loops.push_back(square(20.0, 20.0, 30.0, 30.0));
    QVector<ORNL::Polyline> nested_gap_groups =
        ORNL::SpiralPath::linkClosedPolylineGroups(nested_gap_loops, bead_width);

    passed &= expect(nested_gap_groups.size() == 2,
                     "Expected nested loops separated by more than the transition width to be split.");
    if (nested_gap_groups.size() == 2) {
        passed &= expectPoint(nested_gap_groups.front().back(), 0.0, 1.0,
                              "Expected rejected nested-gap group to end at the normal final stop.");
    }

    QVector<ORNL::Polyline> separated_nested_loops;
    separated_nested_loops.push_back(square(0.0, 0.0, 12.0, 18.0));
    separated_nested_loops.push_back(square(1.0, 1.0, 11.0, 17.0));
    separated_nested_loops.push_back(square(40.0, 0.0, 52.0, 18.0));
    separated_nested_loops.push_back(square(41.0, 1.0, 51.0, 17.0));
    QVector<ORNL::Polyline> separated_nested_groups =
        ORNL::SpiralPath::linkClosedPolylineGroups(separated_nested_loops, bead_width);

    passed &= expect(separated_nested_groups.size() == 2,
                     "Expected separated nested inset stacks to become two spiral groups.");
    if (separated_nested_groups.size() == 2) {
        passed &= expect(separated_nested_groups.front().size() > 6,
                         "Expected the first inset stack to link into a multi-loop spiral group.");
        passed &= expect(separated_nested_groups.back().size() > 6,
                         "Expected the second inset stack to link into a multi-loop spiral group.");
        passed &= expectPoint(separated_nested_groups.back().front(), 40.0, 0.0,
                              "Expected the second inset stack to keep its existing start vertex.");
    }

    QVector<ORNL::Polyline> edge_start_loops;
    edge_start_loops.push_back(rectangleStartingOnLeftEdge(0.0, 0.0, 12.0, 18.0, 3.06));
    edge_start_loops.push_back(square(0.34, 0.34, 11.66, 17.66));
    QVector<ORNL::Polyline> edge_start_groups =
        ORNL::SpiralPath::linkClosedPolylineGroups(edge_start_loops, ORNL::Distance(0.34));

    passed &= expect(edge_start_groups.size() == 1,
                     "Expected an inset stack with a mid-edge outer start point to stay spiralized.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
