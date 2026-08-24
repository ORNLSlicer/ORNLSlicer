#include "geometry/path_modifier.h"

#include <math.h>

#include <QVector>
#include <QtMath>
#include <cmath>

#include <qcontainerfwd.h>
#include <qminmax.h>
#include <qnumeric.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
void copyInitialExtruderSpeed(const QSharedPointer<SettingsBase>& destination,
                              const QSharedPointer<SettingsBase>& source) {
    if (source->contains(MS::Extruder::kInitialSpeed)) {
        destination->setSetting(MS::Extruder::kInitialSpeed, source->setting<int>(MS::Extruder::kInitialSpeed));
    }
}

void copyAdaptedWidthFlag(const QSharedPointer<SettingsBase>& destination, const QSharedPointer<SettingsBase>& source) {
    if (source->contains(SS::kAdapted)) { destination->setSetting(SS::kAdapted, source->setting<bool>(SS::kAdapted)); }
}

bool isExtendableLineSegment(const QSharedPointer<SegmentBase>& segment) {
    return segment->isPrintingSegment() && dynamic_cast<LineSegment*>(segment.data()) != nullptr;
}

bool pointsCoincident(const Point& lhs, const Point& rhs) {
    return lhs.distance(rhs)() <= 1.0e-4;
}

struct Vector2D {
    double x = 0.0;
    double y = 0.0;
};

struct SharpCornerGeometry {
    Point previous_cut;
    Point sharp_point;
    Point next_cut;
    double previous_cut_parameter = 1.0;
    double next_cut_parameter     = 0.0;
};

struct SegmentEndpointUpdates {
    bool has_start = false;
    bool has_end   = false;
    Point start;
    Point end;
    double start_parameter = 0.0;
    double end_parameter   = 1.0;
};

struct SharpCornerSettings {
    bool enabled                  = false;
    double threshold_radians      = 0.0;
    double extension_length       = 0.0;
    double close_points_threshold = 0.0;
    double sharpening_leg_length  = 0.0;
};

template <typename T>
T settingWithFallback(const QSharedPointer<SettingsBase>& settings, const QSharedPointer<SettingsBase>& fallback,
                      const QString& key) {
    if (!settings.isNull() && settings->contains(key)) return settings->setting<T>(key);

    if (!fallback.isNull()) return fallback->setting<T>(key);

    return T();
}

SharpCornerSettings sharpCornerSettingsForSegment(const QSharedPointer<SegmentBase>& segment,
                                                  const QSharedPointer<SettingsBase>& fallback) {
    const QSharedPointer<SettingsBase> segment_settings = segment->getSb();
    const Angle threshold =
        settingWithFallback<Angle>(segment_settings, fallback, PS::SpecialModes::kSharpCornerExtensionAngle);
    const Distance extension_distance =
        settingWithFallback<Distance>(segment_settings, fallback, PS::SpecialModes::kSharpCornerExtensionDistance);
    const Distance close_points_threshold =
        settingWithFallback<Distance>(segment_settings, fallback, PS::SpecialModes::kSharpCornerClosePointsThreshold);
    Distance sharpening_leg_length =
        settingWithFallback<Distance>(segment_settings, fallback, PS::SpecialModes::kSharpCornerSharpeningLegLength);
    if (sharpening_leg_length <= 0) sharpening_leg_length = extension_distance;

    return {
        settingWithFallback<bool>(segment_settings, fallback, PS::SpecialModes::kEnableSharpCornerExtension),
        qMin(M_PI, threshold()),
        extension_distance(),
        close_points_threshold(),
        sharpening_leg_length(),
    };
}

bool sharpCornerEnabledForBothLegs(const QSharedPointer<SegmentBase>& previous_segment,
                                   const QSharedPointer<SegmentBase>& next_segment,
                                   const QSharedPointer<SettingsBase>& fallback, SharpCornerSettings& settings) {
    settings                                = sharpCornerSettingsForSegment(previous_segment, fallback);
    const SharpCornerSettings next_settings = sharpCornerSettingsForSegment(next_segment, fallback);

    return settings.enabled && next_settings.enabled && settings.threshold_radians > 0.0 &&
           settings.extension_length > 0.0 && settings.sharpening_leg_length > 0.0;
}

double crossProduct(const Vector2D& lhs, const Vector2D& rhs) {
    return lhs.x * rhs.y - lhs.y * rhs.x;
}

bool normalizedDirection(const Point& start, const Point& end, Vector2D& direction, double& length) {
    const double x              = end.x() - start.x();
    const double y              = end.y() - start.y();
    length                      = std::hypot(x, y);
    constexpr double kMinLength = 1.0e-6;
    if (length < kMinLength) return false;

    direction = {x / length, y / length};
    return true;
}

Point pointOnSegment(const Point& start, const Point& end, double parameter) {
    return Point(start.x() + ((end.x() - start.x()) * parameter), start.y() + ((end.y() - start.y()) * parameter),
                 start.z() + ((end.z() - start.z()) * parameter));
}

bool lineIntersection(const Point& first_start, const Point& first_end, const Point& second_start,
                      const Point& second_end, Point& intersection) {
    const Vector2D first_axis           = {first_end.x() - first_start.x(), first_end.y() - first_start.y()};
    const Vector2D second_axis          = {second_end.x() - second_start.x(), second_end.y() - second_start.y()};
    const double denominator            = crossProduct(first_axis, second_axis);
    constexpr double kParallelTolerance = 1.0e-8;
    if (std::abs(denominator) < kParallelTolerance) return false;

    const Vector2D start_delta   = {second_start.x() - first_start.x(), second_start.y() - first_start.y()};
    const double first_parameter = crossProduct(start_delta, second_axis) / denominator;

    const double x = first_start.x() + (first_axis.x * first_parameter);
    const double y = first_start.y() + (first_axis.y * first_parameter);
    if (!std::isfinite(x) || !std::isfinite(y)) return false;

    intersection = Point(x, y, (first_end.z() + second_start.z()) / 2.0f);
    return true;
}

QSharedPointer<SegmentBase> cloneSegmentWithEndpoints(const QSharedPointer<SegmentBase>& source, const Point& start,
                                                      const Point& end) {
    QSharedPointer<SegmentBase> segment = source->clone();
    segment->setStart(start);
    segment->setEnd(end);

    if (!source->getSb().isNull()) segment->setSb(QSharedPointer<SettingsBase>::create(*source->getSb()));

    return segment;
}

bool calculateSharpCornerGeometry(const QSharedPointer<SegmentBase>& previous_segment,
                                  const QSharedPointer<SegmentBase>& next_segment, double threshold_radians,
                                  double extension_length, double sharpening_leg_length,
                                  SharpCornerGeometry& geometry) {
    Vector2D previous_direction;
    Vector2D next_direction;
    double previous_length = 0.0;
    double next_length     = 0.0;

    if (!normalizedDirection(previous_segment->start(), previous_segment->end(), previous_direction, previous_length) ||
        !normalizedDirection(next_segment->start(), next_segment->end(), next_direction, next_length)) {
        return false;
    }

    if (previous_length <= sharpening_leg_length || next_length <= sharpening_leg_length) return false;

    double cos_theta = (previous_direction.x * next_direction.x) + (previous_direction.y * next_direction.y);
    cos_theta        = qMin(1.0, cos_theta);
    cos_theta        = qMax(-1.0, cos_theta);

    const double corner_angle = M_PI - std::acos(cos_theta);
    if (corner_angle > threshold_radians) return false;

    Point merge_point;
    if (!lineIntersection(previous_segment->start(), previous_segment->end(), next_segment->start(),
                          next_segment->end(), merge_point)) {
        return false;
    }

    const Vector2D extension_direction      = {previous_direction.x - next_direction.x,
                                               previous_direction.y - next_direction.y};
    const double extension_direction_length = std::hypot(extension_direction.x, extension_direction.y);
    constexpr double kMinLength             = 1.0e-6;
    if (extension_direction_length < kMinLength) return false;

    geometry.previous_cut_parameter = 1.0 - (sharpening_leg_length / previous_length);
    geometry.next_cut_parameter     = sharpening_leg_length / next_length;
    geometry.previous_cut =
        pointOnSegment(previous_segment->start(), previous_segment->end(), geometry.previous_cut_parameter);
    geometry.next_cut    = pointOnSegment(next_segment->start(), next_segment->end(), geometry.next_cut_parameter);
    geometry.sharp_point = Point(
        merge_point.x() + (extension_direction.x / extension_direction_length * extension_length),
        merge_point.y() + (extension_direction.y / extension_direction_length * extension_length), merge_point.z());

    return true;
}

