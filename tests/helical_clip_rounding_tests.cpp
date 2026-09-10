#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "slicing/helical_path_rounding.h"
#include "slicing/helical_start_angle.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition) { return true; }

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
    const double angle     = start_angle() + direction * revolutions * 2.0 * M_PI;
    return ORNL::Point(center.x() + radius() * std::cos(angle), center.y() + radius() * std::sin(angle),
                       start_z() + bead_width() * revolutions);
}

int segmentEndIndexForZ(const ORNL::Polyline& helix, ORNL::Distance z) {
    for (int i = 1; i < helix.size(); ++i) {
        if (helix[i].z() >= z()) { return i; }
    }

    return std::max(1, static_cast<int>(helix.size()) - 1);
}

QVector<ORNL::Polyline> roundedAt(double intersection_revolutions, ORNL::HelicalPathZClipRounding rounding,
                                  ORNL::HelicalPathHandedness handedness = ORNL::HelicalPathHandedness::kRightHanded,
                                  ORNL::Angle start_angle                = 0.0 * ORNL::degree) {
    const ORNL::Point center(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Distance radius     = 10.0 * ORNL::mm;
    const ORNL::Distance start_z    = 0.0 * ORNL::mm;
    const ORNL::Distance bead_width = 4.0 * ORNL::mm;
    const ORNL::Polyline helix      = ORNL::HelicalPathRounding::createHelixForRevolutions(
        center, radius, start_z, bead_width, handedness, start_angle, intersection_revolutions);
    const ORNL::Point intersection =
        pointAtRevolutions(center, radius, start_z, bead_width, handedness, start_angle, intersection_revolutions);

    return ORNL::HelicalPathRounding::clipAtHighestIntersection(
        helix,
        QVector<ORNL::HelicalPathBoundaryIntersection> {ORNL::HelicalPathBoundaryIntersection {
            intersection, segmentEndIndexForZ(helix, ORNL::Distance(intersection.z()))}},
        true, true, center, radius, start_z, bead_width, handedness, start_angle, rounding, 10.0 * ORNL::micron);
}

QVector<ORNL::Polyline> roundedWhollyInsideAt(double top_revolutions, ORNL::HelicalPathZClipRounding rounding) {
    const ORNL::Point center(0.0 * ORNL::mm, 0.0 * ORNL::mm, 0.0 * ORNL::mm);
    const ORNL::Distance radius     = 10.0 * ORNL::mm;
    const ORNL::Distance start_z    = 0.0 * ORNL::mm;
    const ORNL::Distance bead_width = 4.0 * ORNL::mm;
    const ORNL::Polyline helix      = ORNL::HelicalPathRounding::createHelixForRevolutions(
        center, radius, start_z, bead_width, ORNL::HelicalPathHandedness::kRightHanded, 0.0 * ORNL::degree,
        top_revolutions);

    return ORNL::HelicalPathRounding::clipAtHighestIntersection(helix, {}, true, false, center, radius, start_z,
                                                                bead_width, ORNL::HelicalPathHandedness::kRightHanded,
                                                                0.0 * ORNL::degree, rounding, 10.0 * ORNL::micron);
}

bool exactRoundingKeepsIntersection() {
    const QVector<ORNL::Polyline> result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kExactIntersection);
    if (result.size() != 1 || result.first().isEmpty()) { return false; }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 0.0 * ORNL::mm) && nearDistance(end.y(), 10.0 * ORNL::mm) &&
           nearDistance(end.z(), 9.0 * ORNL::mm);
}

bool lastFullRoundingStopsAtPreviousRevolution() {
    const QVector<ORNL::Polyline> result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kLastFullRevolution);
    if (result.size() != 1 || result.first().isEmpty()) { return false; }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 10.0 * ORNL::mm) && nearDistance(end.y(), 0.0 * ORNL::mm) &&
           nearDistance(end.z(), 8.0 * ORNL::mm);
}

bool completeRoundingExtendsPastOriginalTopZ() {
    const QVector<ORNL::Polyline> result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kCompleteRevolution);
    if (result.size() != 1 || result.first().isEmpty()) { return false; }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 10.0 * ORNL::mm) && nearDistance(end.y(), 0.0 * ORNL::mm) &&
           nearDistance(end.z(), 12.0 * ORNL::mm);
}

bool lastFullBeforeOneRevolutionOmitsPath() {
    return roundedAt(0.75, ORNL::HelicalPathZClipRounding::kLastFullRevolution).isEmpty();
}

