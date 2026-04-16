#pragma once

#include <QLinkedList>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/regions/region_base.h"
#include "utilities/enums.h"

#ifdef HAVE_SINGLE_PATH
    #include <single_path/single_path.h>
Q_DECLARE_METATYPE(QList<SinglePath::Bridge>);
#endif

namespace ORNL {
/*!
 * \class IslandBase
 * \brief Base class for islands.
 * \note For more information about the abstract slicing architecture, see the documentation.
 */
class IslandBase {
  public:
    //! \brief Constructor.
    //! \param geometry: the outlines
    //! \param sb: the settings
    //! \param settings_polygons: a vector of settings polygons to apply
    IslandBase(const PolygonList& geometry, const QSharedPointer<SettingsBase>& m_sb,
               const QVector<SettingsPolygon>& settings_polygons);

    //! \brief Destructor.
    virtual ~IslandBase() = default;

    //! \brief mark bridge for this island, the first segment of first region on island
    void markRegionStartSegment();

    //! \brief Returns a string with the GCode representing this island.
    QString writeGCode(QSharedPointer<WriterBase> writer);

    //! \brief Add a region to the list of regions.
    //! \note In the derived classes constructor, this function must be called multiple times to populate the regions.
    //!       Other functions expect the regions to be added in inmost to outmost order.
    void addRegion(QSharedPointer<RegionBase> region);

    //! \brief Get all regions.
    //! \note Returned regions are in order of inmost to outmost.
    const QList<QSharedPointer<RegionBase>> getRegions() const;

    //! \brief Get a specific region.
    //! \return The region (as a RegionBase) when found, nullptr when not found.
    QSharedPointer<RegionBase> getRegion(RegionType type);

    //! \brief Get the geometry that composes this island.
    const PolygonList& getGeometry() const;

    //! \brief Compute all regions in the island.
    void compute(uint layer_num, QSharedPointer<SyncManager>& sync);

#ifdef HAVE_SINGLE_PATH
    //! \brief applies the single path algorithm on a set of geometry
    //! \param geometry: input closed contours
    //! \param layer_num: the index of this layer
    //! \param sync: used to sync between layers
    void applySinglePath(QVector<SinglePath::PolygonList>& geometry, uint layer_num, QSharedPointer<SyncManager>& sync);
#endif

    //! \brief Get the settings for the island.
    QSharedPointer<SettingsBase> getSb() const;

    //! \brief Set the settings for the island.
    void setSb(const QSharedPointer<SettingsBase>& sb);

    //! \brief returns enum type of this island
    IslandType getType();

    //! \brief rotates and then shifts the island by given amounts
    void transform(QQuaternion rotation, Point shift);

    //! \brief returns the minimun z-value of an island
    float getMinZ();

    //! \brief checks to see whether any regions contain a valid path
    //! \return whether or not any paths are non-zero
    bool getAnyValidPaths();

    //! \brief Function called by parent step.  Filters down to individual regions to add
    //! travels and apply path modifiers
    //! \param layerNumber: current layer number
    //! \param current_location: Current point in space
    //! \param previousRegions: sequence of previously visited regions
    virtual void optimize(int layerNumber, Point& currentLocation,
                          QVector<QSharedPointer<RegionBase>>& previousRegions) = 0;

    //! \brief gets the list of settings polygons
    //! \return a list of settings polygons
    QVector<SettingsPolygon> getSettingsPolygons();

    //! \brief Calculate material transition between regions
    //! \param previousRegions: chain of regions visited to allow cross-island and cross-layer transitions
    void calculateMultiMaterialTransitions(QVector<QSharedPointer<RegionBase>>& previousRegions);

    //! \brief adjusts pathing for multiple extruders
    void adjustMultiNozzle();

    //! \brief add nozzle to list of nozzles that should be on when this island prints
    //! \param nozzle number, indexed at 0
    void addNozzle(int nozzle);

    //! \brief sets extruder/nozzle number for this island
    void setExtruder(int ext);

    //! \brief returns extruder/nozzle number for island
    int getExtruder();

  protected:
    //! \brief Geometry of island.
    PolygonList m_geometry;

    //! \brief Regions.
    QList<QSharedPointer<RegionBase>> m_regions;

    //! \brief Settings the island will use.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief Last enclosing contour for use with path modifiers
    Path innermostClosedContour;

    //! \brief The settings polygon this region may use
    QVector<SettingsPolygon> m_settings_polygons;

    //! \brief Enum value of island type
    IslandType m_island_type;

    //! \brief zero-indexed extruder # this island is assigned to
    int m_extruder;
};
} // namespace ORNL
