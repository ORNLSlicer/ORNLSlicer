#include "step/layer/regions/region_base.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <qcontainerfwd.h>
#include <qlist.h>
#include <qquaternion.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/segment_base.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "geometry/settings_polygon.h"
#include "optimizers/optimization_anchor.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
bool selectedSyntaxSupportsArcFitting(const QSharedPointer<SettingsBase>& global_sb) {
    const GcodeSyntax syntax = global_sb->setting<GcodeSyntax>(PRS::MachineSetup::kSyntax);
    switch (syntax) {
        // These writers inherit WriterBase::writeArc(), which emits no motion.
        case GcodeSyntax::kAdamantine:
        case GcodeSyntax::kMVP:
            return false;
        default:
            return true;
    }
}

bool planarArcFittingAllowed(const QSharedPointer<SettingsBase>& global_sb) {
    if (global_sb == nullptr)
        return false;

    if (!global_sb->setting<bool>(PRS::MachineSetup::kSupportG3))
        return false;

    if (!selectedSyntaxSupportsArcFitting(global_sb))
        return false;

    if (static_cast<SlicingMode>(global_sb->setting<int>(PS::Slicing::kSlicingMode)) != SlicingMode::kPlanar)
        return false;

    constexpr double kVectorTolerance = 1.0e-6;
    return std::abs(global_sb->setting<float>(PS::Slicing::kSlicePlaneNormalX)) <= kVectorTolerance &&
           std::abs(global_sb->setting<float>(PS::Slicing::kSlicePlaneNormalY)) <= kVectorTolerance &&
           std::abs(global_sb->setting<float>(PS::Slicing::kSlicePlaneNormalZ) - 1.0f) <= kVectorTolerance;
}

Point flattenIntoOptimizationFrame(const Point& point, const Plane& slicing_plane, const Point& optimization_shift) {
    const QVector3D normal = slicing_plane.normal();
    if (normal.isNull())
        return point;

    const QQuaternion rotation = MathUtils::CreateQuaternion(normal, QVector3D(0, 0, 1));
    const QVector3D shifted = (point - optimization_shift).toQVector3D();
    return Point::fromQVector3D(rotation.rotatedVector(shifted)) + optimization_shift;
}
} // namespace

RegionBase::RegionBase(const QSharedPointer<SettingsBase>& sb, const int index,
                       const QVector<SettingsPolygon>& settings_polygons, PolygonList uncut_geometry,
                       RegionType region_type)
    : m_sb(sb), m_settings_polygons(settings_polygons), m_index(index), m_region_type(region_type),
      m_uncut_geometry(uncut_geometry) {
    // NOP
}

RegionBase::RegionBase(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons,
                       RegionType region_type)
    : m_sb(sb), m_settings_polygons(settings_polygons), m_region_type(region_type) {
    // NOP
}

QVector<Path>& RegionBase::getPaths() { return m_paths; }

std::optional<Point> RegionBase::getFirstPrintingStartPoint() {
    for (Path& path : m_paths) {
        for (const QSharedPointer<SegmentBase>& segment : path.getSegments()) {
            if (segment->isPrintingSegment())
                return segment->start();
        }
    }

    return std::nullopt;
}

void RegionBase::setPreviousLayerStartPoint(const std::optional<Point>& point) {
    if (point.has_value())
        m_previous_layer_start_point = flattenIntoOptimizationFrame(*point, m_optimization_slicing_plane,
                                                                    m_optimization_shift);
    else
        m_previous_layer_start_point = std::nullopt;
}

std::optional<Point> RegionBase::getPreviousLayerStartPoint() const { return m_previous_layer_start_point; }

void RegionBase::appendPath(const Path& path) { m_paths.append(path); }

QSharedPointer<SettingsBase> RegionBase::getSb() const { return m_sb; }

void RegionBase::setSb(const QSharedPointer<SettingsBase>& sb) { m_sb = sb; }

void RegionBase::setOptimizationFrame(const Plane& slicing_plane, const Point& optimization_shift) {
    m_optimization_slicing_plane = slicing_plane;
    m_optimization_shift = optimization_shift;
}

Point RegionBase::customPathOrderPoint() const {
    return OptimizationAnchor::customPathOrderPoint(m_sb, m_optimization_slicing_plane, m_optimization_shift);
}

Point RegionBase::customPointOrderPoint() const {
    return OptimizationAnchor::customPointOrderPoint(m_sb, m_optimization_slicing_plane, m_optimization_shift);
}