bool canApplyEndUpdate(int segment_index, double parameter, const QVector<bool>& skip_segment,
                       const QVector<SegmentEndpointUpdates>& updates) {
    constexpr double kParameterTolerance = 1.0e-6;
    if (skip_segment[segment_index] || updates[segment_index].has_end) return false;

    return !updates[segment_index].has_start ||
           updates[segment_index].start_parameter < parameter - kParameterTolerance;
}

bool canApplyStartUpdate(int segment_index, double parameter, const QVector<bool>& skip_segment,
                         const QVector<SegmentEndpointUpdates>& updates) {
    constexpr double kParameterTolerance = 1.0e-6;
    if (skip_segment[segment_index] || updates[segment_index].has_start) return false;

    return !updates[segment_index].has_end || parameter < updates[segment_index].end_parameter - kParameterTolerance;
}

bool applySharpCornerGeometry(Path& path, int previous_index, int next_index, const SharpCornerGeometry& geometry,
                              QVector<bool>& skip_segment, QVector<SegmentEndpointUpdates>& updates,
                              QVector<QVector<QSharedPointer<SegmentBase>>>& insertions_after) {
    if (!canApplyEndUpdate(previous_index, geometry.previous_cut_parameter, skip_segment, updates) ||
        !canApplyStartUpdate(next_index, geometry.next_cut_parameter, skip_segment, updates) ||
        !insertions_after[previous_index].isEmpty()) {
        return false;
    }

    updates[previous_index].has_end       = true;
    updates[previous_index].end           = geometry.previous_cut;
    updates[previous_index].end_parameter = geometry.previous_cut_parameter;

    updates[next_index].has_start       = true;
    updates[next_index].start           = geometry.next_cut;
    updates[next_index].start_parameter = geometry.next_cut_parameter;

    insertions_after[previous_index].push_back(
        cloneSegmentWithEndpoints(path[previous_index], geometry.previous_cut, geometry.sharp_point));
    insertions_after[previous_index].push_back(
        cloneSegmentWithEndpoints(path[next_index], geometry.sharp_point, geometry.next_cut));

    return true;
}
}  // namespace

void PathModifierGenerator::GenerateTravel(Path& path, Point current_location, Velocity velocity) {
    QSharedPointer<TravelSegment> travel_segment =
        QSharedPointer<TravelSegment>::create(current_location, path.front()->start());
    QSharedPointer<SettingsBase> next_segment_settings = path.front()->getSb();

    copyInitialExtruderSpeed(travel_segment->getSb(), next_segment_settings);
    travel_segment->getSb()->setSetting(SS::kRegionType, next_segment_settings->setting<RegionType>(SS::kRegionType));
    travel_segment->getSb()->setSetting(SS::kExtruderSpeed,
                                        next_segment_settings->setting<AngularVelocity>(SS::kExtruderSpeed));
    travel_segment->getSb()->setSetting(SS::kSpeed, velocity);

    path.prepend(travel_segment);
}

void PathModifierGenerator::GenerateOpenLoopLeadIn(Path& path, Distance leadInDistance, Velocity leadInSpeed,
                                                   AngularVelocity leadInExtruderSpeed, bool enableWidthHeight,
                                                   double areaMultiplier) {
    Point firstPoint  = path[0]->start();
    Point secondPoint = path[0]->end();
    Distance length   = secondPoint.distance(firstPoint);
    Distance X        = firstPoint.x() + (firstPoint.x() - secondPoint.x()) / length() * leadInDistance();
    Distance Y        = firstPoint.y() + (firstPoint.y() - secondPoint.y()) / length() * leadInDistance();
    Distance Z        = firstPoint.z();
    Point newStart    = Point(X, Y, Z);

    QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(newStart, firstPoint);

    copyInitialExtruderSpeed(segment->getSb(), path[0]->getSb());
    copyAdaptedWidthFlag(segment->getSb(), path[0]->getSb());
    segment->getSb()->setSetting(SS::kWidth, path[0]->getSb()->setting<Distance>(SS::kWidth));
    segment->getSb()->setSetting(SS::kHeight, path[0]->getSb()->setting<Distance>(SS::kHeight));
    segment->getSb()->setSetting(SS::kSpeed, leadInSpeed);
    segment->getSb()->setSetting(SS::kAccel, path[0]->getSb()->setting<Acceleration>(SS::kAccel));
    segment->getSb()->setSetting(SS::kExtruderSpeed, leadInExtruderSpeed);
    segment->getSb()->setSetting(SS::kRegionType, path[0]->getSb()->setting<RegionType>(SS::kRegionType));
    segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kLeadIn);

    // Update Width and Height if using Width and Height mode
    if (enableWidthHeight) {
        areaMultiplier = qSqrt(areaMultiplier / 100.0);
        segment->getSb()->setSetting(SS::kWidth, path[0]->getSb()->setting<Distance>(SS::kWidth) * areaMultiplier);
        segment->getSb()->setSetting(SS::kHeight, path[0]->getSb()->setting<Distance>(SS::kHeight) * areaMultiplier);
    }

    path.insert(0, segment);
}

void PathModifierGenerator::GeneratePrestart(Path& path, Distance prestartDistance, Velocity prestartSpeed,
                                             AngularVelocity prestartExtruderSpeed, bool enableWidthHeight,
                                             double areaMultiplier) {
    Point firstPoint  = path[0]->start();
    Point secondPoint = path[0]->end();
    Distance length   = secondPoint.distance(firstPoint);
    Distance X        = firstPoint.x() + (firstPoint.x() - secondPoint.x()) / length() * prestartDistance();
    Distance Y        = firstPoint.y() + (firstPoint.y() - secondPoint.y()) / length() * prestartDistance();
    Distance Z        = firstPoint.z();
    Point newStart    = Point(X, Y, Z);

    QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(newStart, firstPoint);

    copyInitialExtruderSpeed(segment->getSb(), path[0]->getSb());
    copyAdaptedWidthFlag(segment->getSb(), path[0]->getSb());
    segment->getSb()->setSetting(SS::kWidth, path[0]->getSb()->setting<Distance>(SS::kWidth));
    segment->getSb()->setSetting(SS::kHeight, path[0]->getSb()->setting<Distance>(SS::kHeight));
    segment->getSb()->setSetting(SS::kSpeed, prestartSpeed);
    segment->getSb()->setSetting(SS::kAccel, path[0]->getSb()->setting<Acceleration>(SS::kAccel));
    segment->getSb()->setSetting(SS::kExtruderSpeed, prestartExtruderSpeed);
    segment->getSb()->setSetting(SS::kRegionType, path[0]->getSb()->setting<RegionType>(SS::kRegionType));
    segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kPrestart);
    segment->getSb()->setSetting(SS::kMaterialNumber, path[0]->getSb()->setting<int>(SS::kMaterialNumber));

    if (enableWidthHeight) {
        areaMultiplier = qSqrt(areaMultiplier / 100.0);
        segment->getSb()->setSetting(SS::kWidth, path[0]->getSb()->setting<Distance>(SS::kWidth) * areaMultiplier);
        segment->getSb()->setSetting(SS::kHeight, path[0]->getSb()->setting<Distance>(SS::kHeight) * areaMultiplier);
    }

    path.insert(0, segment);
}

