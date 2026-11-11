#include "gcode/gcode_segment_filter.h"

#include <QSet>
#include <QVector3D>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "configs/settings_base.h"
#include "geometry/polygon_list.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/bezier.h"
#include "geometry/segments/line.h"
#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL::GCodeSegmentFilter {
namespace {
constexpr double kVectorEpsilon               = 1.0e-6;
constexpr double kVectorEpsilonSquared        = kVectorEpsilon * kVectorEpsilon;
constexpr double kAreaTolerance               = 1.0;
constexpr double kUncoveredAreaRatioTolerance = 0.01;
constexpr int kCurveSegments                  = 32;
constexpr double kArcSegmentAngle             = (2.0 * M_PI) / 48.0;
constexpr PathModifiers kNonBuildModifiers    = PathModifiers::kCoasting | PathModifiers::kForwardTipWipe |
                                                PathModifiers::kPerimeterTipWipe | PathModifiers::kReverseTipWipe |
                                                PathModifiers::kAngledTipWipe | PathModifiers::kSpiralLift;

struct SegmentFootprint {
    QSharedPointer<SegmentBase> segment;
    PolygonList footprint;
    Distance bead_width;
    bool in_closed_loop = false;
};

struct LayerFootprints {
    QVector<SegmentFootprint> segments;
    QVector<const SegmentBase*> unrepresented_segments;
    PolygonList closed_coverage;
    PolygonList wall_enclosed;
    double normal_position = 0.0;
    double max_bead_height = 0.0;
    double max_bead_width  = 0.0;
    bool has_printable     = false;
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

bool hasRegionType(const QSharedPointer<SegmentBase>& segment, RegionType region_type, const QString& region_name);

bool isSupportSegment(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull()) { return false; }
    return hasFlag(segment->displayType(), SegmentDisplayType::kSupport) ||
           hasRegionType(segment, RegionType::kSupport, Constants::RegionTypeStrings::kSupport) ||
           hasRegionType(segment, RegionType::kSupportRoof, Constants::RegionTypeStrings::kSupportRoof);
}

bool isPrintableBead(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull() || isDegenerateLine(segment)) { return false; }
    if (hasNonBuildModifier(segment)) { return false; }

    const SegmentDisplayType type = segment->displayType();
    if (hasFlag(type, SegmentDisplayType::kTravel) || isSupportSegment(segment)) { return false; }
    if (!segment->depositionActive()) { return false; }
    if (segment->displayWidth() <= kVectorEpsilon) { return false; }

    return true;
}

bool hasRegionType(const QSharedPointer<SegmentBase>& segment, RegionType region_type, const QString& region_name) {
    if (segment.isNull()) { return false; }

    const QSharedPointer<SettingsBase> settings = segment->getSb();
    if (!settings.isNull() && settings->contains(SS::kRegionType) &&
        settings->setting<RegionType>(SS::kRegionType) == region_type) {
        return true;
    }

    return segment->m_segment_info_meta.type.contains(region_name, Qt::CaseInsensitive);
}

bool isInfillOrSkinSegment(const QSharedPointer<SegmentBase>& segment) {
    return hasRegionType(segment, RegionType::kInfill, Constants::RegionTypeStrings::kInfill) ||
           hasRegionType(segment, RegionType::kSkin, Constants::RegionTypeStrings::kSkin);
}

bool isSkeletonSegment(const QSharedPointer<SegmentBase>& segment) {
    return hasRegionType(segment, RegionType::kSkeleton, Constants::RegionTypeStrings::kSkeleton);
}

bool isInteriorFillSegment(const QSharedPointer<SegmentBase>& segment) {
    return isInfillOrSkinSegment(segment) || isSkeletonSegment(segment);
}

void markClosedLoopSegments(QVector<SegmentFootprint>& segments) {
    int group_start = 0;
    for (int i = 1; i <= segments.size(); ++i) {
        if (i == segments.size() || !pointsConnected(segments[i - 1].segment->end(), segments[i].segment->start())) {
            const int count = i - group_start;
            bool has_curve  = false;
            for (int j = group_start; j < i; ++j) {
                if (dynamic_cast<ArcSegment*>(segments[j].segment.data()) != nullptr ||
                    dynamic_cast<BezierSegment*>(segments[j].segment.data()) != nullptr) {
                    has_curve = true;
                    break;
                }
            }
            if ((count >= 3 || (count >= 1 && has_curve)) &&
                pointsConnected(segments[i - 1].segment->end(), segments[group_start].segment->start())) {
                for (int j = group_start; j < i; ++j) { segments[j].in_closed_loop = true; }
            }
            group_start = i;
        }
    }
}

