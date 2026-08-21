#include "slicing/helical_path_rounding.h"

#include <algorithm>
#include <cmath>

#include "geometry/point.h"
#include "geometry/polyline.h"
#include "units/unit.h"

namespace ORNL {
namespace {
constexpr double kRevolutionTolerance = 1.0e-9;
const Distance kMinHelixSegmentLength = 100.0 * micron;

Point pointAtRevolutions(const Point& center, Distance radius, Distance start_z, Distance bead_width,
                         HelicalPathHandedness handedness, Angle start_angle, double revolutions) {
    const double t = revolutions * 2.0 * M_PI;
    const double direction = handedness == HelicalPathHandedness::kLeftHanded ? -1.0 : 1.0;
    const double angle = start_angle() + direction * t;

    return Point(center.x() + radius() * std::cos(angle), center.y() + radius() * std::sin(angle),
                 start_z() + bead_width() * revolutions);
}

double revolutionsAtZ(Distance z, Distance start_z, Distance bead_width) {
    if (bead_width <= 0) {
        return 0.0;
    }

    return (z() - start_z()) / bead_width();
}

bool appendDistinct(Polyline& polyline, const Point& point) {
    if (polyline.isEmpty() || polyline.last() != point) {
        polyline.push_back(point);
        return true;
    }

    return false;
}

QVector<Polyline> filteredResult(const Polyline& polyline, Distance min_path_segment_length) {
    if (polyline.size() < 2 || polyline.length() <= min_path_segment_length) {
        return {};
    }

    return {polyline};
}

Polyline exactIntersectionPrefix(const Polyline& helix, const HelicalPathBoundaryIntersection& intersection) {
    Polyline clipped_helix;
    if (helix.isEmpty()) {
        return clipped_helix;
    }

    const int prefix_end = std::clamp(intersection.segment_end_index, 0, static_cast<int>(helix.size()));
    clipped_helix.reserve(prefix_end + 1);
    for (int i = 0; i < prefix_end; ++i) {
        clipped_helix.push_back(helix[i]);
    }

    appendDistinct(clipped_helix, intersection.point);
    return clipped_helix;
}
} // namespace

Polyline HelicalPathRounding::createHelixForRevolutions(const Point& center, Distance radius, Distance start_z,
                                                        Distance bead_width, HelicalPathHandedness handedness,
                                                        Angle start_angle, double revolutions) {
    Polyline helix;
    if (revolutions <= 0.0 || bead_width <= 0) {
        return helix;
    }

    const double max_t = revolutions * 2.0 * M_PI;
    const double vertical_rise_per_radian = bead_width() / (2.0 * M_PI);
    const double length_per_radian = std::hypot(radius(), vertical_rise_per_radian);
    const Distance target_segment_length =
        bead_width / 2.0 > kMinHelixSegmentLength ? bead_width / 2.0 : kMinHelixSegmentLength;
    const int segments =
        std::clamp(static_cast<int>(std::ceil(max_t * length_per_radian / target_segment_length())), 1, 20000);

    helix.reserve(segments + 1);
    for (int i = 0; i <= segments; ++i) {
        const double revolutions_at_sample = revolutions * static_cast<double>(i) / static_cast<double>(segments);
        helix.push_back(
            pointAtRevolutions(center, radius, start_z, bead_width, handedness, start_angle, revolutions_at_sample));
    }

    return helix;
}

QVector<Polyline> HelicalPathRounding::clipAtHighestIntersection(
    const Polyline& helix, const QVector<HelicalPathBoundaryIntersection>& intersections, bool has_inside_points,
    bool has_outside_points, const Point& center, Distance radius, Distance start_z, Distance bead_width,
    HelicalPathHandedness handedness, Angle start_angle, HelicalPathZClipRounding rounding,
    Distance min_path_segment_length) {
    if (helix.size() < 2) {
        return {};
    }

    if (intersections.isEmpty()) {
        return has_inside_points && !has_outside_points ? QVector<Polyline> {helix} : QVector<Polyline>();
    }

    const auto highest_intersection =
        std::max_element(intersections.cbegin(), intersections.cend(),
                         [](const HelicalPathBoundaryIntersection& lhs, const HelicalPathBoundaryIntersection& rhs) {
                             return lhs.point.z() < rhs.point.z();
                         });

    if (rounding == HelicalPathZClipRounding::kExactIntersection) {
        return filteredResult(exactIntersectionPrefix(helix, *highest_intersection), min_path_segment_length);
    }

    const double raw_revolutions = revolutionsAtZ(Distance(highest_intersection->point.z()), start_z, bead_width);
    const double rounded_revolutions = rounding == HelicalPathZClipRounding::kCompleteRevolution
                                           ? std::ceil(raw_revolutions - kRevolutionTolerance)
                                           : std::floor(raw_revolutions + kRevolutionTolerance);

    if (rounded_revolutions <= kRevolutionTolerance) {
        return {};
    }

    return filteredResult(
        createHelixForRevolutions(center, radius, start_z, bead_width, handedness, start_angle, rounded_revolutions),
        min_path_segment_length);
}
} // namespace ORNL
