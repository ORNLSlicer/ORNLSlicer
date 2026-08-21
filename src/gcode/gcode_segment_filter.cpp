#include "gcode/gcode_segment_filter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <QSet>
#include <QVector3D>

#include "configs/settings_base.h"
#include "geometry/polygon_list.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/bezier.h"
#include "geometry/segments/line.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL::GCodeSegmentFilter {
namespace {
constexpr double kVectorEpsilon = 1.0e-6;
constexpr double kVectorEpsilonSquared = kVectorEpsilon * kVectorEpsilon;
constexpr double kAreaTolerance = 1.0;
constexpr double kUncoveredAreaRatioTolerance = 0.01;
constexpr int kCurveSegments = 32;
constexpr double kArcSegmentAngle = (2.0 * 3.14159265358979323846) / 48.0;
constexpr PathModifiers kNonBuildModifiers = PathModifiers::kCoasting | PathModifiers::kForwardTipWipe |
                                             PathModifiers::kPerimeterTipWipe | PathModifiers::kReverseTipWipe |
                                             PathModifiers::kAngledTipWipe | PathModifiers::kSpiralLift;

struct SegmentFootprint {
    QSharedPointer<SegmentBase> segment;
    PolygonList footprint;
    Distance bead_width;
    bool side_exposure_candidate = false;
};

struct LayerFootprints {
    QVector<SegmentFootprint> segments;
    PolygonList coverage;
};

struct ClosedPathGroup {
    int first = 0;
    int past_last = 0;
    Polygon polygon;
    double area = 0.0;
};

bool hasFlag(SegmentDisplayType type, SegmentDisplayType flag) { return static_cast<bool>(type & flag); }

bool hasAny(PathModifiers type, PathModifiers flags) { return (type & flags) != PathModifiers::kNone; }

bool commentContainsNonBuildModifier(const QString& comment) {
    return comment.contains(Constants::PathModifierStrings::kForwardTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kPerimeterTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kReverseTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kAngledTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kCoasting, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kSpiralLift, Qt::CaseInsensitive);
}

bool hasNonBuildModifier(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull()) {
        return false;
    }

    const QSharedPointer<SettingsBase> settings = segment->getSb();
    if (!settings.isNull()) {
        const auto modifiers = static_cast<PathModifiers>(settings->setting<uint>(SS::kPathModifiers));
        if (hasAny(modifiers, kNonBuildModifiers)) {
            return true;
        }
    }

    return commentContainsNonBuildModifier(segment->m_segment_info_meta.type);
}

bool isDegenerateLine(const QSharedPointer<SegmentBase>& segment) {
    if (dynamic_cast<LineSegment*>(segment.data()) == nullptr) {
        return false;
    }

    return (segment->end().toQVector3D() - segment->start().toQVector3D()).lengthSquared() <= kVectorEpsilonSquared;
}

bool isPrintableBead(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull() || isDegenerateLine(segment)) {
        return false;
    }
    if (hasNonBuildModifier(segment)) {
        return false;
    }

    const SegmentDisplayType type = segment->displayType();
    if (hasFlag(type, SegmentDisplayType::kTravel) || hasFlag(type, SegmentDisplayType::kSupport)) {
        return false;
    }
    if (!segment->depositionActive()) {
        return false;
    }
    if (segment->displayWidth() <= kVectorEpsilon) {
        return false;
    }

    return true;
}

QVector3D toObjectSpace(const Point& point) { return point.toQVector3D() * Constants::OpenGL::kViewToObject; }

Point pointFromObjectVector(const QVector3D& point) { return Point(point.x(), point.y(), point.z()); }

bool pointsConnected(const Point& a, const Point& b) {
    return (a.toQVector3D() - b.toQVector3D()).lengthSquared() <= kVectorEpsilonSquared;
}

std::vector<QVector3D> linePoints(const SegmentBase& segment) {
    return {toObjectSpace(segment.start()), toObjectSpace(segment.end())};
}

