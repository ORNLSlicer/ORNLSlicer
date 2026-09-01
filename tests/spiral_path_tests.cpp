#include <QVector>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "geometry/spiral_path.h"
#include "units/unit.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

bool closeTo(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= 1.0e-4;
}

bool expectPoint(const ORNL::Point& point, double x, double y, const std::string& message) {
    return expect(closeTo(point.x(), x) && closeTo(point.y(), y), message);
}

bool expectForwardFortyFiveConnector(const ORNL::Polyline& loop, const ORNL::Point& connector_start,
                                     const ORNL::Point& connector_end, const std::string& message) {
    const double tangent_x           = loop.front().x() - loop.back().x();
    const double tangent_y           = loop.front().y() - loop.back().y();
    const double connector_x         = connector_end.x() - connector_start.x();
    const double connector_y         = connector_end.y() - connector_start.y();
    const double tangent_length_sq   = (tangent_x * tangent_x) + (tangent_y * tangent_y);
    const double connector_length_sq = (connector_x * connector_x) + (connector_y * connector_y);

    if (tangent_length_sq <= 0.0 || connector_length_sq <= 0.0) { return expect(false, message); }

    const double normalized_dot =
        ((tangent_x * connector_x) + (tangent_y * connector_y)) / std::sqrt(tangent_length_sq * connector_length_sq);
    return expect(closeTo(normalized_dot, std::sqrt(0.5)), message);
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

ORNL::Polyline smoothClosingOuterLoop() {
    ORNL::Polyline line;
    line.push_back(ORNL::Point(0.0, 0.0, 0.0f));
    line.push_back(ORNL::Point(100.0, 0.0, 0.0f));
    line.push_back(ORNL::Point(100.0, 100.0, 0.0f));
    line.push_back(ORNL::Point(-70.0, 80.0, 0.0f));
    line.push_back(ORNL::Point(-10.0, 10.0, 0.0f));
    return line;
}

ORNL::Polyline smoothClosingInnerLoop() {
    ORNL::Polyline line;
    line.push_back(ORNL::Point(-5.0, 5.0, 0.0f));
    line.push_back(ORNL::Point(10.0, 20.0, 0.0f));
    line.push_back(ORNL::Point(85.0, 25.0, 0.0f));
    line.push_back(ORNL::Point(85.0, 85.0, 0.0f));
    line.push_back(ORNL::Point(-35.0, 55.0, 0.0f));
    line.push_back(ORNL::Point(-15.0, 16.0, 0.0f));
    return line;
}

ORNL::Polyline forwardPreferenceOuterLoop() {
    ORNL::Polyline line;
    line.push_back(ORNL::Point(0.0, 0.0, 0.0f));
    line.push_back(ORNL::Point(100.0, 0.0, 0.0f));
    line.push_back(ORNL::Point(100.0, 100.0, 0.0f));
    line.push_back(ORNL::Point(-100.0, 100.0, 0.0f));
    line.push_back(ORNL::Point(-100.0, 0.0, 0.0f));
    line.push_back(ORNL::Point(-10.0, 0.0, 0.0f));
    return line;
}

ORNL::Polyline forwardPreferenceInnerLoop() {
    ORNL::Polyline line;
    line.push_back(ORNL::Point(-2.0, 2.0, 0.0f));
    line.push_back(ORNL::Point(5.0, 5.0, 0.0f));
    line.push_back(ORNL::Point(80.0, 10.0, 0.0f));
    line.push_back(ORNL::Point(80.0, 80.0, 0.0f));
    line.push_back(ORNL::Point(-80.0, 80.0, 0.0f));
    line.push_back(ORNL::Point(-10.0, 10.0, 0.0f));
    return line;
}
}  // namespace