void RegionBase::transform(QQuaternion rotation, Point shift) {
    // rotate and the shift every path in this region
    for (Path path : m_paths) {
        path.transform(rotation, shift);
    }
}

float RegionBase::getMinZ() {
    // find the minimun of the paths is this region
    float region_min = std::numeric_limits<float>::max();
    for (Path path : m_paths) {
        float path_min = path.getMinZ();
        if (path_min < region_min)
            region_min = path_min;
    }
    return region_min;
}

PolygonList RegionBase::getGeometry() const { return m_geometry; }

void RegionBase::setGeometry(const PolygonList& geometry) { m_geometry = geometry; }

void RegionBase::reversePaths() { std::reverse(m_paths.begin(), m_paths.end()); }

int RegionBase::getIndex() { return m_index; }

RegionType RegionBase::getRegionType() const { return m_region_type; }

void RegionBase::setOptimizedLayerNumber(int layer_number) { m_optimized_layer_number = layer_number; }

int RegionBase::getOptimizedLayerNumber() const { return m_optimized_layer_number; }

int RegionBase::getMaterialNumber() { return m_material_number; }

void RegionBase::setMaterialNumber(int material_number) { m_material_number = material_number; }

void RegionBase::calculateMultiMaterialTransition(Distance& transition_distance, int next_material_number) {
    // Step backwards through the paths, evaluating each segment
    for (int i = m_paths.size() - 1; i >= 0; --i) {
        // Step backwards through the segments of the path to find where the transition distance is achieved
        QList<QSharedPointer<SegmentBase>> current_segments = m_paths[i].getSegments();
        for (int j = current_segments.size() - 1; j >= 0; --j) {
            if (!current_segments[j]->isPrintingSegment()) {
                // Update material number so it matches other segments, excluding travels
                if (dynamic_cast<TravelSegment*>(current_segments[j].data()) == nullptr)
                    current_segments[j]->getSb()->setSetting(SS::kMaterialNumber, next_material_number);
            }
            else {
                // If segment length is long enough to exceed transition distance, the segment must be broken to achieve
                // the exact transition distance
                Distance next_segment_distance = current_segments[j]->end().distance(current_segments[j]->start());
                if (next_segment_distance > transition_distance) {
                    float percentage = ((next_segment_distance - transition_distance) / next_segment_distance)();
                    Point end = Point((1.0 - percentage) * current_segments[j]->start().x() +
                                          percentage * current_segments[j]->end().x(),
                                      (1.0 - percentage) * current_segments[j]->start().y() +
                                          percentage * current_segments[j]->end().y());

                    Point old_end = current_segments[j]->end();
                    current_segments[j]->setEnd(end);

                    QSharedPointer<LineSegment> segment = QSharedPointer<LineSegment>::create(end, old_end);
                    segment->getSb()->populate(current_segments[j]->getSb());
                    segment->getSb()->setSetting(SS::kWidth,
                                                 current_segments[j]->getSb()->setting<Distance>(SS::kWidth));
                    segment->getSb()->setSetting(SS::kHeight,
                                                 current_segments[j]->getSb()->setting<Distance>(SS::kHeight));
                    segment->getSb()->setSetting(SS::kSpeed,
                                                 current_segments[j]->getSb()->setting<Distance>(SS::kSpeed));
                    segment->getSb()->setSetting(SS::kAccel,
                                                 current_segments[j]->getSb()->setting<Distance>(SS::kAccel));
                    segment->getSb()->setSetting(SS::kExtruderSpeed,
                                                 current_segments[j]->getSb()->setting<Distance>(SS::kExtruderSpeed));
                    segment->getSb()->setSetting(SS::kMaterialNumber, next_material_number);
                    segment->getSb()->setSetting(SS::kRegionType,
                                                 current_segments[j]->getSb()->setting<Distance>(SS::kRegionType));

                    m_paths[i].insert(j + 1, segment);
                }
                // Segment is shorter than transition distance, update its material number and continue
                else {
                    current_segments[j]->getSb()->setSetting(SS::kMaterialNumber, next_material_number);
                }
                transition_distance -= next_segment_distance;
            }
            if (transition_distance <= 0)
                break;
        }
        if (transition_distance <= 0)
            break;
    }
}

void RegionBase::fitCircularArcs(const QSharedPointer<SettingsBase>& global_sb) {
    if (!planarArcFittingAllowed(global_sb))
        return;

    for (Path& path : m_paths)
        path.fitCircularArcs(m_sb);
}

void RegionBase::setLastSpiral(bool spiral) { m_was_last_region_spiral = spiral; }
} // namespace ORNL
