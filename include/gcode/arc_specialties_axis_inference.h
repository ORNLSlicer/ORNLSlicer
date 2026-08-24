#pragma once

#include <QVector3D>
#include <cmath>
#include <limits>
#include <optional>

#include <qmath.h>

#include "geometry/point.h"

namespace ORNL::ArcSpecialtiesAxisInference {
inline double signedShortestDeltaDegrees(double start_degrees, double end_degrees) {
    double delta_degrees = end_degrees - start_degrees;
    while (delta_degrees > 180.0) { delta_degrees -= 360.0; }
    while (delta_degrees < -180.0) { delta_degrees += 360.0; }

    return delta_degrees;
}

inline double planarDistanceSquared(const Point& a, const Point& b) {
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return (dx * dx) + (dy * dy);
}

inline bool cylindricalAxisFromCpDelta(const QVector3D& start_pos, const QVector3D& end_pos, double start_cp_degrees,
                                       double end_cp_degrees, bool reverse_cp_delta,
                                       const std::optional<Point>& reference_axis, Point& center) {
    const double dx           = end_pos.x() - start_pos.x();
    const double dy           = end_pos.y() - start_pos.y();
    const double chord_length = std::hypot(dx, dy);
    if (chord_length <= std::numeric_limits<double>::epsilon()) { return false; }

    double delta_degrees = signedShortestDeltaDegrees(start_cp_degrees, end_cp_degrees);
    if (reverse_cp_delta) { delta_degrees *= -1.0; }

    const double delta_radians  = qDegreesToRadians(delta_degrees);
    const double half_delta     = std::abs(delta_radians) / 2.0;
    const double tan_half_delta = std::tan(half_delta);
    if (std::abs(tan_half_delta) <= std::numeric_limits<double>::epsilon()) { return false; }

    const double center_offset = chord_length / (2.0 * tan_half_delta);
    const double mid_x         = (start_pos.x() + end_pos.x()) / 2.0;
    const double mid_y         = (start_pos.y() + end_pos.y()) / 2.0;
    const double left_normal_x = -dy / chord_length;
    const double left_normal_y = dx / chord_length;

    const Point positive_center(mid_x + (left_normal_x * center_offset), mid_y + (left_normal_y * center_offset), 0.0);
    const Point negative_center(mid_x - (left_normal_x * center_offset), mid_y - (left_normal_y * center_offset), 0.0);

    if (reference_axis.has_value()) {
        center = planarDistanceSquared(positive_center, *reference_axis) <=
                         planarDistanceSquared(negative_center, *reference_axis)
                     ? positive_center
                     : negative_center;
    }
    else { center = delta_radians >= 0.0 ? positive_center : negative_center; }

    return true;
}
}  // namespace ORNL::ArcSpecialtiesAxisInference