void PathModifierGenerator::GenerateFlyingStart(Path& path, Distance flyingStartDistance, Velocity flyingStartSpeed) {
    // Start with last segment in the path
    int currentIndex = path.size() - 1;
    while (flyingStartDistance > 0) {
        // If the last segment is some version of a tip wipe, ignore the segment and go to the one before it. The flying
        // start should be based on standard path segments
        if (path[currentIndex]->getSb()->setting<PathModifiers>(SS::kPathModifiers) == PathModifiers::kForwardTipWipe ||
            path[currentIndex]->getSb()->setting<PathModifiers>(SS::kPathModifiers) == PathModifiers::kReverseTipWipe ||
            path[currentIndex]->getSb()->setting<PathModifiers>(SS::kPathModifiers) ==
                PathModifiers::kPerimeterTipWipe ||
            path[currentIndex]->getSb()->setting<PathModifiers>(SS::kPathModifiers) == PathModifiers::kAngledTipWipe ||
            path[currentIndex]->getSb()->setting<PathModifiers>(SS::kPathModifiers) == PathModifiers::kSpiralLift) {
            currentIndex = (currentIndex - 1) % path.size();
            continue;
        }

        RegionType regionType = path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType);

        Distance nextSegmentDist = path[currentIndex]->start().distance(path[currentIndex]->end());
        flyingStartDistance -= nextSegmentDist;

        // If flying start distance is greater than zero, use the exact start and end from that segment to create a
        // flying start segment
        if (flyingStartDistance >= 0) {
            QSharedPointer<LineSegment> segment =
                QSharedPointer<LineSegment>::create(path[currentIndex]->start(), path[currentIndex]->end());

            copyInitialExtruderSpeed(segment->getSb(), path[currentIndex]->getSb());
            copyAdaptedWidthFlag(segment->getSb(), path[currentIndex]->getSb());
            segment->getSb()->setSetting(SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth));
            segment->getSb()->setSetting(SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight));
            segment->getSb()->setSetting(SS::kSpeed, flyingStartSpeed);
            segment->getSb()->setSetting(SS::kAccel, path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel));
            segment->getSb()->setSetting(SS::kExtruderSpeed, 0);
            segment->getSb()->setSetting(SS::kRegionType, regionType);
            segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kFlyingStart);
            segment->getSb()->setSetting(SS::kMaterialNumber,
                                         path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber));

            path.insert(0, segment);
        }
        else  // Break the original segment's path into a shorter segment to be used for the flying start
        {
            float percentage = 1 - (-flyingStartDistance() / nextSegmentDist());
            // swap the ends and starts?
            Point newStart = Point(
                (1.0 - percentage) * path[currentIndex]->end().x() + percentage * path[currentIndex]->start().x(),
                (1.0 - percentage) * path[currentIndex]->end().y() + percentage * path[currentIndex]->start().y());

            Point end = path[currentIndex]->end();

            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(newStart, end);

            copyInitialExtruderSpeed(segment->getSb(), path[currentIndex]->getSb());
            copyAdaptedWidthFlag(segment->getSb(), path[currentIndex]->getSb());
            segment->getSb()->setSetting(SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth));
            segment->getSb()->setSetting(SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight));
            segment->getSb()->setSetting(SS::kSpeed, flyingStartSpeed);
            segment->getSb()->setSetting(SS::kAccel, path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel));
            segment->getSb()->setSetting(SS::kExtruderSpeed, 0);
            segment->getSb()->setSetting(SS::kRegionType, regionType);
            segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kFlyingStart);
            segment->getSb()->setSetting(SS::kMaterialNumber,
                                         path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber));

            path.insert(0, segment);
        }
    }
}

void PathModifierGenerator::GenerateInitialStartup(Path& path, Distance startDistance, Velocity startSpeed,
                                                   AngularVelocity extruderSpeed, bool enableWidthHeight,
                                                   double areaMultiplier) {
    int currentIndex = 0;
    while (startDistance > 0) {
        RegionType regionType = path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType);

        Distance nextSegmentDist = path[currentIndex]->start().distance(path[currentIndex]->end());
        startDistance -= nextSegmentDist;

        if (startDistance >= 0) {
            // Update Width and Height if using Width and Height mode
            if (enableWidthHeight) {
                Distance tempWidth =
                    path[currentIndex]->getSb()->setting<Distance>(SS::kWidth) * qSqrt(areaMultiplier / 100);
                Distance tempHeight =
                    path[currentIndex]->getSb()->setting<Distance>(SS::kHeight) * qSqrt(areaMultiplier / 100);
                path[currentIndex]->getSb()->setSetting(SS::kWidth, tempWidth);
                path[currentIndex]->getSb()->setSetting(SS::kHeight, tempHeight);
            }
            path[currentIndex]->getSb()->setSetting(SS::kSpeed, startSpeed);
            path[currentIndex]->getSb()->setSetting(SS::kExtruderSpeed, extruderSpeed);
            path[currentIndex]->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kInitialStartup);
        }
        else {
            float percentage = 1 - (-startDistance() / nextSegmentDist());
            Point end        = Point(
                (1.0 - percentage) * path[currentIndex]->start().x() + percentage * path[currentIndex]->end().x(),
                (1.0 - percentage) * path[currentIndex]->start().y() + percentage * path[currentIndex]->end().y());

            Point oldStart = path[currentIndex]->start();
            path[currentIndex]->setStart(end);

            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(oldStart, end);

            copyInitialExtruderSpeed(segment->getSb(), path[currentIndex]->getSb());
            copyAdaptedWidthFlag(segment->getSb(), path[currentIndex]->getSb());
            segment->getSb()->setSetting(SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth));
            segment->getSb()->setSetting(SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight));
            segment->getSb()->setSetting(SS::kSpeed, startSpeed);
            segment->getSb()->setSetting(SS::kAccel, path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel));
            segment->getSb()->setSetting(SS::kExtruderSpeed, extruderSpeed);
            segment->getSb()->setSetting(SS::kRegionType, regionType);
            segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kInitialStartup);
            segment->getSb()->setSetting(SS::kMaterialNumber,
                                         path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber));

            // Update Width and Height if using Width and Height mode
            if (enableWidthHeight) {
                areaMultiplier = qSqrt(areaMultiplier / 100);
                segment->getSb()->setSetting(
                    SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth) * areaMultiplier);
                segment->getSb()->setSetting(
                    SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight) * areaMultiplier);
            }

            path.insert(currentIndex, segment);
        }

        currentIndex = (currentIndex + 1) % path.size();
    }
}