QVector3D scaledPoint(const Point& point, double scale) {
    return point.toQVector3D() * static_cast<float>(scale);
}

std::vector<QVector3D> linePoints(const SegmentBase& segment, double scale) {
    return {scaledPoint(segment.start(), scale), scaledPoint(segment.end(), scale)};
}

std::vector<QVector3D> arcPoints(const ArcSegment& arc, double scale) {
    const QVector3D start  = scaledPoint(arc.start(), scale);
    const QVector3D center = scaledPoint(arc.center(), scale);
    const QVector3D end    = scaledPoint(arc.end(), scale);
    const double radius    = std::hypot(start.x() - center.x(), start.y() - center.y());
    const double sweep     = arc.angle()();

    if (!std::isfinite(radius) || !std::isfinite(sweep) || radius <= kVectorEpsilon || sweep <= kVectorEpsilon) {
        return linePoints(arc, scale);
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

std::vector<QVector3D> bezierPoints(BezierSegment& curve, double scale) {
    std::vector<QVector3D> points;
    points.reserve(static_cast<std::size_t>(kCurveSegments) + 1);
    for (int i = 0; i <= kCurveSegments; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kCurveSegments);
        points.push_back(scaledPoint(curve.getPointAlong(t), scale));
    }

    return points;
}

enum class ProjectionPlane { kXY, kYZ, kXZ };

ProjectionPlane detectProjectionPlane(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    double intra_dx = 0.0;
    double intra_dy = 0.0;
    double intra_dz = 0.0;
    for (const auto& layer : gcode) {
        for (const auto& segment : layer) {
            if (segment.isNull()) { continue; }
            intra_dx += std::abs(segment->end().x() - segment->start().x());
            intra_dy += std::abs(segment->end().y() - segment->start().y());
            intra_dz += std::abs(segment->end().z() - segment->start().z());
        }
    }

    // Single vertical line / purely vertical motion: project onto YZ
    if (intra_dz > kVectorEpsilon && intra_dx <= kVectorEpsilon && intra_dy <= kVectorEpsilon) {
        return ProjectionPlane::kYZ;
    }

    // Strict planar layer detection: check if segments lie strictly in a plane (zero intra-layer delta)
    const bool planar_xy = intra_dz <= kVectorEpsilon && (intra_dx > kVectorEpsilon || intra_dy > kVectorEpsilon);
    const bool planar_yz = intra_dx <= kVectorEpsilon && (intra_dy > kVectorEpsilon || intra_dz > kVectorEpsilon);
    const bool planar_xz = intra_dy <= kVectorEpsilon && (intra_dx > kVectorEpsilon || intra_dz > kVectorEpsilon);

    if (planar_xy && !planar_yz && !planar_xz) { return ProjectionPlane::kXY; }
    if (planar_yz && !planar_xy && !planar_xz) { return ProjectionPlane::kYZ; }
    if (planar_xz && !planar_xy && !planar_yz) { return ProjectionPlane::kXZ; }

    if (GSM != nullptr && GSM->getGlobal() != nullptr && GSM->getGlobal()->contains(PS::Slicing::kSlicePlaneNormalX) &&
        GSM->getGlobal()->contains(PS::Slicing::kSlicePlaneNormalY) &&
        GSM->getGlobal()->contains(PS::Slicing::kSlicePlaneNormalZ)) {
        const float nx = std::abs(GSM->getGlobal()->setting<float>(PS::Slicing::kSlicePlaneNormalX));
        const float ny = std::abs(GSM->getGlobal()->setting<float>(PS::Slicing::kSlicePlaneNormalY));
        const float nz = std::abs(GSM->getGlobal()->setting<float>(PS::Slicing::kSlicePlaneNormalZ));
        if (nx > ny && nx > nz) { return ProjectionPlane::kYZ; }
        if (ny > nx && ny > nz) { return ProjectionPlane::kXZ; }
        if (nz > nx && nz > ny) { return ProjectionPlane::kXY; }
    }

    if (intra_dz <= intra_dx && intra_dz <= intra_dy) { return ProjectionPlane::kXY; }
    if (intra_dx <= intra_dy && intra_dx <= intra_dz) { return ProjectionPlane::kYZ; }
    if (intra_dy <= intra_dx && intra_dy <= intra_dz) { return ProjectionPlane::kXZ; }

    return ProjectionPlane::kXY;
}

Point projectPoint(const QVector3D& p, ProjectionPlane plane) {
    switch (plane) {
        case ProjectionPlane::kYZ:
            return Point(p.y(), p.z(), 0.0);
        case ProjectionPlane::kXZ:
            return Point(p.x(), p.z(), 0.0);
        case ProjectionPlane::kXY:
        default:
            return Point(p.x(), p.y(), 0.0);
    }
}

double normalCoordinate(const Point& p, ProjectionPlane plane) {
    switch (plane) {
        case ProjectionPlane::kYZ:
            return p.x();
        case ProjectionPlane::kXZ:
            return p.y();
        case ProjectionPlane::kXY:
        default:
            return p.z();
    }
}

double segmentBeadHeight(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull()) { return 0.0; }
    if (segment->displayHeight() > 0.0f) { return segment->displayHeight() * Constants::OpenGL::kViewToObject; }

    const QSharedPointer<SettingsBase> settings = segment->getSb();
    if (!settings.isNull() && settings->contains(PS::Layer::kLayerHeight)) {
        return settings->setting<Distance>(PS::Layer::kLayerHeight)();
    }

    if (GSM != nullptr && GSM->getGlobal() != nullptr && GSM->getGlobal()->contains(PS::Layer::kLayerHeight)) {
        return GSM->getGlobal()->setting<Distance>(PS::Layer::kLayerHeight)();
    }

    return segment->displayWidth() * Constants::OpenGL::kViewToObject;
}

