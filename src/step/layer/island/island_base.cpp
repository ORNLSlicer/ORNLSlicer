#include "step/layer/island/island_base.h"

#include "step/layer/regions/anchor.h"
#include "step/layer/regions/brim.h"
#include "step/layer/regions/infill.h"
#include "step/layer/regions/inset.h"
#include "step/layer/regions/laser_scan.h"
#include "step/layer/regions/perimeter.h"
#include "step/layer/regions/raft.h"
#include "step/layer/regions/skeleton.h"
#include "step/layer/regions/skin.h"
#include "step/layer/regions/skirt.h"
#include "step/layer/regions/support.h"
#include "step/layer/regions/thermal_scan.h"

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
        if (r->getPaths().size() > 0)
            ret += r->writeGCode(writer);
    }

    return ret;
}

void IslandBase::addRegion(QSharedPointer<RegionBase> region) { m_regions.push_back(region); }

const QList<QSharedPointer<RegionBase>> IslandBase::getRegions() const { return m_regions; }

QSharedPointer<RegionBase> IslandBase::getRegion(RegionType type) {
    switch (type) {
        case RegionType::kPerimeter:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Perimeter> perimeter = r.dynamicCast<Perimeter>();
                if (perimeter.isNull())
                    continue;

                return std::move(perimeter);
            }
            break;
        case RegionType::kInset:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Inset> inset = r.dynamicCast<Inset>();
                if (inset.isNull())
                    continue;

                return std::move(inset);
            }
            break;
        case RegionType::kSkin:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Skin> skin = r.dynamicCast<Skin>();
                if (skin.isNull())
                    continue;

                return std::move(skin);
            }
            break;
        case RegionType::kInfill:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Infill> infill = r.dynamicCast<Infill>();
                if (infill.isNull())
                    continue;

                return std::move(infill);
            }
            break;
        case RegionType::kSkeleton:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Skeleton> skeleton = r.dynamicCast<Skeleton>();
                if (skeleton.isNull())
                    continue;

                return std::move(skeleton);
            }
            break;
        case RegionType::kBrim:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Brim> brim = r.dynamicCast<Brim>();
                if (brim.isNull())
                    continue;

                return std::move(brim);
            }
            break;
        case RegionType::kSkirt:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Skirt> skirt = r.dynamicCast<Skirt>();
                if (skirt.isNull())
                    continue;

                return std::move(skirt);
            }
            break;
        case RegionType::kRaft:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Raft> raft = r.dynamicCast<Raft>();
                if (raft.isNull())
                    continue;

                return std::move(raft);
            }
            break;
        case RegionType::kSupport:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Support> support = r.dynamicCast<Support>();
                if (support.isNull())
                    continue;

                return std::move(support);
            }
            break;
        case RegionType::kLaserScan:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<LaserScan> laserscan = r.dynamicCast<LaserScan>();
                if (laserscan.isNull())
                    continue;

                return std::move(laserscan);
            }
            break;
        case RegionType::kThermalScan:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<ThermalScan> thermalscan = r.dynamicCast<ThermalScan>();
                if (thermalscan.isNull())
                    continue;

                return std::move(thermalscan);
            }
            break;
        case RegionType::kAnchor:
            for (QSharedPointer<RegionBase> r : m_regions) {
                QSharedPointer<Anchor> anchor = r.dynamicCast<Anchor>();
                if (anchor.isNull())
                    continue;

                return std::move(anchor);
            }
            break;
        case RegionType::kSupportRoof:
        case RegionType::kUnknown:
        default:
            return nullptr;
    }

    return nullptr;
}

const PolygonList& IslandBase::getGeometry() const { return m_geometry; }

void IslandBase::compute(uint layer_num, QSharedPointer<SyncManager>& sync) {
    PolygonList pl = m_geometry;

    for (QSharedPointer<RegionBase> r : m_regions) {
        r->setGeometry(pl);
        r->compute(layer_num, sync);
        pl = r->getGeometry();
    }

#ifdef HAVE_SINGLE_PATH
    if (m_sb->setting<bool>(ES::SinglePath::kEnableSinglePath)) {
        // Combine and pass to sp
        QVector<SinglePath::PolygonList> combined_geometry;

        QSharedPointer<Perimeter> perimeters = nullptr;
        QSharedPointer<Inset> insets = nullptr;

        // Find regions if they exist
        for (int index = 0, end = m_regions.size(); index < end; ++index) {
            if (perimeters == nullptr)
                perimeters = m_regions[index].dynamicCast<Perimeter>();

            if (insets == nullptr)
                insets = m_regions[index].dynamicCast<Inset>();
        }

        // If perimeters are found, add them
        if (perimeters != nullptr) {

            for (auto& poly_list : perimeters->getComputedGeometry()) {
                SinglePath::PolygonList sp_poly_list;
                for (auto& polygon : poly_list) {
                    SinglePath::Polygon sp_polygon = polygon;
                    sp_polygon.setRegionType(SinglePath::RegionType::kPerimeter);
                    sp_poly_list.append(sp_polygon);
                }
                combined_geometry.append(sp_poly_list);
            }
        }

        // If insets are found, add them
        if (insets != nullptr) {
            for (auto& poly_list : insets->getComputedGeometry()) {
                SinglePath::PolygonList sp_poly_list;
                for (auto& polygon : poly_list) {
                    SinglePath::Polygon sp_polygon = polygon;
                    sp_polygon.setRegionType(SinglePath::RegionType::kInset);
                    sp_poly_list.append(sp_polygon);
                }
                combined_geometry.append(sp_poly_list);
            }
        }

        // Only apply if there is geometry
        if (combined_geometry.size() > 0)
            applySinglePath(combined_geometry, layer_num, sync);

        // Only need to path in one region. Prefer building in perimeters, but use insets as fallback
        if (perimeters != nullptr) {
            perimeters->setSinglePathGeometry(combined_geometry);
            perimeters->createSinglePaths();
        }
        else if (insets != nullptr) {
            insets->setSinglePathGeometry(combined_geometry);
            insets->createSinglePaths();
        } // Else no paths to build
    }
#endif
}