void PathModifierGenerator::GenerateInitialStartupWithRampUp(Path& path, Distance startDistance, Velocity startSpeed,
                                                             Velocity endSpeed, AngularVelocity startExtruderSpeed,
                                                             AngularVelocity endExtruderSpeed, int steps,
                                                             bool enableWidthHeight, double areaMultiplier) {
    int currentIndex        = 0;
    Distance stepDistance   = startDistance / steps;
    AngularVelocity rpmStep = (endExtruderSpeed - startExtruderSpeed) / steps;
    Velocity speedStep      = (endSpeed - startSpeed) / steps;

    // Loop through once to do the standard initial startup pathing
    while (startDistance > 0) {
        RegionType regionType = path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType);

        Distance nextSegmentDist = path[currentIndex]->start().distance(path[currentIndex]->end());
        startDistance -= nextSegmentDist;

        if (startDistance >= 0) {
            // Update Width and Height if using Width and Height mode
            if (enableWidthHeight) {
                Distance tempWidth =
                    path[currentIndex]->getSb()->setting<Distance>(SS::kWidth) * qSqrt(areaMultiplier / 100);
                Distance tempHeight =
                    path[currentIndex]->getSb()->setting<Distance>(SS::kHeight) * qSqrt(areaMultiplier / 100);
                path[currentIndex]->getSb()->setSetting(SS::kWidth, tempWidth);
                path[currentIndex]->getSb()->setSetting(SS::kHeight, tempHeight);
            }
            path[currentIndex]->getSb()->setSetting(SS::kSpeed, startSpeed);
            path[currentIndex]->getSb()->setSetting(SS::kExtruderSpeed, startExtruderSpeed);
            path[currentIndex]->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kInitialStartup);
        }
        else {
            float percentage = 1 - (-startDistance() / nextSegmentDist());
            Point end        = Point(
                (1.0 - percentage) * path[currentIndex]->start().x() + percentage * path[currentIndex]->end().x(),
                (1.0 - percentage) * path[currentIndex]->start().y() + percentage * path[currentIndex]->end().y());

            Point oldStart = path[currentIndex]->start();
            path[currentIndex]->setStart(end);

            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(oldStart, end);

            copyInitialExtruderSpeed(segment->getSb(), path[currentIndex]->getSb());
            copyAdaptedWidthFlag(segment->getSb(), path[currentIndex]->getSb());
            segment->getSb()->setSetting(SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth));
            segment->getSb()->setSetting(SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight));
            segment->getSb()->setSetting(SS::kSpeed, startSpeed);
            segment->getSb()->setSetting(SS::kAccel, path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel));
            segment->getSb()->setSetting(SS::kExtruderSpeed, startExtruderSpeed);
            segment->getSb()->setSetting(SS::kRegionType, regionType);
            segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kInitialStartup);
            segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kInitialStartup);
            segment->getSb()->setSetting(SS::kMaterialNumber,
                                         path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber));

            // Update Width and Height if using Width and Height mode
            if (enableWidthHeight) {
                areaMultiplier = qSqrt(areaMultiplier / 100);
                segment->getSb()->setSetting(
                    SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth) * areaMultiplier);
                segment->getSb()->setSetting(
                    SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight) * areaMultiplier);
            }

            path.insert(currentIndex, segment);
        }

        currentIndex = (currentIndex + 1) % path.size();
    }

    // Loop through the initial startup pathing to break into smaller moves with corrected extruder speed
    currentIndex = 0;
    Distance currentDistance;
    AngularVelocity currentExtruderSpeed;
    Velocity currentSpeed;
    for (int j = 1; j < steps; j++) {
        currentDistance      = stepDistance;
        currentExtruderSpeed = startExtruderSpeed + rpmStep * (j - 1);
        currentSpeed         = startSpeed + speedStep * (j - 1);
        while (currentDistance > 0) {
            RegionType regionType = path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType);

            Distance nextSegmentDist = path[currentIndex]->start().distance(path[currentIndex]->end());
            currentDistance -= nextSegmentDist;

            if (currentDistance >= 0) {
                path[currentIndex]->getSb()->setSetting(SS::kSpeed, currentSpeed);
                path[currentIndex]->getSb()->setSetting(SS::kExtruderSpeed, currentExtruderSpeed);
                path[currentIndex]->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kInitialStartup);
            }
            else {
                float percentage = 1 - (-currentDistance() / nextSegmentDist());
                Point end        = Point(
                    (1.0 - percentage) * path[currentIndex]->start().x() + percentage * path[currentIndex]->end().x(),
                    (1.0 - percentage) * path[currentIndex]->start().y() + percentage * path[currentIndex]->end().y());

                Point oldStart = path[currentIndex]->start();
                path[currentIndex]->setStart(end);

                QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(oldStart, end);

                copyInitialExtruderSpeed(segment->getSb(), path[currentIndex]->getSb());
                copyAdaptedWidthFlag(segment->getSb(), path[currentIndex]->getSb());
                segment->getSb()->setSetting(SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth));
                segment->getSb()->setSetting(SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight));
                segment->getSb()->setSetting(SS::kSpeed, currentSpeed);
                segment->getSb()->setSetting(SS::kAccel,
                                             path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel));
                segment->getSb()->setSetting(SS::kExtruderSpeed, currentExtruderSpeed);
                segment->getSb()->setSetting(SS::kRegionType, regionType);
                segment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kInitialStartup);

                path.insert(currentIndex, segment);
            }

            currentIndex = (currentIndex + 1) % path.size();
        }
    }
    // After going through all steps, any remaining initial startup segments don't need start/end edited but need
    // updated RPM and speed
    while (path[currentIndex]->getSb()->setting<PathModifiers>(SS::kPathModifiers) == PathModifiers::kInitialStartup) {
        path[currentIndex]->getSb()->setSetting(SS::kSpeed, startSpeed + speedStep * (steps - 1));
        path[currentIndex]->getSb()->setSetting(SS::kExtruderSpeed, startExtruderSpeed + rpmStep * (steps - 1));
        currentIndex++;
    }
}

