#pragma once

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"

namespace ORNL {

//! \brief Convenience struct to hold layer information. Each layer is divided into sectors.
//! \param perimeters: perimeters represented as polylines. Calculated in RPBF_slicer as perimeters only need to be
//! calculated once. \param start_vector: vector representing "starting line" for each sector. Used to order perimeters
//! in each island based on circular rotation. \param infill: geometric representation of area for infill generation.
//! \param start_angle: angle used to achieve sector rotation. Used to orient infill.
struct SectorInformation {
    QVector<Polyline> perimeters;
    Point start_vector;
    Angle start_angle;
    PolygonList infill;
};

/*!
 * \class PowderSectorIsland
 * \brief Island that RPBF builds use. Metal powder system with pathing divided into rotating sectors.
 */
class PowderSectorIsland : public IslandBase {
  public:
    //! \brief Constructor
    //! \param si: necessary geometric information for each region type
    //! \param sb: the settings
    //! \param settings_polygons: a vector of settings polygons to apply
    PowderSectorIsland(SectorInformation si, const QSharedPointer<SettingsBase>& sb,
                       const QVector<SettingsPolygon>& settings_polygons);

    //! \brief Override from base. Filters down to individual regions to add
    //! travels and apply path modifiers
    void optimize(int layerNumber, Point& currentLocation,
                  QVector<QSharedPointer<RegionBase>>& previousRegions) override;

    //! \brief Reorder regions based on previously identified order
    void reorderRegions();
};
} // namespace ORNL