std::vector<QVector3D> arcPoints(const ArcSegment& arc) {
    const QVector3D start = toObjectSpace(arc.start());
    const QVector3D center = toObjectSpace(arc.center());
    const QVector3D end = toObjectSpace(arc.end());
    const double radius = std::hypot(start.x() - center.x(), start.y() - center.y());
    const double sweep = arc.angle()();

    if (!std::isfinite(radius) || !std::isfinite(sweep) || radius <= kVectorEpsilon || sweep <= kVectorEpsilon) {
        return linePoints(arc);
    }

    const int segment_count = std::max(1, static_cast<int>(std::ceil(sweep / kArcSegmentAngle)));
    const double start_angle = std::atan2(start.y() - center.y(), start.x() - center.x());
    const double signed_sweep = arc.counterclockwise() ? sweep : -sweep;
    const double z_delta = end.z() - start.z();

    std::vector<QVector3D> points;
    points.reserve(static_cast<std::size_t>(segment_count) + 1);
    points.push_back(start);
    for (int i = 1; i < segment_count; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(segment_count);
        const double angle = start_angle + (signed_sweep * t);
        points.push_back(QVector3D(center.x() + (radius * std::cos(angle)), center.y() + (radius * std::sin(angle)),
                                   start.z() + (z_delta * t)));
    }
    points.push_back(end);

    return points;
}

std::vector<QVector3D> bezierPoints(BezierSegment& curve) {
    std::vector<QVector3D> points;
    points.reserve(static_cast<std::size_t>(kCurveSegments) + 1);
    for (int i = 0; i <= kCurveSegments; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kCurveSegments);
        points.push_back(toObjectSpace(curve.getPointAlong(t)));
    }

    return points;
}

std::vector<QVector3D> segmentPoints(const QSharedPointer<SegmentBase>& segment) {
    if (auto* arc = dynamic_cast<ArcSegment*>(segment.data())) {
        return arcPoints(*arc);
    }
    if (auto* bezier = dynamic_cast<BezierSegment*>(segment.data())) {
        return bezierPoints(*bezier);
    }

    return linePoints(*segment);
}

PolygonList footprintForSegment(const QSharedPointer<SegmentBase>& segment) {
    PolygonList footprint;
    const Distance bead_width(segment->displayWidth() * Constants::OpenGL::kViewToObject);
    const std::vector<QVector3D> points = segmentPoints(segment);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        if ((points[i + 1] - points[i]).lengthSquared() <= kVectorEpsilonSquared) {
            continue;
        }

        footprint += Polyline({pointFromObjectVector(points[i]), pointFromObjectVector(points[i + 1])})
                         .makeReal(bead_width);
    }

    return footprint;
}

double netArea(PolygonList polygons) { return std::abs(polygons.netArea()()); }

bool hasArea(PolygonList polygons) { return netArea(polygons) > kAreaTolerance; }

bool hasUncoveredArea(const PolygonList& footprint, const PolygonList& coverage) {
    const double footprint_area = netArea(footprint);
    if (footprint_area <= kAreaTolerance) {
        return false;
    }
    if (coverage.isEmpty()) {
        return true;
    }

    const double uncovered_area = netArea(footprint - coverage);
    return uncovered_area > std::max(kAreaTolerance, footprint_area * kUncoveredAreaRatioTolerance);
}

bool hasSideExposure(const SegmentFootprint& segment, const PolygonList& layer_coverage) {
    if (!segment.side_exposure_candidate || layer_coverage.isEmpty()) {
        return false;
    }

    const PolygonList layer_interior = layer_coverage.offset(-(segment.bead_width / 2.0));
    return hasUncoveredArea(segment.footprint, layer_interior);
}

Polygon centerlinePolygonForGroup(const QVector<SegmentFootprint>& segments, int first, int past_last) {
    Polyline centerline;
    for (int i = first; i < past_last; ++i) {
        const std::vector<QVector3D> points = segmentPoints(segments[i].segment);
        for (const QVector3D& point : points) {
            const Point centerline_point = pointFromObjectVector(point);
            if (centerline.isEmpty() || !pointsConnected(centerline.back(), centerline_point)) {
                centerline.push_back(centerline_point);
            }
        }
    }

    if (centerline.size() < 3) {
        return {};
    }
    if (!pointsConnected(centerline.back(), centerline.front())) {
        centerline.push_back(centerline.front());
    }

    return centerline.close();
}