void PathModifierGenerator::GenerateSlowdown(Path& path, Distance slowDownDistance, Distance slowDownLiftDistance,
                                             Distance slowDownCutoffDistance, Velocity slowDownSpeed,
                                             AngularVelocity extruderSpeed, bool enableWidthHeight,
                                             double areaMultiplier) {
    int currentIndex      = path.size() - 1;
    bool isClosed         = false;
    Distance tempDistance = slowDownDistance;
    Point newEnd;
    if (path[currentIndex]->end() == path[0]->start()) isClosed = true;

    PathModifiers current_mod;
    if (extruderSpeed <= 0)
        current_mod = PathModifiers::kCoasting;
    else
        current_mod = PathModifiers::kSlowDown;

    while (tempDistance > 0 && ((currentIndex >= 0 && !isClosed) || isClosed)) {
        Distance newZIncrement = tempDistance / slowDownDistance * slowDownLiftDistance;
        newEnd                 = Point(path[currentIndex]->end().x(), path[currentIndex]->end().y(),
                                       path[currentIndex]->end().z() + newZIncrement);

        // Update start point of the move following this one, so that start points have correct Z value
        if (tempDistance != slowDownDistance && currentIndex + 1 < path.size()) {
            path[currentIndex + 1]->setStart(
                Point(path[currentIndex + 1]->start().x(), path[currentIndex + 1]->start().y(), newEnd.z()));
        }

        Distance nextSegmentDist = path[currentIndex]->end().distance(path[currentIndex]->start());
        tempDistance -= nextSegmentDist;

        if (tempDistance >= 0) {
            // Update Width and Height if using Width and Height mode
            if (enableWidthHeight) {
                Distance tempWidth =
                    path[currentIndex]->getSb()->setting<Distance>(SS::kWidth) * qSqrt(areaMultiplier / 100);
                Distance tempHeight =
                    path[currentIndex]->getSb()->setting<Distance>(SS::kHeight) * qSqrt(areaMultiplier / 100);
                path[currentIndex]->getSb()->setSetting(SS::kWidth, tempWidth);
                path[currentIndex]->getSb()->setSetting(SS::kHeight, tempHeight);
            }
            path[currentIndex]->getSb()->setSetting(SS::kSpeed, slowDownSpeed);
            path[currentIndex]->getSb()->setSetting(SS::kExtruderSpeed, extruderSpeed);
            path[currentIndex]->getSb()->setSetting(SS::kPathModifiers, current_mod);
            path[currentIndex]->setEnd(newEnd);
        }
        else {
            float percentage = 1 - (-tempDistance() / nextSegmentDist());
            Point end        = Point(
                (1.0 - percentage) * path[currentIndex]->end().x() + percentage * path[currentIndex]->start().x(),
                (1.0 - percentage) * path[currentIndex]->end().y() + percentage * path[currentIndex]->start().y());

            Point oldEnd = path[currentIndex]->end();
            path[currentIndex]->setEnd(end);

            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(end, newEnd);

            copyInitialExtruderSpeed(segment->getSb(), path[currentIndex]->getSb());
            copyAdaptedWidthFlag(segment->getSb(), path[currentIndex]->getSb());
            segment->getSb()->setSetting(SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth));
            segment->getSb()->setSetting(SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight));
            segment->getSb()->setSetting(SS::kSpeed, slowDownSpeed);
            segment->getSb()->setSetting(SS::kAccel, path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel));
            segment->getSb()->setSetting(SS::kExtruderSpeed, extruderSpeed);
            segment->getSb()->setSetting(SS::kRegionType,
                                         path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType));
            segment->getSb()->setSetting(SS::kPathModifiers, current_mod);
            segment->getSb()->setSetting(SS::kMaterialNumber,
                                         path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber));

            // Update Width and Height if using Width and Height mode
            if (enableWidthHeight) {
                areaMultiplier = qSqrt(areaMultiplier / 100);
                segment->getSb()->setSetting(
                    SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth) * areaMultiplier);
                segment->getSb()->setSetting(
                    SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight) * areaMultiplier);
            }

            path.insert(currentIndex + 1, segment);
        }

        currentIndex -= 1;
        if (currentIndex < 0) currentIndex = path.size() - 1;
    }

    // Step through loop again if a cutoff distance is needed
    currentIndex = path.size() - 1;
    current_mod  = PathModifiers::kCoasting;
    while (slowDownCutoffDistance > 0 && ((currentIndex >= 1 && !isClosed) || isClosed)) {
        Distance nextSegmentDist = path[currentIndex]->end().distance(path[currentIndex]->start());
        slowDownCutoffDistance -= nextSegmentDist;

        if (slowDownCutoffDistance >= 0) {
            path[currentIndex]->getSb()->setSetting(SS::kSpeed, slowDownSpeed);
            path[currentIndex]->getSb()->setSetting(SS::kExtruderSpeed, 0);
            path[currentIndex]->getSb()->setSetting(SS::kPathModifiers, current_mod);
        }
        else {
            float percentage = 1 - (-slowDownCutoffDistance() / nextSegmentDist());
            Point end        = Point(
                (1.0 - percentage) * path[currentIndex]->end().x() + percentage * path[currentIndex]->start().x(),
                (1.0 - percentage) * path[currentIndex]->end().y() + percentage * path[currentIndex]->start().y(),
                (1.0 - percentage) * path[currentIndex]->end().z() + percentage * path[currentIndex]->start().z());

            Point oldEnd = path[currentIndex]->end();
            path[currentIndex]->setEnd(end);

            QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(end, oldEnd);

            copyInitialExtruderSpeed(segment->getSb(), path[currentIndex]->getSb());
            copyAdaptedWidthFlag(segment->getSb(), path[currentIndex]->getSb());
            segment->getSb()->setSetting(SS::kWidth, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth));
            segment->getSb()->setSetting(SS::kHeight, path[currentIndex]->getSb()->setting<Distance>(SS::kHeight));
            segment->getSb()->setSetting(SS::kSpeed, slowDownSpeed);
            segment->getSb()->setSetting(SS::kAccel, path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel));
            segment->getSb()->setSetting(SS::kExtruderSpeed, 0);
            segment->getSb()->setSetting(SS::kRegionType,
                                         path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType));
            segment->getSb()->setSetting(SS::kPathModifiers, current_mod);
            segment->getSb()->setSetting(SS::kMaterialNumber,
                                         path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber));

            path.insert(currentIndex + 1, segment);
        }

        currentIndex -= 1;
        if (currentIndex < 0) currentIndex = path.size() - 1;
    }
}

void PathModifierGenerator::GenerateLayerLeadIn(Path& path, const Point& leadIn, QSharedPointer<SettingsBase> sb) {
    QSharedPointer<SegmentBase> firstBuildSegment;
    int extRate;
    int pathSize = path.size();

    for (int i = 0; i < pathSize; i++) {
        firstBuildSegment = path[i];
        extRate           = path[i]->getSb()->setting<int>(SS::kExtruderSpeed);
        if (extRate <= 0)  // If extrusion rate is zero, must be travel move
        {
            path[i]->setEnd(leadIn);
            continue;
        }

        QSharedPointer<LineSegment> leadInSegment =
            QSharedPointer<LineSegment>::create(leadIn, firstBuildSegment->start());
        copyInitialExtruderSpeed(leadInSegment->getSb(), firstBuildSegment->getSb());
        copyAdaptedWidthFlag(leadInSegment->getSb(), firstBuildSegment->getSb());
        leadInSegment->getSb()->setSetting(SS::kWidth, firstBuildSegment->getSb()->setting<Distance>(SS::kWidth));
        leadInSegment->getSb()->setSetting(SS::kHeight, firstBuildSegment->getSb()->setting<Distance>(SS::kHeight));
        leadInSegment->getSb()->setSetting(SS::kSpeed, firstBuildSegment->getSb()->setting<Velocity>(SS::kSpeed));
        leadInSegment->getSb()->setSetting(SS::kAccel, firstBuildSegment->getSb()->setting<Acceleration>(SS::kAccel));
        leadInSegment->getSb()->setSetting(SS::kExtruderSpeed,
                                           firstBuildSegment->getSb()->setting<AngularVelocity>(SS::kExtruderSpeed));
        leadInSegment->getSb()->setSetting(SS::kRegionType,
                                           firstBuildSegment->getSb()->setting<RegionType>(SS::kRegionType));
        leadInSegment->getSb()->setSetting(SS::kPathModifiers, PathModifiers::kLeadIn);
        path.insert(i, leadInSegment);

        break;
    }
}

