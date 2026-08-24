#include "step/layer/island/island_base.h"

#include <limits>
#include <optional>
#include <utility>

#include <qcontainerfwd.h>
#include <qlist.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "step/layer/regions/brim.h"
#include "step/layer/regions/infill.h"
#include "step/layer/regions/inset.h"
#include "step/layer/regions/laser_scan.h"
#include "step/layer/regions/perimeter.h"
#include "step/layer/regions/raft.h"
#include "step/layer/regions/region_base.h"
#include "step/layer/regions/skeleton.h"
#include "step/layer/regions/skin.h"
#include "step/layer/regions/skirt.h"
#include "step/layer/regions/support.h"
#include "step/layer/regions/thermal_scan.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
IslandBase::IslandBase(const PolygonList& geometry, const QSharedPointer<SettingsBase>& sb,
                       const QVector<SettingsPolygon>& settings_polygons)
    : m_geometry(geometry), m_sb(sb), m_settings_polygons(settings_polygons) {
    // NOP
}

void IslandBase::markRegionStartSegment() {
    if (m_regions.size() > 0) {
        auto firstRegion = m_regions.first();
        if (firstRegion->getPaths().size() > 0 && firstRegion->getIndex() == 0) {
            auto firstSegment = firstRegion->getPaths().first();
            if (firstSegment.size() > 0) {
                auto sb = firstSegment.begin()->data()->getSb();
                sb->setSetting(SS::kIsRegionStartSegment, true);
            }
        }
    }
}

QString IslandBase::writeGCode(QSharedPointer<WriterBase> writer) {
    QString ret;

    for (auto r : m_regions) {
        if (r->getPaths().size() > 0) ret += r->writeGCode(writer);
    }

    return ret;
}

void IslandBase::addRegion(QSharedPointer<RegionBase> region) {
    m_regions.push_back(region);
}

const QList<QSharedPointer<RegionBase>> IslandBase::getRegions() const {
    return m_regions;
}

QSharedPointer<RegionBase> IslandBase::getRegion(RegionType type) {
    switch (type) {
        case RegionType::kPerimeter:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Perimeter> perimeter = r.dynamicCast<Perimeter>();
                if (perimeter.isNull()) continue;

                return std::move(perimeter);
            }
            break;
        case RegionType::kInset:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Inset> inset = r.dynamicCast<Inset>();
                if (inset.isNull()) continue;

                return std::move(inset);
            }
            break;
        case RegionType::kSkin:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Skin> skin = r.dynamicCast<Skin>();
                if (skin.isNull()) continue;

                return std::move(skin);
            }
            break;
        case RegionType::kInfill:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Infill> infill = r.dynamicCast<Infill>();
                if (infill.isNull()) continue;

                return std::move(infill);
            }
            break;
        case RegionType::kSkeleton:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Skeleton> skeleton = r.dynamicCast<Skeleton>();
                if (skeleton.isNull()) continue;

                return std::move(skeleton);
            }
            break;
        case RegionType::kBrim:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Brim> brim = r.dynamicCast<Brim>();
                if (brim.isNull()) continue;

                return std::move(brim);
            }
            break;
        case RegionType::kSkirt:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Skirt> skirt = r.dynamicCast<Skirt>();
                if (skirt.isNull()) continue;

                return std::move(skirt);
            }
            break;
        case RegionType::kRaft:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Raft> raft = r.dynamicCast<Raft>();
                if (raft.isNull()) continue;

                return std::move(raft);
            }
            break;
        case RegionType::kSupport:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Support> support = r.dynamicCast<Support>();
                if (support.isNull()) continue;

                return std::move(support);
            }
            break;
        case RegionType::kLaserScan:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<LaserScan> laserscan = r.dynamicCast<LaserScan>();
                if (laserscan.isNull()) continue;

                return std::move(laserscan);
            }
            break;
        case RegionType::kThermalScan:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<ThermalScan> thermalscan = r.dynamicCast<ThermalScan>();
                if (thermalscan.isNull()) continue;

                return std::move(thermalscan);
            }
            break;
        case RegionType::kSupportRoof:
        case RegionType::kUnknown:
        default:
            return nullptr;
    }

    return nullptr;
}

