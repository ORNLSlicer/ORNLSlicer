#include "step/layer/island/polymer_island.h"

#include "managers/settings/settings_manager.h"
#include "step/layer/regions/infill.h"
#include "step/layer/regions/inset.h"
#include "step/layer/regions/perimeter.h"
#include "step/layer/regions/skeleton.h"
#include "step/layer/regions/skin.h"

namespace ORNL {
PolymerIsland::PolymerIsland(const PolygonList& geometry, const QSharedPointer<SettingsBase>& sb,
                             const QVector<SettingsPolygon>& settings_polygons, const PolygonList& uncut_geometry)
    : IslandBase(geometry, sb, settings_polygons) {
    bool enable_perimeter = this->getSb()->setting<bool>(PS::Perimeter::kEnable);
    bool enable_inset = this->getSb()->setting<bool>(PS::Inset::kEnable);
    bool enable_skin = this->getSb()->setting<bool>(PS::Skin::kEnable);
    bool enable_infill = this->getSb()->setting<bool>(PS::Infill::kEnable);
    bool enable_skeleton = this->getSb()->setting<bool>(PS::Skeleton::kEnable);

    QList<QString> order = this->getSb()->setting<QList<QString>>(PS::Ordering::kRegionOrder);
    QList<RegionType> regionOrder;
    for (QString str : order)
        regionOrder.push_back(fromString(str.toUpper()));

    for (int i = regionOrder.size() - 1; i >= 0; --i) {
        if ((regionOrder[i] == RegionType::kPerimeter && !enable_perimeter) ||
            (regionOrder[i] == RegionType::kInset && !enable_inset) ||
            (regionOrder[i] == RegionType::kSkin && !enable_skin) ||
            (regionOrder[i] == RegionType::kInfill && !enable_infill) ||
            (regionOrder[i] == RegionType::kSkeleton && !enable_skeleton))
            regionOrder.removeAt(i);
    }

    if (enable_perimeter)
        this->addRegion(QSharedPointer<Perimeter>::create(sb, regionOrder.indexOf(RegionType::kPerimeter),
                                                          settings_polygons, uncut_geometry));
    if (enable_inset)
        this->addRegion(
            QSharedPointer<Inset>::create(sb, regionOrder.indexOf(RegionType::kInset), settings_polygons));
    if (enable_skin)
        this->addRegion(
            QSharedPointer<Skin>::create(sb, regionOrder.indexOf(RegionType::kSkin), settings_polygons));
    if (enable_infill)
        this->addRegion(
            QSharedPointer<Infill>::create(sb, regionOrder.indexOf(RegionType::kInfill), settings_polygons));
    if (enable_skeleton)
        this->addRegion(QSharedPointer<Skeleton>::create(sb, regionOrder.indexOf(RegionType::kSkeleton),
                                                         settings_polygons));

    m_island_type = IslandType::kPolymer;
}

void PolymerIsland::optimize(int layerNumber, Point& currentLocation,
                             QVector<QSharedPointer<RegionBase>>& previousRegions) {
    QSharedPointer<Inset> insets = getRegion(RegionType::kInset).dynamicCast<Inset>();
    QSharedPointer<Perimeter> perimeters = getRegion(RegionType::kPerimeter).dynamicCast<Perimeter>();

    QVector<Path> innerMostClosedContour;
    if (insets != nullptr)
        innerMostClosedContour = insets->getInnerMostPathSet();
    else if (perimeters != nullptr)
        innerMostClosedContour = perimeters->getInnerMostPathSet();

    QVector<Path> outerMostClosedContour;
    if (perimeters != nullptr)
        outerMostClosedContour = perimeters->getOuterMostPathSet();
    else if (insets != nullptr)
        outerMostClosedContour = insets->getOuterMostPathSet();

    bool shouldNextPathBeCCW = true;
    if (previousRegions.size() != 0 && previousRegions.last()->getPaths().size() != 0)
        shouldNextPathBeCCW = !previousRegions.last()->getPaths().last().getCCW();

    bool wasLastSpiral = false;

    for (QSharedPointer<RegionBase> r : m_regions) {
        if (previousRegions.size() > 0)
            wasLastSpiral = previousRegions.last()->getSb()->setting<bool>(PS::SpecialModes::kEnableSpiralize);

        r->setLastSpiral(wasLastSpiral);

        r->optimize(layerNumber, currentLocation, innerMostClosedContour, outerMostClosedContour, shouldNextPathBeCCW);

        if (r->getPaths().size() > 0)
            previousRegions.push_back(r);

        if (m_sb->setting<bool>(MS::MultiMaterial::kEnable) &&
            m_sb->setting<Distance>(MS::MultiMaterial::kTransitionDistance) > 0 &&
            !m_sb->setting<bool>(ES::MultiNozzle::kEnableMultiNozzleMultiMaterial)) {
            // multi-material without multi-nozzle requires material transitions
            calculateMultiMaterialTransitions(previousRegions);
        }
    }
}

void PolymerIsland::reorderRegions() {
    std::sort(m_regions.begin(), m_regions.end(),
              [](auto const& a, auto const& b) { return a->getIndex() < b->getIndex(); });
}
} // namespace ORNL
