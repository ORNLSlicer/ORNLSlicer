#include "step/layer/island/anchor_island.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "managers/settings/settings_manager.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/anchor.h"
#include "step/layer/regions/region_base.h"
#include "utilities/enums.h"

namespace ORNL {
AnchorIsland::AnchorIsland(const PolygonList& geometry, const QSharedPointer<SettingsBase>& sb,
                           const QVector<SettingsPolygon>& settings_polygons)
    : IslandBase(geometry, sb, settings_polygons) {
    this->addRegion(QSharedPointer<Anchor>::create(sb, settings_polygons));
    m_island_type = IslandType::kPolymer;
}

void AnchorIsland::optimize(int layerNumber, Point& currentLocation,
                            QVector<QSharedPointer<RegionBase>>& previousRegions) {
    bool unused = true;
    for (QSharedPointer<RegionBase> r : m_regions) {
        QVector<Path> tmp_path;
        r->optimize(layerNumber, currentLocation, tmp_path, tmp_path, unused);
    }
}
} // namespace ORNL
