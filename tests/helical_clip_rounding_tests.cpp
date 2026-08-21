#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include <QVector>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "slicing/helical_path_rounding.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

bool near(double actual, double expected, double tolerance = 1.0e-5) {
    return std::abs(actual - expected) <= tolerance;
}

bool nearDistance(float actual, ORNL::Distance expected, double tolerance_mm = 1.0e-5) {
    return near(ORNL::Distance(actual).to(ORNL::mm), expected.to(ORNL::mm), tolerance_mm);
}

ORNL::Point pointAtRevolutions(const ORNL::Point& center, ORNL::Distance radius, ORNL::Distance start_z,
                               ORNL::Distance bead_width, ORNL::HelicalPathHandedness handedness,
                               ORNL::Angle start_angle, double revolutions) {
    const double direction = handedness == ORNL::HelicalPathHandedness::kLeftHanded ? -1.0 : 1.0;
    const double angle = start_angle() + direction * revolutions * 2.0 * M_PI;
    return ORNL::Point(center.x() + radius() * std::cos(angle), center.y() + radius() * std::sin(angle),
                       start_z() + bead_width() * revolutions);
}

int segmentEndIndexForZ(const ORNL::Polyline& helix, ORNL::Distance z) {
    for (int i = 1; i < helix.size(); ++i) {
        if (helix[i].z() >= z()) {
            return i;
        }
    }

    return std::max(1, static_cast<int>(helix.size()) - 1);
}

QVector<ORNL::Polyline> roundedAt(double intersection_revolutions, ORNL::HelicalPathZClipRounding rounding,
                                  ORNL::HelicalPathHandedness handedness = ORNL::HelicalPathHandedness::kRightHanded,
                                  ORNL::Angle start_angle = 0.0 * ORNL::degree) {
    const ORNL::Point center(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Distance radius = 10.0 * ORNL::mm;
    const ORNL::Distance start_z = 0.0 * ORNL::mm;
    const ORNL::Distance bead_width = 4.0 * ORNL::mm;
    const ORNL::Polyline helix = ORNL::HelicalPathRounding::createHelixForRevolutions(
        center, radius, start_z, bead_width, handedness, start_angle, intersection_revolutions);
    const ORNL::Point intersection =
        pointAtRevolutions(center, radius, start_z, bead_width, handedness, start_angle, intersection_revolutions);

    return ORNL::HelicalPathRounding::clipAtHighestIntersection(
        helix,
        QVector<ORNL::HelicalPathBoundaryIntersection> {ORNL::HelicalPathBoundaryIntersection {
            intersection, segmentEndIndexForZ(helix, ORNL::Distance(intersection.z()))}},
        true, true, center, radius, start_z, bead_width, handedness, start_angle, rounding, 10.0 * ORNL::micron);
}

bool exactRoundingKeepsIntersection() {
    const QVector<ORNL::Polyline> result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kExactIntersection);
    if (result.size() != 1 || result.first().isEmpty()) {
        return false;
    }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 0.0 * ORNL::mm) && nearDistance(end.y(), 10.0 * ORNL::mm) &&
           nearDistance(end.z(), 9.0 * ORNL::mm);
}

bool lastFullRoundingStopsAtPreviousRevolution() {
    const QVector<ORNL::Polyline> result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kLastFullRevolution);
    if (result.size() != 1 || result.first().isEmpty()) {
        return false;
    }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 10.0 * ORNL::mm) && nearDistance(end.y(), 0.0 * ORNL::mm) &&
           nearDistance(end.z(), 8.0 * ORNL::mm);
}

bool completeRoundingExtendsPastOriginalTopZ() {
    const QVector<ORNL::Polyline> result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kCompleteRevolution);
    if (result.size() != 1 || result.first().isEmpty()) {
        return false;
    }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 10.0 * ORNL::mm) && nearDistance(end.y(), 0.0 * ORNL::mm) &&
           nearDistance(end.z(), 12.0 * ORNL::mm);
}

bool lastFullBeforeOneRevolutionOmitsPath() {
    return roundedAt(0.75, ORNL::HelicalPathZClipRounding::kLastFullRevolution).isEmpty();
}

bool fullRevolutionEndpointPreservesStartAngleForBothHandednesses() {
    const ORNL::Angle start_angle = 90.0 * ORNL::degree;
    const QVector<ORNL::Polyline> right_result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
                                                           ORNL::HelicalPathHandedness::kRightHanded, start_angle);
    const QVector<ORNL::Polyline> left_result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
                                                          ORNL::HelicalPathHandedness::kLeftHanded, start_angle);

    if (right_result.size() != 1 || left_result.size() != 1 || right_result.first().isEmpty() ||
        left_result.first().isEmpty()) {
        return false;
    }

    const ORNL::Point right_end = right_result.first().last();
    const ORNL::Point left_end = left_result.first().last();
    return nearDistance(right_end.x(), 0.0 * ORNL::mm) && nearDistance(right_end.y(), 10.0 * ORNL::mm) &&
           nearDistance(left_end.x(), 0.0 * ORNL::mm) && nearDistance(left_end.y(), 10.0 * ORNL::mm) &&
           nearDistance(right_end.z(), 12.0 * ORNL::mm) && nearDistance(left_end.z(), 12.0 * ORNL::mm);
}
} // namespace

int main() {
    bool passed = true;

    passed &= expect(exactRoundingKeepsIntersection(), "Expected exact rounding to keep the intersection endpoint.");
    passed &= expect(lastFullRoundingStopsAtPreviousRevolution(),
                     "Expected last-full rounding to stop at the previous complete revolution.");
    passed &= expect(completeRoundingExtendsPastOriginalTopZ(),
                     "Expected complete rounding to extend to the next full revolution.");
    passed &= expect(lastFullBeforeOneRevolutionOmitsPath(),
                     "Expected last-full rounding before one revolution to omit the path.");
    passed &= expect(fullRevolutionEndpointPreservesStartAngleForBothHandednesses(),
                     "Expected full-revolution endpoints to preserve start angle for both handednesses.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
