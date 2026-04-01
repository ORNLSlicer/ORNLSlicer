#include "step/layer/regions/region_base.h"

#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"

namespace ORNL {
RegionBase::RegionBase(const QSharedPointer<SettingsBase>& sb, const int index,
                       const QVector<SettingsPolygon>& settings_polygons, PolygonList uncut_geometry)
    : m_sb(sb), m_index(index), m_settings_polygons(settings_polygons), m_uncut_geometry(uncut_geometry) {
    // NOP
}

RegionBase::RegionBase(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons)
    : m_sb(sb), m_settings_polygons(settings_polygons) {
    // NOP
}

QVector<Path>& RegionBase::getPaths() { return m_paths; }

void RegionBase::appendPath(const Path& path) { m_paths.append(path); }

QSharedPointer<SettingsBase> RegionBase::getSb() const { return m_sb; }

void RegionBase::setSb(const QSharedPointer<SettingsBase>& sb) { m_sb = sb; }

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

void RegionBase::adjustMultiNozzle() {
    for (auto& path : getPaths()) {
        path.adjustMultiNozzle();
    }
}

void RegionBase::addNozzle(int nozzle) {
    for (Path path : getPaths()) {
        path.addNozzle(nozzle);
    }
}

void RegionBase::setLastSpiral(bool spiral) { m_was_last_region_spiral = spiral; }
} // namespace ORNL
