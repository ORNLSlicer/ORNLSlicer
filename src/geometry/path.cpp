#include "geometry/path.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <qcontainerfwd.h>
#include <qlist.h>
#include <qquaternion.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/polyline.h"
#include "geometry/segment_base.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
constexpr double kNumericalTolerance     = 1.0e-6;
constexpr double kPlanarZTolerance       = 1.0;
constexpr double kPi                     = 3.14159265358979323846;
constexpr double kMinimumArcSweep        = 5.0 * kPi / 180.0;
constexpr double kMaximumArcSweep        = kPi;
constexpr int kDefaultMinimumArcSegments = 3;

struct CircleFit {
    Point center;
    double radius = 0.0;
    double sweep  = 0.0;
    bool ccw      = false;
};

enum class ArcFitResult { kInvalid, kNeedsMoreSweep, kValid };

double normalizedAngleDelta(double delta) {
    while (delta <= -kPi) delta += 2.0 * kPi;
    while (delta > kPi) delta -= 2.0 * kPi;
    return delta;
}

bool solve3x3(double matrix[3][4], double solution[3]) {
    for (int column = 0; column < 3; ++column) {
        int pivot_row      = column;
        double pivot_value = std::abs(matrix[column][column]);
        for (int row = column + 1; row < 3; ++row) {
            const double candidate = std::abs(matrix[row][column]);
            if (candidate > pivot_value) {
                pivot_value = candidate;
                pivot_row   = row;
            }
        }

        if (pivot_value < kNumericalTolerance) return false;

        if (pivot_row != column) {
            for (int i = column; i < 4; ++i) std::swap(matrix[column][i], matrix[pivot_row][i]);
        }

        const double pivot = matrix[column][column];
        for (int i = column; i < 4; ++i) matrix[column][i] /= pivot;

        for (int row = 0; row < 3; ++row) {
            if (row == column) continue;

            const double scale = matrix[row][column];
            for (int i = column; i < 4; ++i) matrix[row][i] -= scale * matrix[column][i];
        }
    }

    for (int i = 0; i < 3; ++i) solution[i] = matrix[i][3];

    return true;
}

void removeTransientSettings(fifojson& settings) {
    if (!settings.is_array()) return;

    for (auto& item : settings) {
        if (item.is_object()) item.erase(SS::kIsRegionStartSegment.toStdString());
    }
}

bool settingsCompatible(const QSharedPointer<SettingsBase>& lhs, const QSharedPointer<SettingsBase>& rhs) {
    if (lhs == nullptr || rhs == nullptr) return lhs == rhs;

    fifojson lhs_json = lhs->json();
    fifojson rhs_json = rhs->json();
    removeTransientSettings(lhs_json);
    removeTransientSettings(rhs_json);

    return lhs_json == rhs_json;
}

QSharedPointer<SettingsBase> resolvedArcSettings(const QSharedPointer<SegmentBase>& segment,
                                                 const QSharedPointer<SettingsBase>& fallback_settings) {
    QSharedPointer<SettingsBase> segment_settings = segment->getSb();
    if (segment_settings != nullptr && segment_settings->contains(PS::SpecialModes::kEnableArcFitting))
        return segment_settings;

    return fallback_settings;
}

bool lineEligibleForArcFitting(const QSharedPointer<SegmentBase>& segment,
                               const QSharedPointer<SettingsBase>& fallback_settings) {
    if (dynamic_cast<LineSegment*>(segment.data()) == nullptr) return false;

    if (!segment->isPrintingSegment()) return false;

    QSharedPointer<SettingsBase> settings = resolvedArcSettings(segment, fallback_settings);
    if (settings == nullptr || !settings->setting<bool>(PS::SpecialModes::kEnableArcFitting)) return false;

    if (segment->length() <= 0) return false;

    return std::abs(segment->start().z() - segment->end().z()) <= kPlanarZTolerance;
}

QVector<Point> collectPoints(const QList<QSharedPointer<SegmentBase>>& segments, int begin, int end) {
    QVector<Point> points;
    points.reserve(end - begin + 1);
    points.push_back(segments[begin]->start());

    for (int i = begin; i < end; ++i) points.push_back(segments[i]->end());

    return points;
}

