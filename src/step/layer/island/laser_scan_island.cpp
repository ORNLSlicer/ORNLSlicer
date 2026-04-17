#include "step/layer/island/laser_scan_island.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/laser_scan.h"
#include "step/layer/regions/region_base.h"
#include "utilities/enums.h"

namespace ORNL {
LaserScanIsland::LaserScanIsland(const PolygonList& geometry, const QSharedPointer<SettingsBase>& sb,
                                 const QVector<SettingsPolygon>& settings_polygons)
    : IslandBase(geometry, sb, settings_polygons) {
    // Stage @
    this->addRegion(QSharedPointer<LaserScan>::create(sb, settings_polygons));
    m_island_type = IslandType::kLaserScan;
}

void LaserScanIsland::optimize(int layerNumber, Point& currentLocation,
                               QVector<QSharedPointer<RegionBase>>& previousRegions) {
    bool unused = true;
    for (QSharedPointer<RegionBase> r : m_regions) {
        QVector<Path> tmp_path;
        r->optimize(layerNumber, currentLocation, tmp_path, tmp_path, unused);
    }
}
} // namespace ORNL