bool groupContainsGroup(const ClosedPathGroup& container, const ClosedPathGroup& candidate) {
    if (container.area <= candidate.area + kAreaTolerance || candidate.polygon.isEmpty()) {
        return false;
    }

    return container.polygon.inside(candidate.polygon.first(), false);
}

void tagClosedPathSegments(LayerFootprints& layer) {
    QVector<ClosedPathGroup> closed_groups;

    const auto markGroup = [&layer](int first, int past_last) {
        for (int i = first; i < past_last; ++i) {
            layer.segments[i].side_exposure_candidate = true;
        }
    };

    const auto addClosedGroup = [&layer, &closed_groups](int first, int past_last) {
        if (first >= past_last) {
            return;
        }

        const bool closed =
            pointsConnected(layer.segments[past_last - 1].segment->end(), layer.segments[first].segment->start());
        if (!closed) {
            return;
        }

        ClosedPathGroup group;
        group.first = first;
        group.past_last = past_last;
        group.polygon = centerlinePolygonForGroup(layer.segments, first, past_last);
        group.area = std::abs(group.polygon.area()());
        if (group.area > kAreaTolerance) {
            closed_groups.push_back(group);
        }
    };

    int group_start = 0;
    for (int i = 1; i <= layer.segments.size(); ++i) {
        if (i == layer.segments.size() ||
            !pointsConnected(layer.segments[i - 1].segment->end(), layer.segments[i].segment->start())) {
            addClosedGroup(group_start, i);
            group_start = i;
        }
    }

    for (const ClosedPathGroup& group : closed_groups) {
        const bool contained_by_another_group =
            std::any_of(closed_groups.cbegin(), closed_groups.cend(), [&group](const ClosedPathGroup& other_group) {
                return &other_group != &group && groupContainsGroup(other_group, group);
            });
        if (!contained_by_another_group) {
            markGroup(group.first, group.past_last);
        }
    }
}

QVector<LayerFootprints> buildLayerFootprints(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    QVector<LayerFootprints> layers;
    layers.reserve(gcode.size());

    for (const QVector<QSharedPointer<SegmentBase>>& layer_segments : gcode) {
        LayerFootprints layer;
        for (const QSharedPointer<SegmentBase>& segment : layer_segments) {
            if (!isPrintableBead(segment)) {
                continue;
            }

            SegmentFootprint segment_footprint;
            segment_footprint.segment = segment;
            segment_footprint.bead_width = Distance(segment->displayWidth() * Constants::OpenGL::kViewToObject);
            segment_footprint.footprint = footprintForSegment(segment);
            if (!hasArea(segment_footprint.footprint)) {
                continue;
            }

            layer.coverage += segment_footprint.footprint;
            layer.segments.push_back(segment_footprint);
        }

        tagClosedPathSegments(layer);
        layers.push_back(layer);
    }

    return layers;
}
} // namespace

bool isNonBuildModifierSegment(const QSharedPointer<SegmentBase>& segment) { return hasNonBuildModifier(segment); }

QSet<const SegmentBase*> externalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    QSet<const SegmentBase*> external_segments;
    const QVector<LayerFootprints> layers = buildLayerFootprints(gcode);

    for (int layer_index = 0; layer_index < layers.size(); ++layer_index) {
        const PolygonList empty_coverage;
        const PolygonList& lower_coverage = layer_index > 0 ? layers[layer_index - 1].coverage : empty_coverage;
        const PolygonList& upper_coverage =
            layer_index + 1 < layers.size() ? layers[layer_index + 1].coverage : empty_coverage;

        for (const SegmentFootprint& segment : layers[layer_index].segments) {
            if (hasSideExposure(segment, layers[layer_index].coverage) ||
                hasUncoveredArea(segment.footprint, lower_coverage) ||
                hasUncoveredArea(segment.footprint, upper_coverage)) {
                external_segments.insert(segment.segment.data());
            }
        }
    }

    return external_segments;
}

void tagInternalSegments(QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    const QSet<const SegmentBase*> external_segments = externalSegments(gcode);
    for (QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (isNonBuildModifierSegment(segment) ||
                (isPrintableBead(segment) && !external_segments.contains(segment.data()))) {
                segment->setDisplayType(segment->displayType() | SegmentDisplayType::kInternal);
            }
        }
    }
}
} // namespace ORNL::GCodeSegmentFilter