bool fullRevolutionEndpointPreservesStartAngleForBothHandednesses() {
    const ORNL::Angle start_angle              = 90.0 * ORNL::degree;
    const QVector<ORNL::Polyline> right_result = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
                                                           ORNL::HelicalPathHandedness::kRightHanded, start_angle);
    const QVector<ORNL::Polyline> left_result  = roundedAt(2.25, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
                                                           ORNL::HelicalPathHandedness::kLeftHanded, start_angle);

    if (right_result.size() != 1 || left_result.size() != 1 || right_result.first().isEmpty() ||
        left_result.first().isEmpty()) {
        return false;
    }

    const ORNL::Point right_end = right_result.first().last();
    const ORNL::Point left_end  = left_result.first().last();
    return nearDistance(right_end.x(), 0.0 * ORNL::mm) && nearDistance(right_end.y(), 10.0 * ORNL::mm) &&
           nearDistance(left_end.x(), 0.0 * ORNL::mm) && nearDistance(left_end.y(), 10.0 * ORNL::mm) &&
           nearDistance(right_end.z(), 12.0 * ORNL::mm) && nearDistance(left_end.z(), 12.0 * ORNL::mm);
}

bool whollyInsideCompleteRoundsGeneratedTopToNextRevolution() {
    const QVector<ORNL::Polyline> result =
        roundedWhollyInsideAt(2.25, ORNL::HelicalPathZClipRounding::kCompleteRevolution);
    if (result.size() != 1 || result.first().isEmpty()) { return false; }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 10.0 * ORNL::mm) && nearDistance(end.y(), 0.0 * ORNL::mm) &&
           nearDistance(end.z(), 12.0 * ORNL::mm);
}

bool whollyInsideLastFullRoundsGeneratedTopToPreviousRevolution() {
    const QVector<ORNL::Polyline> result =
        roundedWhollyInsideAt(2.25, ORNL::HelicalPathZClipRounding::kLastFullRevolution);
    if (result.size() != 1 || result.first().isEmpty()) { return false; }

    const ORNL::Point end = result.first().last();
    return nearDistance(end.x(), 10.0 * ORNL::mm) && nearDistance(end.y(), 0.0 * ORNL::mm) &&
           nearDistance(end.z(), 8.0 * ORNL::mm);
}

bool directionAwareOffsetFollowsAlternatingCompleteClosestDirection() {
    const ORNL::Angle configured_offset = -12.0 * ORNL::degree;

    const ORNL::Angle first_offset = ORNL::HelicalStartAngle::effectiveOffset(
        configured_offset, 0, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
        ORNL::PathOrderOptimization::kNextClosest);
    const ORNL::Angle second_offset = ORNL::HelicalStartAngle::effectiveOffset(
        configured_offset, 1, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
        ORNL::PathOrderOptimization::kNextClosest);
    const ORNL::Angle third_offset = ORNL::HelicalStartAngle::effectiveOffset(
        configured_offset, 2, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
        ORNL::PathOrderOptimization::kNextClosest);

    return near(first_offset.to(ORNL::degree), -12.0) && near(second_offset.to(ORNL::degree), 12.0) &&
           near(third_offset.to(ORNL::degree), -12.0);
}

bool helicalGeometryStartAngleStaysAtTopDeadCenter() {
    return near(ORNL::HelicalStartAngle::geometricStartAngle().to(ORNL::degree), 90.0);
}

bool directionAwareOffsetRequiresPredictableCompleteClosestDirection() {
    const ORNL::Angle configured_offset = -12.0 * ORNL::degree;

    const ORNL::Angle exact_offset = ORNL::HelicalStartAngle::effectiveOffset(
        configured_offset, 1, ORNL::HelicalPathZClipRounding::kExactIntersection,
        ORNL::PathOrderOptimization::kNextClosest);
    const ORNL::Angle last_full_offset = ORNL::HelicalStartAngle::effectiveOffset(
        configured_offset, 1, ORNL::HelicalPathZClipRounding::kLastFullRevolution,
        ORNL::PathOrderOptimization::kNextClosest);
    const ORNL::Angle farthest_offset = ORNL::HelicalStartAngle::effectiveOffset(
        configured_offset, 1, ORNL::HelicalPathZClipRounding::kCompleteRevolution,
        ORNL::PathOrderOptimization::kNextFarthest);

    return near(exact_offset.to(ORNL::degree), -12.0) && near(last_full_offset.to(ORNL::degree), -12.0) &&
           near(farthest_offset.to(ORNL::degree), -12.0);
}
}  // namespace

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
    passed &= expect(whollyInsideCompleteRoundsGeneratedTopToNextRevolution(),
                     "Expected complete rounding to round a wholly inside helix top to the next full revolution.");
    passed &= expect(whollyInsideLastFullRoundsGeneratedTopToPreviousRevolution(),
                     "Expected last-full rounding to round a wholly inside helix top to the previous full revolution.");
    passed &= expect(directionAwareOffsetFollowsAlternatingCompleteClosestDirection(),
                     "Expected direction-aware helical offset to flip on every other Complete Revolution/Next Closest "
                     "radius pass.");
    passed &= expect(helicalGeometryStartAngleStaysAtTopDeadCenter(),
                     "Expected emitted helical X/Y geometry to start at top dead center.");
    passed &= expect(directionAwareOffsetRequiresPredictableCompleteClosestDirection(),
                     "Expected direction-aware helical offset to keep the configured sign outside Complete "
                     "Revolution/Next Closest.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
