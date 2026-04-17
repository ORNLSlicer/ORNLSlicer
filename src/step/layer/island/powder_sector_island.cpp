#include "step/layer/island/powder_sector_island.h"

#include <algorithm>

#include <qcontainerfwd.h>
#include <qlist.h>
#include <qsharedpointer.h>

#include "geometry/point.h"
#include "geometry/settings_polygon.h"
#include "managers/settings/settings_manager.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/infill_sector.h"
#include "step/layer/regions/perimeter_sector.h"
#include "step/layer/regions/region_base.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
PowderSectorIsland::PowderSectorIsland(SectorInformation si, const QSharedPointer<SettingsBase>& sb,
                                       const QVector<SettingsPolygon>& settings_polygons)
    : IslandBase(si.infill, sb, settings_polygons) {
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
    if (sb->setting<bool>(PS::Perimeter::kEnable)) {
        QSharedPointer<PerimeterSector> perimeter =
            QSharedPointer<PerimeterSector>::create(sb, regionOrder.indexOf(RegionType::kPerimeter), settings_polygons);
        perimeter->setComputedGeometry(si.perimeters);
        perimeter->setStartVector(si.start_vector);
        this->addRegion(perimeter);
    }

    if (sb->setting<bool>(PS::Infill::kEnable)) {
        QSharedPointer<InfillSector> infill =
            QSharedPointer<InfillSector>::create(sb, regionOrder.indexOf(RegionType::kInfill), settings_polygons);
        infill->setSectorAngle(si.start_angle);
        infill->setStartVector(si.start_vector);
        this->addRegion(infill);
    }

    m_island_type = IslandType::kPowderSector;
}

void PowderSectorIsland::optimize(int layerNumber, Point& currentLocation,
                                  QVector<QSharedPointer<RegionBase>>& previousRegions) {
    // NOP
}

void PowderSectorIsland::reorderRegions() {
    std::sort(m_regions.begin(), m_regions.end(),
              [](auto const& a, auto const& b) { return a->getIndex() < b->getIndex(); });
}
} // namespace ORNL
