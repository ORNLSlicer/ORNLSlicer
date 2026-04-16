#include "step/layer/island/support_island.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/region_base.h"
#include "step/layer/regions/support.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
SupportIsland::SupportIsland(const PolygonList& geometry, const QSharedPointer<SettingsBase>& sb,
                             const QVector<SettingsPolygon>& settings_polygons)
    : IslandBase(geometry, sb, settings_polygons) {
    // Stage 0
    this->addRegion(QSharedPointer<Support>::create(sb, settings_polygons));
    m_island_type = IslandType::kSupport;
}

void SupportIsland::optimize(int layerNumber, Point& currentLocation,
                             QVector<QSharedPointer<RegionBase>>& previousRegions) {
    bool unused = true;
    for (QSharedPointer<RegionBase> r : m_regions) {
        QVector<Path> tmp_path;
        r->optimize(layerNumber, currentLocation, tmp_path, tmp_path, unused);

        if (r->getPaths().size() > 0)
            previousRegions.push_back(r);

        if (m_sb->setting<bool>(MS::MultiMaterial::kEnable) &&
            m_sb->setting<Distance>(MS::MultiMaterial::kTransitionDistance) > 0 &&
            !m_sb->setting<bool>(ES::MultiNozzle::kEnableMultiNozzleMultiMaterial))
            calculateMultiMaterialTransitions(previousRegions);
    }
}
} // namespace ORNL