void PathModifierGenerator::GenerateTrajectorySlowdown(Path& path, QSharedPointer<SettingsBase> sb) {
    Angle trajactoryAngleThresh = sb->setting<Angle>(ES::Ramping::kTrajectoryAngleThreshold);

    // if the threshold angle set to zero ignores the calculations and returns
    if (trajactoryAngleThresh <= 0) return;

    Distance rampDownLength               = sb->setting<Distance>(ES::Ramping::kTrajectoryAngleRampDownDistance);
    Distance rampUpLength                 = sb->setting<Distance>(ES::Ramping::kTrajectoryAngleRampUpDistance);
    Velocity speedSlowDown                = sb->setting<Distance>(ES::Ramping::kTrajectoryAngleSpeedSlowDown)();
    AngularVelocity extruderSpeedSlowDown = sb->setting<Distance>(ES::Ramping::kTrajectoryAngleExtruderSpeedSlowDown)();
    Velocity speedUp                      = sb->setting<Distance>(ES::Ramping::kTrajectoryAngleSpeedUp)();
    AngularVelocity extruderSpeedUp       = sb->setting<Distance>(ES::Ramping::kTrajectoryAngleExtruderSpeedUp)();

    for (int pathIndex = 0, end = path.size() - 1; pathIndex < end; ++pathIndex) {
        if (!path[pathIndex]->isPrintingSegment() || !path[pathIndex + 1]->isPrintingSegment()) continue;

        Point startPoint      = path[pathIndex]->start();
        Point connectingPoint = path[pathIndex]->end();
        Point endPoint        = path[pathIndex + 1]->end();

        double seg1X      = connectingPoint.x() - startPoint.x();
        double seg1Y      = connectingPoint.y() - startPoint.y();
        double seg2X      = endPoint.x() - connectingPoint.x();
        double seg2Y      = endPoint.y() - connectingPoint.y();
        double seg1Length = qSqrt(seg1X * seg1X + seg1Y * seg1Y);
        double seg2Length = qSqrt(seg2X * seg2X + seg2Y * seg2Y);

        double cosTheta = (seg1X * seg2X + seg1Y * seg2Y) / (seg1Length * seg2Length);
        cosTheta        = qMin(1.0, cosTheta);
        cosTheta        = qMax(-1.0, cosTheta);
        double theta    = qAbs(M_PI - qAcos(cosTheta));

        if (theta < trajactoryAngleThresh) {
            bool segmentSplitted = false;
            GenerateRamp(path, segmentSplitted, pathIndex, PathModifiers::kRampingDown, rampDownLength, speedSlowDown,
                         extruderSpeedSlowDown);
            if (segmentSplitted) {
                ++pathIndex;
                ++end;
            }

            segmentSplitted = false;
            GenerateRamp(path, segmentSplitted, pathIndex + 1, PathModifiers::kRampingUp, rampUpLength, speedUp,
                         extruderSpeedUp);
            if (segmentSplitted) {
                ++pathIndex;
                ++end;
            }
        }
    }
}

void PathModifierGenerator::GenerateSharpCornerExtension(Path& path, QSharedPointer<SettingsBase> sb) {
    if (path.size() < 2) return;

    const int segmentCount = path.size();
    const bool isClosed    = pointsCoincident(path.front()->start(), path.back()->end());

    QVector<bool> skipSegment(segmentCount, false);
    QVector<SegmentEndpointUpdates> endpointUpdates(segmentCount);
    QVector<QVector<QSharedPointer<SegmentBase>>> insertionsAfter(segmentCount);

    bool modifiedPath = false;
    if (segmentCount >= 3) {
        const int firstConnectorIndex = isClosed ? 0 : 1;
        const int connectorLimit      = isClosed ? segmentCount : segmentCount - 1;

        for (int connectorIndex = firstConnectorIndex; connectorIndex < connectorLimit; ++connectorIndex) {
            if (skipSegment[connectorIndex] || !isExtendableLineSegment(path[connectorIndex])) { continue; }

            const int previousIndex = (connectorIndex + segmentCount - 1) % segmentCount;
            const int nextIndex     = (connectorIndex + 1) % segmentCount;

            if (skipSegment[previousIndex] || skipSegment[nextIndex] || previousIndex == nextIndex ||
                !isExtendableLineSegment(path[previousIndex]) || !isExtendableLineSegment(path[nextIndex])) {
                continue;
            }

            if (!pointsCoincident(path[previousIndex]->end(), path[connectorIndex]->start()) ||
                !pointsCoincident(path[connectorIndex]->end(), path[nextIndex]->start())) {
                continue;
            }

            SharpCornerSettings settings;
            const SharpCornerSettings connector_settings = sharpCornerSettingsForSegment(path[connectorIndex], sb);
            if (!sharpCornerEnabledForBothLegs(path[previousIndex], path[nextIndex], sb, settings) ||
                !connector_settings.enabled || settings.close_points_threshold <= 0.0 ||
                path[connectorIndex]->length() > settings.close_points_threshold) {
                continue;
            }

            SharpCornerGeometry geometry;
            if (!calculateSharpCornerGeometry(path[previousIndex], path[nextIndex], settings.threshold_radians,
                                              settings.extension_length, settings.sharpening_leg_length, geometry)) {
                continue;
            }

            if (!applySharpCornerGeometry(path, previousIndex, nextIndex, geometry, skipSegment, endpointUpdates,
                                          insertionsAfter)) {
                continue;
            }

            skipSegment[connectorIndex] = true;
            modifiedPath                = true;
        }
    }

    const int junctionCount = isClosed ? segmentCount : segmentCount - 1;
    for (int previousIndex = 0; previousIndex < junctionCount; ++previousIndex) {
        const int nextIndex = (previousIndex + 1) % segmentCount;
        if (skipSegment[previousIndex] || skipSegment[nextIndex] || !isExtendableLineSegment(path[previousIndex]) ||
            !isExtendableLineSegment(path[nextIndex])) {
            continue;
        }

        if (!pointsCoincident(path[previousIndex]->end(), path[nextIndex]->start())) continue;

        SharpCornerSettings settings;
        if (!sharpCornerEnabledForBothLegs(path[previousIndex], path[nextIndex], sb, settings)) continue;

        SharpCornerGeometry geometry;
        if (!calculateSharpCornerGeometry(path[previousIndex], path[nextIndex], settings.threshold_radians,
                                          settings.extension_length, settings.sharpening_leg_length, geometry)) {
            continue;
        }

        modifiedPath |= applySharpCornerGeometry(path, previousIndex, nextIndex, geometry, skipSegment, endpointUpdates,
                                                 insertionsAfter);
    }

    if (!modifiedPath) return;

    Path sharpenedPath;
    for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex) {
        if (skipSegment[segmentIndex]) continue;

        Point start =
            endpointUpdates[segmentIndex].has_start ? endpointUpdates[segmentIndex].start : path[segmentIndex]->start();
        Point end =
            endpointUpdates[segmentIndex].has_end ? endpointUpdates[segmentIndex].end : path[segmentIndex]->end();

        sharpenedPath.append(cloneSegmentWithEndpoints(path[segmentIndex], start, end));
        for (const QSharedPointer<SegmentBase>& insertion : insertionsAfter[segmentIndex])
            sharpenedPath.append(insertion);
    }

    path.clear();
    path.append(sharpenedPath);
}

