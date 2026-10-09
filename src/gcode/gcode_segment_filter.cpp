#include "gcode/gcode_segment_filter.h"

#include <QSet>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "configs/settings_base.h"
#include "geometry/polygon_list.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/bezier.h"
#include "geometry/segments/line.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL::GCodeSegmentFilter {
namespace {
constexpr double kVectorEpsilon               = 1.0e-6;
constexpr double kVectorEpsilonSquared        = kVectorEpsilon * kVectorEpsilon;
constexpr double kAreaTolerance               = 1.0;
constexpr double kUncoveredAreaRatioTolerance = 0.20;
constexpr int kCurveSegments                  = 32;
constexpr double kArcSegmentAngle             = (2.0 * M_PI) / 48.0;
constexpr PathModifiers kNonBuildModifiers    = PathModifiers::kCoasting | PathModifiers::kForwardTipWipe |
                                                PathModifiers::kPerimeterTipWipe | PathModifiers::kReverseTipWipe |
                                                PathModifiers::kAngledTipWipe | PathModifiers::kSpiralLift;

struct SegmentFootprint {
    QSharedPointer<SegmentBase> segment;
    PolygonList footprint;
    Distance bead_width;
};

struct LayerFootprints {
    QVector<SegmentFootprint> segments;
    PolygonList coverage;
    PolygonList closed_coverage;
    Distance max_bead_width = 0;
};

bool hasFlag(SegmentDisplayType type, SegmentDisplayType flag) {
    return static_cast<bool>(type & flag);
}

bool hasAny(PathModifiers type, PathModifiers flags) {
    return (type & flags) != PathModifiers::kNone;
}

bool commentContainsNonBuildModifier(const QString& comment) {
    return comment.contains(Constants::PathModifierStrings::kForwardTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kPerimeterTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kReverseTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kAngledTipWipe, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kCoasting, Qt::CaseInsensitive) ||
           comment.contains(Constants::PathModifierStrings::kSpiralLift, Qt::CaseInsensitive);
}

bool hasNonBuildModifier(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull()) { return false; }

    const QSharedPointer<SettingsBase> settings = segment->getSb();
    if (!settings.isNull()) {
        const auto modifiers = static_cast<PathModifiers>(settings->setting<uint>(SS::kPathModifiers));
        if (hasAny(modifiers, kNonBuildModifiers)) { return true; }
    }

    return commentContainsNonBuildModifier(segment->m_segment_info_meta.type);
}

bool isDegenerateLine(const QSharedPointer<SegmentBase>& segment) {
    if (dynamic_cast<LineSegment*>(segment.data()) == nullptr) { return false; }

    return (segment->end().toQVector3D() - segment->start().toQVector3D()).lengthSquared() <= kVectorEpsilonSquared;
}

bool isPrintableBead(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull() || isDegenerateLine(segment)) { return false; }
    if (hasNonBuildModifier(segment)) { return false; }

    const SegmentDisplayType type = segment->displayType();
    if (hasFlag(type, SegmentDisplayType::kTravel) || hasFlag(type, SegmentDisplayType::kSupport)) { return false; }
    if (!segment->depositionActive()) { return false; }
    if (segment->displayWidth() <= kVectorEpsilon) { return false; }

    return true;
}

bool isInfillSegment(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull()) { return false; }

    const QSharedPointer<SettingsBase> settings = segment->getSb();
    if (!settings.isNull() && settings->contains(SS::kRegionType)) {
        const auto region = settings->setting<RegionType>(SS::kRegionType);
        if (region == RegionType::kInfill || region == RegionType::kSkeleton) { return true; }
    }

    const QString& comment = segment->m_segment_info_meta.type;
    return comment.contains(Constants::RegionTypeStrings::kInfill, Qt::CaseInsensitive) ||
           comment.contains(Constants::RegionTypeStrings::kSkeleton, Qt::CaseInsensitive);
}

QVector3D toObjectSpace(const Point& point) {
    return point.toQVector3D() * Constants::OpenGL::kViewToObject;
}

std::vector<QVector3D> linePoints(const SegmentBase& segment) {
    return {toObjectSpace(segment.start()), toObjectSpace(segment.end())};
}

std::vector<QVector3D> arcPoints(const ArcSegment& arc) {
    const QVector3D start  = toObjectSpace(arc.start());
    const QVector3D center = toObjectSpace(arc.center());
    const QVector3D end    = toObjectSpace(arc.end());
    const double radius    = std::hypot(start.x() - center.x(), start.y() - center.y());
    const double sweep     = arc.angle()();

    if (!std::isfinite(radius) || !std::isfinite(sweep) || radius <= kVectorEpsilon || sweep <= kVectorEpsilon) {
        return linePoints(arc);
    }

    const int segment_count   = std::max(1, static_cast<int>(std::ceil(sweep / kArcSegmentAngle)));
    const double start_angle  = std::atan2(start.y() - center.y(), start.x() - center.x());
    const double signed_sweep = arc.counterclockwise() ? sweep : -sweep;
    const double z_delta      = end.z() - start.z();

    std::vector<QVector3D> points;
    points.reserve(static_cast<std::size_t>(segment_count) + 1);
    points.push_back(start);
    for (int i = 1; i < segment_count; ++i) {
        const double t     = static_cast<double>(i) / static_cast<double>(segment_count);
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
    if (auto* arc = dynamic_cast<ArcSegment*>(segment.data())) { return arcPoints(*arc); }
    if (auto* bezier = dynamic_cast<BezierSegment*>(segment.data())) { return bezierPoints(*bezier); }

    return linePoints(*segment);
}

PolygonList footprintForSegment(const QSharedPointer<SegmentBase>& segment) {
    PolygonList footprint;
    const Distance bead_width(segment->displayWidth() * Constants::OpenGL::kViewToObject);
    const std::vector<QVector3D> points = segmentPoints(segment);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        if ((points[i + 1] - points[i]).lengthSquared() <= kVectorEpsilonSquared) { continue; }

        footprint += Polyline({Point(points[i]), Point(points[i + 1])}).makeReal(bead_width);
    }

    return footprint;
}