bool fitCircle(const QVector<Point>& points, CircleFit& fit) {
    if (points.size() < 3) return false;

    double mean_x = 0.0;
    double mean_y = 0.0;
    for (const Point& point : points) {
        mean_x += point.x();
        mean_y += point.y();
    }
    mean_x /= static_cast<double>(points.size());
    mean_y /= static_cast<double>(points.size());

    double uu = 0.0;
    double uv = 0.0;
    double vv = 0.0;
    double u  = 0.0;
    double v  = 0.0;
    double ur = 0.0;
    double vr = 0.0;
    double r  = 0.0;

    for (const Point& point : points) {
        const double local_x        = point.x() - mean_x;
        const double local_y        = point.y() - mean_y;
        const double radius_squared = (local_x * local_x) + (local_y * local_y);

        uu += local_x * local_x;
        uv += local_x * local_y;
        vv += local_y * local_y;
        u += local_x;
        v += local_y;
        ur += local_x * radius_squared;
        vr += local_y * radius_squared;
        r += radius_squared;
    }

    double matrix[3][4] = {{uu, uv, u, -ur}, {uv, vv, v, -vr}, {u, v, static_cast<double>(points.size()), -r}};
    double solution[3]  = {0.0, 0.0, 0.0};
    if (!solve3x3(matrix, solution)) return false;

    const double center_x       = mean_x - (solution[0] / 2.0);
    const double center_y       = mean_y - (solution[1] / 2.0);
    const double center_local_x = center_x - mean_x;
    const double center_local_y = center_y - mean_y;
    const double radius_squared = (center_local_x * center_local_x) + (center_local_y * center_local_y) - solution[2];

    if (radius_squared <= kNumericalTolerance) return false;

    fit.center = Point(center_x, center_y, points.front().z());
    fit.radius = std::sqrt(radius_squared);

    return std::isfinite(fit.radius);
}

ArcFitResult validateArcFit(const QVector<Point>& points, double tolerance, CircleFit& fit) {
    const double allowed_error = std::max(tolerance, kNumericalTolerance);
    const float reference_z    = points.front().z();
    for (const Point& point : points) {
        if (std::abs(point.z() - reference_z) > kPlanarZTolerance) return ArcFitResult::kInvalid;

        const double point_radius = std::hypot(point.x() - fit.center.x(), point.y() - fit.center.y());
        if (std::abs(point_radius - fit.radius) > allowed_error) return ArcFitResult::kInvalid;
    }

    if (points.front().distance(points.back()) <= kNumericalTolerance) return ArcFitResult::kInvalid;

    double sweep  = 0.0;
    int direction = 0;
    for (int i = 1, end = points.size(); i < end; ++i) {
        const double previous_angle =
            std::atan2(points[i - 1].y() - fit.center.y(), points[i - 1].x() - fit.center.x());
        const double current_angle = std::atan2(points[i].y() - fit.center.y(), points[i].x() - fit.center.x());
        const double delta         = normalizedAngleDelta(current_angle - previous_angle);

        if (std::abs(delta) <= kNumericalTolerance) continue;

        const int delta_direction = delta > 0.0 ? 1 : -1;
        if (direction == 0)
            direction = delta_direction;
        else if (direction != delta_direction)
            return ArcFitResult::kInvalid;

        sweep += delta;
    }

    fit.sweep = std::abs(sweep);
    fit.ccw   = sweep > 0.0;

    if (direction == 0) return ArcFitResult::kInvalid;

    if (fit.sweep < kMinimumArcSweep) return ArcFitResult::kNeedsMoreSweep;

    if (fit.sweep > kMaximumArcSweep) return ArcFitResult::kInvalid;

    return ArcFitResult::kValid;
}

ArcFitResult tryFitArc(const QList<QSharedPointer<SegmentBase>>& segments, int begin, int end, double tolerance,
                       CircleFit& fit) {
    const QVector<Point> points = collectPoints(segments, begin, end);
    if (!fitCircle(points, fit)) return ArcFitResult::kInvalid;

    return validateArcFit(points, tolerance, fit);
}

QSharedPointer<ArcSegment> createArcSegment(const QSharedPointer<SegmentBase>& source, const Point& start,
                                            const Point& end, const CircleFit& fit) {
    QSharedPointer<ArcSegment> arc =
        QSharedPointer<ArcSegment>::create(start, end, fit.center, Angle(fit.sweep), fit.ccw);
    QSharedPointer<SettingsBase> settings = QSharedPointer<SettingsBase>::create(*source->getSb());
    arc->setSb(settings);

    return arc;
}
}  // namespace

void Path::add(const QSharedPointer<SegmentBase>& ps) {
    m_segments.append(ps);
}

void Path::append(const QSharedPointer<SegmentBase>& ps) {
    m_segments.append(ps);
}

void Path::append(Path path) {
    for (QSharedPointer<SegmentBase> seg : path.getSegments()) m_segments.append(seg);
}

void Path::prepend(const QSharedPointer<SegmentBase>& ps) {
    m_segments.prepend(ps);
}

void Path::insert(int index, const QSharedPointer<SegmentBase>& ps) {
    m_segments.insert(index, ps);
}

void Path::remove(const QSharedPointer<SegmentBase>& ps) {
    m_segments.removeOne(ps);
}

void Path::removeAt(int index) {
    m_segments.removeAt(index);
}

void Path::reverseSegments() {
    QList<QSharedPointer<SegmentBase>> newSegments;
    newSegments.reserve(m_segments.size());

    for (int i = m_segments.size() - 1; i >= 0; --i) {
        m_segments[i]->reverse();
        newSegments.push_back(m_segments[i]);
    }
    m_segments = newSegments;
}

Path Path::operator+=(const QSharedPointer<SegmentBase>& ps) {
    m_segments.append(ps);
    return *this;
}

QList<QSharedPointer<SegmentBase>>::iterator Path::begin() {
    return m_segments.begin();
}

QList<QSharedPointer<SegmentBase>>::iterator Path::end() {
    return m_segments.end();
}