// to generate tip wipe
void PathModifierGenerator::GenerateTipWipe(Path& path, PathModifiers modifiers, Distance wipeDistance,
                                            Velocity wipeSpeed, Angle wipeAngle, AngularVelocity extruderSpeed,
                                            Distance tipWipeLiftDistance, Distance tipWipeCutoffDistance) {
    tipWipeDistanceCovered = 0;

    if (static_cast<int>(modifiers & PathModifiers::kForwardTipWipe) != 0) {
        int currentIndex            = 0;
        Distance cumulativeDistance = 0;
        Distance wipeLength         = wipeDistance;
        while (wipeDistance > 0) {
            Distance nextSegmentDist = path[currentIndex]->length();

            if (nextSegmentDist == 0) break;

            wipeDistance -= nextSegmentDist;
            cumulativeDistance += nextSegmentDist;
            Distance tempZ;

            Point end;
            if (wipeDistance >= 0) {
                end = Point(path[currentIndex]->end().x(), path[currentIndex]->end().y(),
                            path[currentIndex]->end().z() + (tipWipeLiftDistance * cumulativeDistance / wipeLength));
            }
            else {
                float percentage = 1 - (-wipeDistance() / nextSegmentDist());
                end              = Point(
                    (1.0 - percentage) * path[currentIndex]->start().x() + percentage * path[currentIndex]->end().x(),
                    (1.0 - percentage) * path[currentIndex]->start().y() + percentage * path[currentIndex]->end().y(),
                    path[currentIndex]->end().z() + tipWipeLiftDistance);
            }
            generateTipWipeSegment(
                path, path[currentIndex]->start(), end, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth),
                path[currentIndex]->getSb()->setting<Distance>(SS::kHeight), wipeSpeed,
                path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel), extruderSpeed,
                path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType), PathModifiers::kForwardTipWipe,
                path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber), tipWipeCutoffDistance,
                path[currentIndex]->getSb()->setting<bool>(SS::kAdapted));

            currentIndex = (currentIndex + 1) % path.size();
        }
    }
    else if (static_cast<int>(modifiers & PathModifiers::kAngledTipWipe) != 0) {
        int currentIndex = path.size() - 1;
        // Find difference in X, and Y, between start and end of last segment in path
        float diff_x = path[currentIndex]->end().x() - path[currentIndex]->start().x();
        float diff_y = path[currentIndex]->end().y() - path[currentIndex]->start().y();
        // Calculate the angle of the last segment of the path
        Angle current_angle = atan2(diff_y, diff_x);
        // Add the wipe angle
        Angle new_angle = current_angle + wipeAngle;
        // Find the new X and Y location to wipe to
        Distance new_x = wipeDistance * cos(new_angle) + path[currentIndex]->end().x();
        Distance new_y = wipeDistance * sin(new_angle) + path[currentIndex]->end().y();
        Point end      = Point(new_x, new_y, path[currentIndex]->end().z() + tipWipeLiftDistance);

        generateTipWipeSegment(
            path, path[currentIndex]->end(), end, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth),
            path[currentIndex]->getSb()->setting<Distance>(SS::kHeight), wipeSpeed,
            path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel), extruderSpeed,
            path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType), PathModifiers::kAngledTipWipe,
            path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber), tipWipeCutoffDistance,
            path[currentIndex]->getSb()->setting<bool>(SS::kAdapted));
    }
    else {
        int currentIndex            = path.size() - 1;
        Distance cumulativeDistance = 0;
        Distance wipeLength         = wipeDistance;
        bool isClosed               = false;
        if (path[currentIndex]->end() == path[0]->start()) isClosed = true;

        while (wipeDistance > 0 && ((currentIndex >= 0 && !isClosed) || isClosed)) {
            Distance nextSegmentDist = path[currentIndex]->end().distance(path[currentIndex]->start());
            wipeDistance -= nextSegmentDist;
            cumulativeDistance += nextSegmentDist;

            Point end;
            if (wipeDistance >= 0) {
                end = Point(path[currentIndex]->start().x(), path[currentIndex]->start().y(),
                            path[currentIndex]->start().z() + (tipWipeLiftDistance * cumulativeDistance / wipeLength));
            }
            else {
                float percentage = 1 - (-wipeDistance() / nextSegmentDist());
                end              = Point(
                    (1.0 - percentage) * path[currentIndex]->end().x() + percentage * path[currentIndex]->start().x(),
                    (1.0 - percentage) * path[currentIndex]->end().y() + percentage * path[currentIndex]->start().y(),
                    path[currentIndex]->start().z() + tipWipeLiftDistance);
            }

            generateTipWipeSegment(
                path, path[currentIndex]->end(), end, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth),
                path[currentIndex]->getSb()->setting<Distance>(SS::kHeight), wipeSpeed,
                path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel), extruderSpeed,
                path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType), PathModifiers::kReverseTipWipe,
                path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber), tipWipeCutoffDistance,
                path[currentIndex]->getSb()->setting<bool>(SS::kAdapted));

            currentIndex -= 1;
            if (currentIndex < 0) currentIndex = path.size() - 1;
        }
    }
}

void PathModifierGenerator::GenerateForwardTipWipeOpenLoop(Path& path, PathModifiers modifiers, Distance wipeDistance,
                                                           Velocity wipeSpeed, AngularVelocity extruderSpeed,
                                                           Distance tipWipeLiftDistance, Distance tipWipeCutoffDistance,
                                                           bool clearTipWipDistanceCovered) {
    if (clearTipWipDistanceCovered) tipWipeDistanceCovered = 0;

    int currentIndex = path.size() - 1;
    Distance length  = path[currentIndex]->end().distance(path[currentIndex]->start());
    Distance X       = path[currentIndex]->end().x() +
                       (path[currentIndex]->end().x() - path[currentIndex]->start().x()) / length() * wipeDistance();
    Distance Y       = path[currentIndex]->end().y() +
                       (path[currentIndex]->end().y() - path[currentIndex]->start().y()) / length() * wipeDistance();
    Distance Z       = path[currentIndex]->end().z() + tipWipeLiftDistance;
    Point end(X, Y, Z);

    generateTipWipeSegment(
        path, path[currentIndex]->end(), end, path[currentIndex]->getSb()->setting<Distance>(SS::kWidth),
        path[currentIndex]->getSb()->setting<Distance>(SS::kHeight), wipeSpeed,
        path[currentIndex]->getSb()->setting<Acceleration>(SS::kAccel), extruderSpeed,
        path[currentIndex]->getSb()->setting<RegionType>(SS::kRegionType), PathModifiers::kForwardTipWipe,
        path[currentIndex]->getSb()->setting<int>(SS::kMaterialNumber), tipWipeCutoffDistance,
        path[currentIndex]->getSb()->setting<bool>(SS::kAdapted));
}

void PathModifierGenerator::GenerateSpiralLift(Path& path, Distance spiralWidth, Distance spiralHeight,
                                               int spiralPoints, Velocity spiralLiftVelocity, bool supportsG3) {
    Point startPoint = path.back()->end();

    if (supportsG3) {
        const Angle spiral_angle = 355.0f * degree;
        const Angle end_angle    = (360.0f * degree) - spiral_angle;
        Point spiral_start_point(startPoint.x() + spiralWidth, startPoint.y(), startPoint.z());
        Point spiral_end_point(startPoint.x() + spiralWidth * qCos(end_angle()),
                               startPoint.y() + spiralWidth * qSin(end_angle()), startPoint.z() + spiralHeight);
        Point center_point(startPoint.x(), startPoint.y(), startPoint.z());

        writeSegment(path, startPoint, spiral_start_point, path.back()->getSb()->setting<Distance>(SS::kWidth),
                     spiralHeight, spiralLiftVelocity, path.back()->getSb()->setting<Acceleration>(SS::kAccel), .0f,
                     path.back()->getSb()->setting<RegionType>(SS::kRegionType), PathModifiers::kSpiralLift,
                     path.back()->getSb()->setting<int>(SS::kMaterialNumber),
                     path.back()->getSb()->setting<bool>(SS::kAdapted));

        writeArcSegment(
            path, spiral_start_point, spiral_end_point, center_point, spiral_angle, false,
            path.back()->getSb()->setting<Distance>(SS::kWidth), path.back()->getSb()->setting<Distance>(SS::kHeight),
            spiralLiftVelocity, path.back()->getSb()->setting<Acceleration>(SS::kAccel), .0f,
            path.back()->getSb()->setting<RegionType>(SS::kRegionType), PathModifiers::kSpiralLift,
            path.back()->getSb()->setting<int>(SS::kMaterialNumber), path.back()->getSb()->setting<bool>(SS::kAdapted));
    }
    else {
        float currentZ = startPoint.z();
        Point newStart = startPoint;
        for (int i = 0; i < spiralPoints; ++i) {
            Point newEnd(startPoint.x() - (float(i) / float(spiralPoints) * qCos(i * M_PI / 8.0) * spiralWidth),
                         startPoint.y() - (float(i) / float(spiralPoints) * qSin(i * M_PI / 8.0) * spiralWidth),
                         currentZ);

            currentZ += spiralHeight() / float(spiralPoints);

            writeSegment(path, newStart, newEnd, path.back()->getSb()->setting<Distance>(SS::kWidth), spiralHeight,
                         spiralLiftVelocity, path.back()->getSb()->setting<Acceleration>(SS::kAccel), .0f,
                         path.back()->getSb()->setting<RegionType>(SS::kRegionType), PathModifiers::kSpiralLift,
                         path.back()->getSb()->setting<int>(SS::kMaterialNumber),
                         path.back()->getSb()->setting<bool>(SS::kAdapted));

            newStart = newEnd;
        }
    }
}