int main() {
    bool passed = true;

    const ORNL::Distance bead_width(1.0);

    QVector<ORNL::Polyline> adjacent_loops;
    adjacent_loops.push_back(square(0.0, 0.0, 10.0, 10.0));
    adjacent_loops.push_back(square(1.0, 1.0, 9.0, 9.0));
    QVector<ORNL::Polyline> adjacent_groups = ORNL::SpiralPath::linkClosedPolylineGroups(adjacent_loops, bead_width);

    passed &= expect(adjacent_groups.size() == 1, "Expected adjacent nested loops to remain in one spiral group.");

    QVector<ORNL::Polyline> complete_adjacent_groups =
        ORNL::SpiralPath::linkClosedPolylineGroups(adjacent_loops, bead_width, true);

    passed &= expect(complete_adjacent_groups.size() == 1,
                     "Expected complete-before-connecting nested loops to remain in one spiral group.");
    if (complete_adjacent_groups.size() == 1) {
        const ORNL::Polyline& complete_group = complete_adjacent_groups.front();
        passed &= expect(complete_group.size() >= 6,
                         "Expected complete-before-connecting spiral group to include both loops and connector.");
        if (complete_group.size() >= 6) {
            passed &= expectPoint(complete_group[4], 0.0, 0.0,
                                  "Expected first loop to close before connecting to the next loop.");
            passed &= expectPoint(complete_group[5], 1.0, 1.0,
                                  "Expected completed loop to connect to the next loop at its nearest corner.");
            passed &= expect(closeTo(std::abs(complete_group[5].x() - complete_group[4].x()),
                                     std::abs(complete_group[5].y() - complete_group[4].y())),
                             "Expected completed-loop connector to move at a 45-degree angle.");
        }
        passed &= expectPoint(complete_group.back(), 1.0, 1.0,
                              "Expected final loop to close when complete-before-connecting is enabled.");
    }

    QVector<ORNL::Polyline> smooth_completed_loops;
    smooth_completed_loops.push_back(smoothClosingOuterLoop());
    smooth_completed_loops.push_back(smoothClosingInnerLoop());
    QVector<ORNL::Polyline> smooth_completed_groups =
        ORNL::SpiralPath::linkClosedPolylineGroups(smooth_completed_loops, ORNL::Distance(5.0), true);

    passed &= expect(smooth_completed_groups.size() == 1,
                     "Expected completed smooth-closing loops to connect to the nearest next loop point.");
    if (smooth_completed_groups.size() == 1) {
        const ORNL::Polyline& complete_group = smooth_completed_groups.front();
        passed &= expect(complete_group.size() >= 7,
                         "Expected completed smooth-closing loop to include a printed connector.");
        if (complete_group.size() >= 7) {
            passed &=
                expectPoint(complete_group[5], 0.0, 0.0, "Expected smooth-closing loop to complete before connecting.");
            passed &= expectPoint(complete_group[6], -5.0, 5.0,
                                  "Expected smooth-closing loop to fall back to the nearest connectable point.");
        }
    }

    QVector<ORNL::Polyline> forward_preference_loops;
    forward_preference_loops.push_back(forwardPreferenceOuterLoop());
    forward_preference_loops.push_back(forwardPreferenceInnerLoop());
    QVector<ORNL::Polyline> forward_preference_groups =
        ORNL::SpiralPath::linkClosedPolylineGroups(forward_preference_loops, ORNL::Distance(5.0), true);

    passed &= expect(forward_preference_groups.size() == 1,
                     "Expected completed loops with forward and backward diagonals to remain connected.");
    if (forward_preference_groups.size() == 1) {
        const ORNL::Polyline& complete_group = forward_preference_groups.front();
        passed &=
            expect(complete_group.size() >= 8, "Expected forward-preference loop to include a printed connector.");
        if (complete_group.size() >= 8) {
            passed &= expectPoint(complete_group[6], 0.0, 0.0,
                                  "Expected forward-preference loop to complete before connecting.");
            passed &= expectPoint(complete_group[7], 5.0, 5.0,
                                  "Expected forward-preference loop to skip the shorter backward diagonal.");
            passed &= expectForwardFortyFiveConnector(
                forward_preference_loops.front(), complete_group[6], complete_group[7],
                "Expected completed-loop connector to move forward at a 45-degree angle.");
        }
    }

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

    QVector<ORNL::Polyline> complete_disjoint_groups =
        ORNL::SpiralPath::linkClosedPolylineGroups(disjoint_loops, bead_width, true);

    passed &= expect(complete_disjoint_groups.size() == 2,
                     "Expected complete-before-connecting disjoint loops to remain split.");
    if (complete_disjoint_groups.size() == 2) {
        passed &= expectPoint(complete_disjoint_groups.front().back(), 0.0, 0.0,
                              "Expected rejected complete-before-connecting group to close before travel.");
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