bool layersInContact(const LayerFootprints& a, const LayerFootprints& b) {
    if (!a.has_printable || !b.has_printable) { return false; }
    const double separation    = std::abs(a.normal_position - b.normal_position);
    const double max_dimension = std::max({a.max_bead_height, b.max_bead_height, a.max_bead_width, b.max_bead_width});
    if (max_dimension <= kVectorEpsilon) { return true; }

    return separation <= (max_dimension * 1.5);
}

PolygonList footprintForSegment(const QSharedPointer<SegmentBase>& segment, ProjectionPlane plane) {
    PolygonList footprint;
    const Distance bead_width(segment->displayWidth() * Constants::OpenGL::kViewToObject);
    const std::vector<QVector3D> points = segmentCenterlinePoints(segment, Constants::OpenGL::kViewToObject);
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const Point proj_start = projectPoint(points[i], plane);
        const Point proj_end   = projectPoint(points[i + 1], plane);
        if ((proj_end.toQVector3D() - proj_start.toQVector3D()).lengthSquared() <= kVectorEpsilonSquared) { continue; }

        footprint += Polyline({proj_start, proj_end}).makeReal(bead_width);
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

bool hasSideExposure(const SegmentFootprint& segment, const PolygonList& layer_interior,
                     const PolygonList& wall_enclosed) {
    if (isInfillOrSkinSegment(segment.segment)) {
        if (!wall_enclosed.isEmpty() && !hasUncoveredArea(segment.footprint, wall_enclosed)) { return false; }
    }

    return hasUncoveredArea(segment.footprint, layer_interior);
}

QVector<LayerFootprints> buildLayerFootprints(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                                              ProjectionPlane plane, const std::function<bool()>& should_cancel) {
    QVector<LayerFootprints> layers(gcode.size());
    std::atomic<bool> cancelled(false);

#if defined(_OPENMP)
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int layer_index = 0; layer_index < gcode.size(); ++layer_index) {
        if (cancelled.load(std::memory_order_relaxed)) { continue; }
        if (should_cancel && should_cancel()) {
            cancelled.store(true, std::memory_order_relaxed);
            continue;
        }

        const QVector<QSharedPointer<SegmentBase>>& layer_segments = gcode[layer_index];
        LayerFootprints& layer                                     = layers[layer_index];
        PolygonList wall_coverage;
        PolygonList infill_coverage;
        Distance max_bead_width = 0;
        double normal_sum       = 0.0;
        int normal_count        = 0;
        double max_height       = 0.0;

        for (const QSharedPointer<SegmentBase>& segment : layer_segments) {
            if (!isPrintableBead(segment)) { continue; }

            const double start_normal = normalCoordinate(segment->start(), plane) * Constants::OpenGL::kViewToObject;
            const double end_normal   = normalCoordinate(segment->end(), plane) * Constants::OpenGL::kViewToObject;
            normal_sum += (start_normal + end_normal) * 0.5;
            normal_count++;
            max_height = std::max(max_height, segmentBeadHeight(segment));

            SegmentFootprint segment_footprint;
            segment_footprint.segment    = segment;
            segment_footprint.bead_width = Distance(segment->displayWidth() * Constants::OpenGL::kViewToObject);
            segment_footprint.footprint  = footprintForSegment(segment, plane);
            if (!hasArea(segment_footprint.footprint)) {
                layer.unrepresented_segments.push_back(segment.data());
                continue;
            }

            max_bead_width = std::max(max_bead_width, segment_footprint.bead_width);
            if (isInteriorFillSegment(segment)) { infill_coverage += segment_footprint.footprint; }
            else { wall_coverage += segment_footprint.footprint; }
            layer.segments.push_back(segment_footprint);
        }

        if (normal_count > 0) {
            layer.normal_position = normal_sum / normal_count;
            layer.max_bead_height = max_height;
            layer.max_bead_width  = max_bead_width();
            layer.has_printable   = true;
        }

        markClosedLoopSegments(layer.segments);

        const PolygonList closed_walls   = closeCoverage(wall_coverage, max_bead_width);
        layer.wall_enclosed              = closed_walls;
        const QVector<PolygonList> parts = closed_walls.splitIntoParts();
        for (const PolygonList& part : parts) {
            for (int h = 1; h < part.size(); ++h) {
                Polygon positive_hole = part[h];
                if (!positive_hole.orientation()) { std::reverse(positive_hole.begin(), positive_hole.end()); }
                PolygonList direct_hole;
                direct_hole += positive_hole;
                for (const PolygonList& other_part : parts) {
                    if (&other_part != &part && !other_part.isEmpty() && !other_part[0].isEmpty() &&
                        positive_hole.inside(other_part[0].first())) {
                        direct_hole -= other_part[0];
                    }
                }
                if (hasArea(infill_coverage & direct_hole)) { layer.wall_enclosed += direct_hole; }
            }
        }

        PolygonList coverage = layer.wall_enclosed;
        coverage += infill_coverage;
        layer.closed_coverage = coverage;
    }

    if (cancelled.load(std::memory_order_relaxed)) { return {}; }

    return layers;
}
}  // namespace