QList<QSharedPointer<SegmentBase>>::const_iterator Path::begin() const {
    return m_segments.constBegin();
}

QList<QSharedPointer<SegmentBase>>::const_iterator Path::end() const {
    return m_segments.constEnd();
}

QSharedPointer<SegmentBase> Path::operator[](const int index) const {
    return m_segments[index];
}

QSharedPointer<SegmentBase> Path::at(const int index) const {
    return m_segments[index];
}

QSharedPointer<SegmentBase> Path::front() const {
    return m_segments.front();
}

QSharedPointer<SegmentBase> Path::back() const {
    return m_segments.back();
}

int Path::size() const {
    return m_segments.size();
}

void Path::move(int from, int to) {
    m_segments.move(from, to);
}

void Path::clear() {
    m_segments.clear();
}

QList<QSharedPointer<SegmentBase>>& Path::getSegments() {
    return m_segments;
}

Distance Path::calculateLength() {
    Distance totalLength;
    for (QSharedPointer<SegmentBase> segment : m_segments) totalLength += segment->length();
    return totalLength;
}

Distance Path::calculateLengthNoTravel() {
    Distance totalLength;
    for (QSharedPointer<SegmentBase> segment : m_segments) {
        if (!segment->isPrintingSegment()) continue;

        totalLength += segment->length();
    }
    return totalLength;
}

Distance Path::calculatePrintingLength() {
    Distance totalLength;
    for (QSharedPointer<SegmentBase> segment : m_segments) {
        if (TravelSegment* ts = dynamic_cast<TravelSegment*>(segment.data())) continue;
        totalLength += segment->length();
    }
    return totalLength;
}

void Path::transform(QQuaternion rotation, Point shift) {
    // rotate and then shift every segment in the path
    for (QSharedPointer<SegmentBase> path_segment : m_segments) {
        path_segment->rotate(rotation);
        path_segment->shift(shift);
    }
}

float Path::getMinZ() {
    // find the minimum of the segments in this path
    float path_min = std::numeric_limits<float>::max();
    for (QSharedPointer<SegmentBase> segment : m_segments) {
        float segment_min = segment->getMinZ();
        if (segment_min < path_min) path_min = segment_min;
    }
    return path_min;
}

void Path::removeTravels() {
    for (int i = m_segments.size() - 1; i >= 0; --i) {
        if (TravelSegment* ts = dynamic_cast<TravelSegment*>(m_segments[i].data())) m_segments.removeAt(i);
    }
}

void Path::fitCircularArcs(const QSharedPointer<SettingsBase>& fallback_settings) {
    if (m_segments.size() < kDefaultMinimumArcSegments) return;

    QList<QSharedPointer<SegmentBase>> fitted_segments;
    fitted_segments.reserve(m_segments.size());

    int index = 0;
    while (index < m_segments.size()) {
        if (!lineEligibleForArcFitting(m_segments[index], fallback_settings)) {
            fitted_segments.push_back(m_segments[index]);
            ++index;
            continue;
        }

        int run_end = index + 1;
        while (run_end < m_segments.size() && lineEligibleForArcFitting(m_segments[run_end], fallback_settings) &&
               m_segments[run_end - 1]->end() == m_segments[run_end]->start() &&
               settingsCompatible(m_segments[run_end - 1]->getSb(), m_segments[run_end]->getSb())) {
            ++run_end;
        }

        int run_index = index;
        while (run_index < run_end) {
            QSharedPointer<SettingsBase> settings = resolvedArcSettings(m_segments[run_index], fallback_settings);
            Distance tolerance                    = settings->setting<Distance>(PS::SpecialModes::kArcFittingTolerance);
            if (tolerance < 0) tolerance = 0.0 * micron;

            const int minimum_segments = std::max(
                kDefaultMinimumArcSegments, settings->setting<int>(PS::SpecialModes::kArcFittingMinimumSegmentCount));

            CircleFit best_fit;
            int best_end = -1;

            for (int candidate_end = run_index + minimum_segments; candidate_end <= run_end; ++candidate_end) {
                CircleFit candidate_fit;
                const ArcFitResult fit_result =
                    tryFitArc(m_segments, run_index, candidate_end, tolerance(), candidate_fit);
                if (fit_result != ArcFitResult::kValid) {
                    if (best_end >= 0 || fit_result == ArcFitResult::kInvalid) break;

                    continue;
                }

                best_fit = candidate_fit;
                best_end = candidate_end;
            }

            if (best_end >= 0) {
                const Point start = m_segments[run_index]->start();
                const Point end   = m_segments[best_end - 1]->end();
                fitted_segments.push_back(createArcSegment(m_segments[run_index], start, end, best_fit));
                run_index = best_end;
            }
            else {
                fitted_segments.push_back(m_segments[run_index]);
                ++run_index;
            }
        }

        index = run_end;
    }

    m_segments = fitted_segments;
}

bool Path::isClosed() {
    return m_segments.first()->start() == m_segments.last()->end();
}

void Path::setCCW(bool ccw) {
    m_ccw = ccw;
}

bool Path::getCCW() {
    return m_ccw;
}

}  // namespace ORNL