double netArea(PolygonList polygons) {
    return std::abs(polygons.netArea()());
}

bool hasArea(const PolygonList& polygons) {
    return netArea(polygons) > kAreaTolerance;
}

PolygonList closeCoverage(const PolygonList& coverage, Distance max_bead_width) {
    if (coverage.isEmpty() || max_bead_width <= 0) { return coverage; }
    const Distance closure = max_bead_width * 0.75;
    return coverage.offset(closure).offset(-closure);
}

bool hasUncoveredArea(const PolygonList& footprint, const PolygonList& coverage) {
    const double footprint_area = netArea(footprint);
    if (footprint_area <= kAreaTolerance) { return false; }
    if (coverage.isEmpty()) { return true; }

    const double uncovered_area = netArea(footprint - coverage);
    return uncovered_area > std::max(kAreaTolerance, footprint_area * kUncoveredAreaRatioTolerance);
}

bool hasSideExposure(const SegmentFootprint& segment, const PolygonList& layer_interior) {
    if (isInfillSegment(segment.segment)) { return false; }

    return hasUncoveredArea(segment.footprint, layer_interior);
}

QVector<LayerFootprints> buildLayerFootprints(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    QVector<LayerFootprints> layers;
    layers.reserve(gcode.size());

    for (const QVector<QSharedPointer<SegmentBase>>& layer_segments : gcode) {
        LayerFootprints layer;
        PolygonList wall_coverage;
        PolygonList infill_coverage;

        for (const QSharedPointer<SegmentBase>& segment : layer_segments) {
            if (!isPrintableBead(segment)) { continue; }

            SegmentFootprint segment_footprint;
            segment_footprint.segment    = segment;
            segment_footprint.bead_width = Distance(segment->displayWidth() * Constants::OpenGL::kViewToObject);
            segment_footprint.footprint  = footprintForSegment(segment);
            if (!hasArea(segment_footprint.footprint)) { continue; }

            layer.max_bead_width = std::max(layer.max_bead_width, segment_footprint.bead_width);
            if (isInfillSegment(segment)) {
                infill_coverage += segment_footprint.footprint;
            } else {
                wall_coverage += segment_footprint.footprint;
            }
            layer.segments.push_back(segment_footprint);
        }

        layer.coverage                   = wall_coverage + infill_coverage;
        const QVector<PolygonList> parts = wall_coverage.splitIntoParts();
        for (const PolygonList& part : parts) {
            if (part.isEmpty()) { continue; }
            for (int h = 1; h < part.size(); ++h) {
                Polygon positive_hole = part[h];
                if (!positive_hole.orientation()) {
                    std::reverse(positive_hole.begin(), positive_hole.end());
                }
                const double hole_area = std::abs(positive_hole.area()());
                PolygonList direct_hole;
                direct_hole += positive_hole;
                for (const PolygonList& other_part : parts) {
                    if (&other_part != &part && !other_part.isEmpty() &&
                        std::abs(other_part[0].area()()) < hole_area) {
                        direct_hole -= other_part[0];
                    }
                }
                if (hasArea(infill_coverage & direct_hole)) {
                    layer.coverage += direct_hole;
                }
            }
        }

        layer.closed_coverage = closeCoverage(layer.coverage, layer.max_bead_width);

        layers.push_back(layer);
    }

    return layers;
}

QSet<const SegmentBase*> externalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    QSet<const SegmentBase*> external_segments;
    const QVector<LayerFootprints> layers = buildLayerFootprints(gcode);

    for (int layer_index = 0; layer_index < layers.size(); ++layer_index) {
        const PolygonList empty_coverage;
        const PolygonList& lower_coverage =
            layer_index > 0 ? layers[layer_index - 1].closed_coverage : empty_coverage;
        const PolygonList& upper_coverage =
            layer_index + 1 < layers.size() ? layers[layer_index + 1].closed_coverage : empty_coverage;

        using WidthKey = int64_t;
        std::map<WidthKey, PolygonList> cached_interiors;

        for (const SegmentFootprint& segment : layers[layer_index].segments) {
            bool side_exposed = false;
            if (!layers[layer_index].closed_coverage.isEmpty()) {
                const auto width_key = static_cast<WidthKey>(std::llround(segment.bead_width()));
                auto it              = cached_interiors.find(width_key);
                if (it == cached_interiors.end()) {
                    it = cached_interiors
                             .emplace(width_key,
                                      layers[layer_index].closed_coverage.offset(-(segment.bead_width / 2.0)))
                             .first;
                }
                side_exposed = hasSideExposure(segment, it->second);
            }

            if (side_exposed ||
                hasUncoveredArea(segment.footprint, lower_coverage) ||
                hasUncoveredArea(segment.footprint, upper_coverage)) {
                external_segments.insert(segment.segment.data());
            }
        }
    }

    return external_segments;
}
}  // namespace

bool isNonBuildModifierSegment(const QSharedPointer<SegmentBase>& segment) {
    return hasNonBuildModifier(segment);
}

void tagInternalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    const QSet<const SegmentBase*> external_segments = externalSegments(gcode);
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (isNonBuildModifierSegment(segment) ||
                (isPrintableBead(segment) && !external_segments.contains(segment.data()))) {
                segment->setDisplayType(segment->displayType() | SegmentDisplayType::kInternal);
            }
        }
    }
}
}  // namespace ORNL::GCodeSegmentFilter
