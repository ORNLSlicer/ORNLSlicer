#pragma once

#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmap.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <quuid.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "part/part.h"
#include "step/layer/island/island_base.h"
#include "step/layer/regions/region_base.h"
#include "step/layer/scan_layer.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class GlobalLayer
 * \brief A meta structure to contain steps from multiple parts that should be printed
 *        as one layer
 */
class GlobalLayer {
  public:
    //! \brief Constructor
    //! \param layer_number - the # in sequence the global layer should be printed
    GlobalLayer(int layer_number);

    //! \brief compensates for rotation and shift during cross-sectioning
    //! \note  only affects clean objects; should be called before connectPaths
    void unorient();

    //! \brief compensates for rotation and shift during cross-sectioning
    //! \note should be called after connectPaths
    void reorient();

    //! \brief Creates path modifiers; must be called after connectPaths()
    void calculateModifiers(QSharedPointer<SettingsBase> global_sb, QVector<Point>& current_location, int layer_num);

    //! \brief Adjusts pathing on a layer to account for multiple fixed nozzles
    void adjustFixedMultiNozzle();

    //! \brief orders islands & paths, generates travels
    //! \param global_sb - a reference to the global settings base
    //! \param start - list of the start points of pathing for each extruder
    //! \param start_index - list of island index to start with for each extruder
    //! \param previous_regions - list of previousRegions visited by each extruder
    //! \note index in vector corresponds to extruder number
    void connectPaths(QSharedPointer<SettingsBase> global_sb, QVector<Point>& start, QVector<int>& start_index,
                      QVector<QVector<QSharedPointer<RegionBase>>>& previousRegions);

    //! \brief generates gcode
    //! \param the writer/syntax to use
    QString writeGCode(QSharedPointer<WriterBase> writer);

    //! \brief sets the dirty bit of all steps contained in the global layer
    //! \param status of dirty flag
    void setDirtyBit(bool dirty);

    //! \brief adds a step pair to this layer
    //! \param part_id - the QUuid for the part that the step_pair is from
    //! \param step_pair - the step pair to add to the layer
    void addStepPair(QUuid part_id, Part::StepPair step_pair);

    //! \brief returns a list of all the islands, from all parts and step groups
    QVector<QSharedPointer<IslandBase>> getIslands();

    //! \brief returns the minimum z-coordinate found within the layer
    //! \note used primarily for determining table movement
    double getMinZ();

    //! \brief returns the layer height of the global layer (assumes all parts have same layer height)
    //! \note used for writeBeforeLayer() function
    Distance getLayerHeight();

  private:
    //! \brief Creates tree-like structure if brims exist, otherwise, sorts islands into precendence order;
    //!        helper function for connectPaths()
    QList<QMap<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>>
    createSequence(QList<QSharedPointer<IslandBase>> parent, QList<QList<QSharedPointer<IslandBase>>> children);

    //! \brief returns true if this global layer contains scan layers
    bool containsScanLayers();

    //! \brief collection of step groups contained in the global layer
    //! \note maps unique part id to step group to track which part the step group is from
    QMap<QUuid, QSharedPointer<Part::StepPair>> m_step_pairs;

    //! \brief Precendence list based on geometry and settings to define order for traveling and gcode.
    //! \note used by connectPaths()
    QVector<QList<QSharedPointer<IslandBase>>> m_island_order;

    //! \brief maintains the order of this layer in the list of all global layers
    int m_layer_number;

    //! \brief saves the order for scan_layers
    //! \note order determined in connectPaths and saved for use again in writeGcode
    QList<QUuid> m_part_order;
};
} // namespace ORNL