void PathModifierGenerator::writeSegment(Path& path, Point start, Point end, Distance width, Distance height,
                                         Velocity speed, Acceleration acceleration, AngularVelocity extruder_speed,
                                         RegionType regionType, PathModifiers pathModifiers, int materialNumber,
                                         bool adapted) {
    QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(start, end);

    segment->getSb()->setSetting(SS::kWidth, width);
    segment->getSb()->setSetting(SS::kHeight, height);
    segment->getSb()->setSetting(SS::kSpeed, speed);
    segment->getSb()->setSetting(SS::kAccel, acceleration);
    segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
    segment->getSb()->setSetting(SS::kRegionType, regionType);
    segment->getSb()->setSetting(SS::kPathModifiers, pathModifiers);
    segment->getSb()->setSetting(SS::kMaterialNumber, materialNumber);
    segment->getSb()->setSetting(SS::kAdapted, adapted);

    path.append(segment);
}

void PathModifierGenerator::writeArcSegment(Path& path, Point start, Point end, Point center, Angle angle, bool ccw,
                                            Distance width, Distance height, Velocity speed, Acceleration acceleration,
                                            AngularVelocity extruder_speed, RegionType regionType,
                                            PathModifiers path_modifiers, int materialNumber, bool adapted) {
    QSharedPointer<ArcSegment> segment = QSharedPointer<ArcSegment>::create(start, end, center, angle, ccw);

    segment->getSb()->setSetting(SS::kWidth, width);
    segment->getSb()->setSetting(SS::kHeight, height);
    segment->getSb()->setSetting(SS::kSpeed, speed);
    segment->getSb()->setSetting(SS::kAccel, acceleration);
    segment->getSb()->setSetting(SS::kExtruderSpeed, extruder_speed);
    segment->getSb()->setSetting(SS::kRegionType, regionType);
    segment->getSb()->setSetting(SS::kPathModifiers, path_modifiers);
    segment->getSb()->setSetting(SS::kMaterialNumber, materialNumber);
    segment->getSb()->setSetting(SS::kAdapted, adapted);

    path.append(segment);
}

void PathModifierGenerator::GenerateRamp(Path& path, bool& segmentSplitted, int segmentIndex,
                                         PathModifiers pathModifiers, Distance rampLength, Velocity speed,
                                         AngularVelocity extruderSpeed) {
    Distance rampLengthCovered = 0;
    int endIndex               = path.size() - 1;
    bool rampDown              = pathModifiers == PathModifiers::kRampingDown;

    while (rampLengthCovered < rampLength) {
        if (rampDown) {
            if (segmentIndex < 0) break;
        }
        else {
            if (segmentIndex > endIndex) break;
        }

        QSharedPointer<SegmentBase> segment = path[segmentIndex];

        PathModifiers segPM = segment->getSb()->setting<PathModifiers>(SS::kPathModifiers);
        if (segPM == PathModifiers::kRampingUp || segPM == PathModifiers::kRampingDown) break;

        if (segment->length() > (rampLength - rampLengthCovered)) {
            double newPDist = ((rampLength - rampLengthCovered) / segment->length())();
            if (!rampDown) newPDist = 1 - newPDist;

            Point startP = segment->start();
            Point endP   = segment->end();
            Point newPV  = Point((startP.x() - endP.x()) * newPDist, (startP.y() - endP.y()) * newPDist,
                                 (startP.z() - endP.z()) * newPDist);
            Point newP   = Point(endP.x() + newPV.x(), endP.y() + newPV.y(), endP.z() + newPV.z());

            segment->setEnd(newP);

            QSharedPointer<LineSegment> newSegment = QSharedPointer<LineSegment>::create(newP, endP);
            copyInitialExtruderSpeed(newSegment->getSb(), segment->getSb());
            copyAdaptedWidthFlag(newSegment->getSb(), segment->getSb());
            newSegment->getSb()->setSetting(SS::kWidth, segment->getSb()->setting<Distance>(SS::kWidth));
            newSegment->getSb()->setSetting(SS::kHeight, segment->getSb()->setting<Distance>(SS::kHeight));
            newSegment->getSb()->setSetting(SS::kAccel, segment->getSb()->setting<Acceleration>(SS::kAccel));
            newSegment->getSb()->setSetting(SS::kMaterialNumber, segment->getSb()->setting<int>(SS::kMaterialNumber));

            RegionType regionType = segment->getSb()->setting<RegionType>(SS::kRegionType);
            if (regionType == RegionType::kUnknown)
                regionType = path[segmentIndex + 1]->getSb()->setting<RegionType>(SS::kRegionType);
            newSegment->getSb()->setSetting(SS::kRegionType, regionType);

            if (rampDown) {
                newSegment->getSb()->setSetting(SS::kSpeed, speed);
                newSegment->getSb()->setSetting(SS::kExtruderSpeed, extruderSpeed);
                newSegment->getSb()->setSetting(SS::kPathModifiers, pathModifiers);
            }
            else {
                newSegment->getSb()->setSetting(SS::kSpeed, segment->getSb()->setting<Velocity>(SS::kSpeed));
                newSegment->getSb()->setSetting(SS::kExtruderSpeed,
                                                segment->getSb()->setting<AngularVelocity>(SS::kExtruderSpeed));

                segment->getSb()->setSetting(SS::kSpeed, speed);
                segment->getSb()->setSetting(SS::kExtruderSpeed, extruderSpeed);
                segment->getSb()->setSetting(SS::kPathModifiers, pathModifiers);
            }

            path.insert(segmentIndex + 1, newSegment);

            segmentSplitted = true;
            break;
        }
        else {
            segment->getSb()->setSetting(SS::kSpeed, speed);
            segment->getSb()->setSetting(SS::kExtruderSpeed, extruderSpeed);
            segment->getSb()->setSetting(SS::kPathModifiers, pathModifiers);
        }

        rampLengthCovered += segment->length();
        segmentIndex += rampDown ? -1 : 1;
    };
}

void PathModifierGenerator::generateTipWipeSegment(Path& path, Point start, Point end, Distance width, Distance height,
                                                   Velocity speed, Acceleration acceleration,
                                                   AngularVelocity extruder_speed, RegionType regionType,
                                                   PathModifiers pathModifiers, int materialNumber,
                                                   Distance tipWipeCutoffDistance, bool adapted) {
    if (tipWipeCutoffDistance > 0) {
        Distance length = end.distance(start);

        if (tipWipeDistanceCovered >= tipWipeCutoffDistance) { extruder_speed = 0; }
        else if (tipWipeDistanceCovered() + length() - tipWipeCutoffDistance() > 0.09) {
            auto ratio = (tipWipeCutoffDistance() - tipWipeDistanceCovered()) / length();
            Point hopPoint(start.x() + ((end.x() - start.x()) * ratio), start.y() + ((end.y() - start.y()) * ratio),
                           end.z());

            writeSegment(path, start, hopPoint, width, height, speed, acceleration, extruder_speed, regionType,
                         pathModifiers, materialNumber, adapted);

            start          = hopPoint;
            extruder_speed = 0;
        }

        tipWipeDistanceCovered += length;
    }

    writeSegment(path, start, end, width, height, speed, acceleration, extruder_speed, regionType, pathModifiers,
                 materialNumber, adapted);
}

Distance PathModifierGenerator::tipWipeDistanceCovered = 0;
}  // namespace ORNL