const PolygonList& IslandBase::getGeometry() const {
    return m_geometry;
}

void IslandBase::compute(uint layer_num) {
    PolygonList pl = m_geometry;

    for (QSharedPointer<RegionBase> r : m_regions) {
        r->setGeometry(pl);
        r->compute(layer_num);
        pl = r->getGeometry();
    }
}

QSharedPointer<SettingsBase> IslandBase::getSb() const {
    return m_sb;
}

void IslandBase::setSb(const QSharedPointer<SettingsBase>& sb) {
    m_sb = sb;

    // For each region in the stages, populate their sb's with the island's.
    for (auto r : m_regions) { r->setSb(m_sb); }
}

void IslandBase::setOptimizationFrame(const Plane& slicing_plane, const Point& optimization_shift) {
    for (auto r : m_regions) { r->setOptimizationFrame(slicing_plane, optimization_shift); }
}

IslandType IslandBase::getType() {
    return m_island_type;
}

void IslandBase::transform(QQuaternion rotation, Point shift) {
    // rotate and then shift every region in this island
    for (QSharedPointer<RegionBase> region : m_regions) { region->transform(rotation, shift); }
}

float IslandBase::getMinZ() {
    // find the min of the regions in this island
    float island_min = std::numeric_limits<float>::max();
    for (QSharedPointer<RegionBase> region : m_regions) {
        float region_min = region->getMinZ();
        if (region_min < island_min) island_min = region_min;
    }
    return island_min;
}

bool IslandBase::getAnyValidPaths() {
    bool ret = false;

    for (auto r : m_regions) {
        if (r->getPaths().size() > 0) {
            ret = true;
            break;
        }
    }

    return ret;
}

QVector<SettingsPolygon> IslandBase::getSettingsPolygons() {
    return m_settings_polygons;
}

void IslandBase::prepareRegionForOptimization(const QSharedPointer<RegionBase>& region, int layerNumber,
                                              QVector<QSharedPointer<RegionBase>>& previousRegions) {
    std::optional<Point> previous_start;

    for (int i = previousRegions.size() - 1; i >= 0; --i) {
        const QSharedPointer<RegionBase>& previous_region = previousRegions[i];
        if (previous_region->getRegionType() != region->getRegionType()) continue;

        if (previous_region->getOptimizedLayerNumber() >= layerNumber) continue;

        previous_start = previous_region->getFirstPrintingStartPoint();
        if (previous_start.has_value()) break;
    }

    region->setOptimizedLayerNumber(layerNumber);
    region->setPreviousLayerStartPoint(previous_start);
}

void IslandBase::calculateMultiMaterialTransitions(QVector<QSharedPointer<RegionBase>>& previousRegions) {
    if (previousRegions.size() > 1) {
        QSharedPointer<RegionBase> lastRegion       = previousRegions.last();
        QSharedPointer<RegionBase> preceedingRegion = previousRegions[previousRegions.size() - 2];
        if (lastRegion->getMaterialNumber() != preceedingRegion->getMaterialNumber()) {
            Distance transition_distance;
            if (m_sb->setting<int>(MS::MultiMaterial::kEnableSecondDistance) && lastRegion->getMaterialNumber() == 2) {
                transition_distance = m_sb->setting<Distance>(MS::MultiMaterial::kSecondDistance);
            }
            else { transition_distance = m_sb->setting<Distance>(MS::MultiMaterial::kTransitionDistance); }

            int i = previousRegions.size() - 2;
            while (i >= 0 && transition_distance > 0) {
                previousRegions[i]->calculateMultiMaterialTransition(transition_distance,
                                                                     lastRegion->getMaterialNumber());
                --i;
            }
        }
    }
}

void IslandBase::fitCircularArcs(const QSharedPointer<SettingsBase>& global_sb) {
    for (QSharedPointer<RegionBase> region : m_regions) region->fitCircularArcs(global_sb);
}

}  // namespace ORNL