bool pointsConnected(const Point& a, const Point& b) {
    return (a.toQVector3D() - b.toQVector3D()).lengthSquared() <= kVectorEpsilonSquared;
}

bool isDegenerateLine(const QSharedPointer<SegmentBase>& segment) {
    if (dynamic_cast<LineSegment*>(segment.data()) == nullptr) { return false; }

    return (segment->end().toQVector3D() - segment->start().toQVector3D()).lengthSquared() <= kVectorEpsilonSquared;
}

std::vector<QVector3D> segmentCenterlinePoints(const QSharedPointer<SegmentBase>& segment, double scale) {
    if (segment.isNull()) { return {}; }
    if (auto* arc = dynamic_cast<ArcSegment*>(segment.data())) { return arcPoints(*arc, scale); }
    if (auto* bezier = dynamic_cast<BezierSegment*>(segment.data())) { return bezierPoints(*bezier, scale); }

    return linePoints(*segment, scale);
}

bool isNonBuildModifierSegment(const QSharedPointer<SegmentBase>& segment) {
    return hasNonBuildModifier(segment);
}

QSet<const SegmentBase*> externalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                                          const std::function<bool()>& should_cancel) {
    const ProjectionPlane plane           = detectProjectionPlane(gcode);
    const QVector<LayerFootprints> layers = buildLayerFootprints(gcode, plane, should_cancel);
    if ((should_cancel && should_cancel()) || (layers.isEmpty() && !gcode.isEmpty())) { return {}; }

    QVector<int> prev_printable_layer(layers.size(), -1);
    QVector<int> next_printable_layer(layers.size(), -1);
    int last_printable = -1;
    for (int i = 0; i < layers.size(); ++i) {
        prev_printable_layer[i] = last_printable;
        if (!layers[i].closed_coverage.isEmpty()) { last_printable = i; }
    }
    int next_printable = -1;
    for (int i = layers.size() - 1; i >= 0; --i) {
        next_printable_layer[i] = next_printable;
        if (!layers[i].closed_coverage.isEmpty()) { next_printable = i; }
    }

    QVector<QVector<const SegmentBase*>> layer_external(layers.size());
    std::atomic<bool> cancelled(false);

#if defined(_OPENMP)
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int layer_index = 0; layer_index < layers.size(); ++layer_index) {
        if (cancelled.load(std::memory_order_relaxed)) { continue; }
        if (should_cancel && should_cancel()) {
            cancelled.store(true, std::memory_order_relaxed);
            continue;
        }

        const PolygonList empty_coverage;
        const int prev_index              = prev_printable_layer[layer_index];
        const int next_index              = next_printable_layer[layer_index];
        const bool prev_in_contact        = prev_index >= 0 && layersInContact(layers[layer_index], layers[prev_index]);
        const bool next_in_contact        = next_index >= 0 && layersInContact(layers[layer_index], layers[next_index]);
        const PolygonList& lower_coverage = prev_in_contact ? layers[prev_index].closed_coverage : empty_coverage;
        const PolygonList& upper_coverage = next_in_contact ? layers[next_index].closed_coverage : empty_coverage;

        using WidthKey = int64_t;
        std::map<WidthKey, PolygonList> cached_interiors;

        for (const SegmentBase* unrepresented : layers[layer_index].unrepresented_segments) {
            layer_external[layer_index].push_back(unrepresented);
        }

        for (const SegmentFootprint& segment : layers[layer_index].segments) {
            bool side_exposed = false;
            if (!layers[layer_index].closed_coverage.isEmpty()) {
                const auto width_key = static_cast<WidthKey>(std::llround(segment.bead_width()));
                auto it              = cached_interiors.find(width_key);
                if (it == cached_interiors.end()) {
                    it =
                        cached_interiors
                            .emplace(width_key, layers[layer_index].closed_coverage.offset(-(segment.bead_width / 2.0)))
                            .first;
                }
                side_exposed = hasSideExposure(segment, it->second, layers[layer_index].wall_enclosed);
            }

            if (side_exposed || hasUncoveredArea(segment.footprint, lower_coverage) ||
                hasUncoveredArea(segment.footprint, upper_coverage)) {
                layer_external[layer_index].push_back(segment.segment.data());
            }
        }
    }

    if (cancelled.load(std::memory_order_relaxed)) { return {}; }

    QSet<const SegmentBase*> external_segments;
    for (const QVector<const SegmentBase*>& layer_segments : layer_external) {
        for (const SegmentBase* segment_ptr : layer_segments) { external_segments.insert(segment_ptr); }
    }

    return external_segments;
}

void tagInternalSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                         const std::function<bool()>& should_cancel) {
    if (gcode.isEmpty() || (should_cancel && should_cancel())) { return; }

    const QSet<const SegmentBase*> external_segments = externalSegments(gcode, should_cancel);
    if (should_cancel && should_cancel()) { return; }

    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (segment.isNull()) { continue; }
            SegmentDisplayType type = segment->displayType() & ~SegmentDisplayType::kInternal;
            if (isNonBuildModifierSegment(segment) ||
                (isPrintableBead(segment) && !external_segments.contains(segment.data()))) {
                type |= SegmentDisplayType::kInternal;
            }
            segment->setDisplayType(type);
        }
    }
}
}  // namespace ORNL::GCodeSegmentFilter
