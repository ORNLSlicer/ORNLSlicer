#pragma once

#include <QLinkedList>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "step/layer/regions/region_base.h"
#include "step/step.h"

namespace ORNL {
class IslandBase;

class Layer : public Step {
  public:
    //! \brief Constructor
    Layer(uint layer_nr, const QSharedPointer<SettingsBase>& sb);

    //! \brief Writes the code for the layer.
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Get the layer number this layer contains.
    uint getLayerNumber() const;

    //! \brief Computes the layer.
    void compute() override;

    //! \brief Sets the settings base and populates it to the islands.
    //! \note This function should only be called after the layer has been populated (all islands are constructed).
    void setSb(const QSharedPointer<SettingsBase>& sb) override;

    //! \brief Add travels according to optimization strategy
    //! \param start: Point at which to begin adding travels
    //! \param start_index: Selected first island from previous layer (used in some optimization strategies)
    //! \param previousRegions: chain of previously visited regions to allow decisions on inter-island and inter-layer
    //! transitions
    void connectPaths(Point& start, int& start_index, QVector<QSharedPointer<RegionBase>>& previousRegions);

    //! \brief Adjusts pathing to use multiple nozzles
    void adjustMultiNozzle();

    //! \brief Creates modifiers
    //! \param currentLocation: current location used to update start points of travels after modifiers are added
    void calculateModifiers(Point& currentLocation) override;

    //! \brief Check if any settings polygons have changed from the last slice
    //! \param sb SettingsBase to check against
    void flagIfDirtySettingsPolygons(const QVector<SettingsPolygon>& settings_poly);

    //! \brief gets the last location in this layer
    //! \return the last location of (0, 0, 0) if there are no paths
    Point getEndLocation() override;

    //! \brief remove rotation and shift compensation during cross sectioning using
    //!        m_plane_normal and m_shift amount; should only be called once
    //!        when dealing with clean objects
    void unorient();

    //! \brief compensates for rotation and shift during cross sectioning using
    //!        m_plane_normal and m_shift amount; should only be called once
    void reorient();

    //! \brief shifts this layers in three axis to compensate for raft layers that where added
    void compensateForRafts();

    //! \brief returns the minimum z of a layer
    float getMinZ() override;

    //! \brief Returns the final location of the layer
    Point getFinalLayerLocation();

    //! \brief sets the list of polygons that contain settings
    //! \param settings_polygons: a list of polygon overrides for settings
    void setSettingsPolygons(QVector<SettingsPolygon>& settings_polygons);

    //! \brief gets the settings polygons for this layer
    //! \return a list of settings polygons
    QVector<SettingsPolygon> getSettingsPolygons();

  protected:
    //! \brief Layer number.
    uint m_layer_nr;

    //! \brief Precendence list based on geometry and settings to define order for traveling and gcode.
    QList<QSharedPointer<IslandBase>> m_island_order;

  private:
    //! \brief Creates tree-like structure if brims exist, otherwise, sorts islands into precendence order
    QList<QHash<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>>
    createSequence(QList<QSharedPointer<IslandBase>> parent, QList<QList<QSharedPointer<IslandBase>>> children);

    //! \brief removes duplicate islands according to remove-duplicate-path settings
    void removeDuplicateIslands();

    //! \brief a collection of polygons on this layer that contain setting overrides
    QVector<SettingsPolygon> m_settings_polygons;
};
} // namespace ORNL