#ifdef HAVE_SINGLE_PATH
void IslandBase::applySinglePath(QVector<SinglePath::PolygonList>& single_path_geometry, uint layer_num,
                                 QSharedPointer<SyncManager>& sync) {
    // Get settings
    bool enable_exclusion = m_sb->setting<bool>(ES::SinglePath::kEnableBridgeExclusion);
    bool enable_zippering = m_sb->setting<bool>(ES::SinglePath::kEnableZippering);
    Distance previous_layer_exclusion_distance = m_sb->setting<Distance>(ES::SinglePath::kPrevLayerExclusionDistance);
    Distance corner_exclusion_distance = m_sb->setting<Distance>(ES::SinglePath::kCornerExclusionDistance);
    Distance max_bridge_length = m_sb->setting<Distance>(ES::SinglePath::kMaxBridgeLength);
    Distance min_bridge_separation = m_sb->setting<Distance>(ES::SinglePath::kMinBridgeSeparation);
    Distance inset_bead_width = m_sb->setting<Distance>(PS::Inset::kBeadWidth);
    Distance perimeter_bead_width = m_sb->setting<Distance>(PS::Perimeter::kBeadWidth);

    SinglePath::Settings settings(enable_exclusion, enable_zippering, previous_layer_exclusion_distance(),
                                  corner_exclusion_distance(), max_bridge_length(), min_bridge_separation());

    // Apply single path
    SinglePath::SinglePath sp(single_path_geometry, perimeter_bead_width(), inset_bead_width(), settings);

    QList<SinglePath::Bridge> exclusion_bridges;
    QList<SinglePath::Bridge> zipper_bridges;

    if (enable_exclusion)
        exclusion_bridges = sync->wait<QList<SinglePath::Bridge>>(layer_num, LinkType::kPreviousLayerExclusionInset);

    if (enable_zippering)
        zipper_bridges = sync->wait<QList<SinglePath::Bridge>>(layer_num, LinkType::kZipperingInset);

    QList<SinglePath::Bridge> picked_bridges = sp.buildGraph(exclusion_bridges, zipper_bridges);

    if (enable_exclusion)
        sync->wake<QList<SinglePath::Bridge>>(layer_num, LinkType::kPreviousLayerExclusionInset, picked_bridges);

    if (enable_zippering)
        sync->wake<QList<SinglePath::Bridge>>(layer_num, LinkType::kZipperingInset, picked_bridges);

    single_path_geometry = sp.convertToSinglePath();
}
#endif

QSharedPointer<SettingsBase> IslandBase::getSb() const { return m_sb; }

void IslandBase::setSb(const QSharedPointer<SettingsBase>& sb) {
    m_sb = sb;

    // For each region in the stages, populate their sb's with the island's.
    for (auto r : m_regions) {
        r->setSb(m_sb);
    }
}

IslandType IslandBase::getType() { return m_island_type; }

void IslandBase::transform(QQuaternion rotation, Point shift) {
    // rotate and then shift every region in this island
    for (QSharedPointer<RegionBase> region : m_regions) {
        region->transform(rotation, shift);
    }
}

float IslandBase::getMinZ() {
    // find the min of the regions in this island
    float island_min = std::numeric_limits<float>::max();
    for (QSharedPointer<RegionBase> region : m_regions) {
        float region_min = region->getMinZ();
        if (region_min < island_min)
            island_min = region_min;
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

QVector<SettingsPolygon> IslandBase::getSettingsPolygons() { return m_settings_polygons; }

void IslandBase::calculateMultiMaterialTransitions(QVector<QSharedPointer<RegionBase>>& previousRegions) {
    if (previousRegions.size() > 1) {
        QSharedPointer<RegionBase> lastRegion = previousRegions.last();
        QSharedPointer<RegionBase> preceedingRegion = previousRegions[previousRegions.size() - 2];
        if (lastRegion->getMaterialNumber() != preceedingRegion->getMaterialNumber()) {
            Distance transition_distance;
            if (m_sb->setting<int>(MS::MultiMaterial::kEnableSecondDistance) && lastRegion->getMaterialNumber() == 2) {
                transition_distance = m_sb->setting<Distance>(MS::MultiMaterial::kSecondDistance);
            }
            else {
                transition_distance = m_sb->setting<Distance>(MS::MultiMaterial::kTransitionDistance);
            }

            int i = previousRegions.size() - 2;
            while (i >= 0 && transition_distance > 0) {
                previousRegions[i]->calculateMultiMaterialTransition(transition_distance,
                                                                     lastRegion->getMaterialNumber());
                --i;
            }
        }
    }
}

void IslandBase::adjustMultiNozzle() {
    for (auto region : getRegions()) {
        region->adjustMultiNozzle();
    }
}

void IslandBase::addNozzle(int extruder) {
    for (QSharedPointer<ORNL::RegionBase> region : getRegions()) {
        region->addNozzle(extruder);
    }
}

void IslandBase::setExtruder(int ext) { m_extruder = ext; }

int IslandBase::getExtruder() { return m_extruder; }
} // namespace ORNL
