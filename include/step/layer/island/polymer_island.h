#pragma once

#include "step/layer/island/island_base.h"

namespace ORNL {
/*!
 * \class PolymerIsland
 * \brief Island that polymer builds use.
 */
class PolymerIsland : public IslandBase {
  public:
    //! \brief Constructor
    //! \param geometry: the outlines
    //! \param sb: the settings
    //! \param settings_polygons: a vector of settings polygons to apply
    PolymerIsland(const PolygonList& geometry, const QSharedPointer<SettingsBase>& sb,
                  const QVector<SettingsPolygon>& settings_polygons,
                  const PolygonList& uncut_geometry = PolygonList());

    //! \brief Override from base. Filters down to individual regions to add
    //! travels and apply path modifiers
    void optimize(int layerNumber, Point& currentLocation,
                  QVector<QSharedPointer<RegionBase>>& previousRegions) override;

    //! \brief Reorder regions based on previously identified order
    void reorderRegions();
};
} // namespace ORNL
