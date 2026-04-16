#pragma once

#include <QDir>
#include <QObject>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/plane.h"
#include "geometry/polygon_list.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/island/island_base.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class Step
 * \brief Abstract implementation of a step in the slicer process.
 */
class Step {
  public:
    //! \brief Constructor
    Step(const QSharedPointer<SettingsBase>& sb = QSharedPointer<SettingsBase>::create());
    //! \brief Destructor
    virtual ~Step() = default;

    //! \brief Pure virtual of GCode writer function.
    //! \param writer: Gcode syntax that pathing will be output as
    virtual QString writeGCode(QSharedPointer<WriterBase> writer) = 0;

    //! \brief Get the SettingsBase.
    //! \return pointer to SettingsBase
    QSharedPointer<SettingsBase> getSb() const;

    //! \brief Set the SettingsBase.
    //! \note This function is virtual so that a child class can push the sb to its members.
    //! \param sb: Pointer to SettingsBase to set
    virtual void setSb(const QSharedPointer<SettingsBase>& sb);

    //! \brief Function that does the computation for the step.
    virtual void compute() = 0;

    //! \brief calculates modifiers as part of the post-processing step
    //! \param currentLocation: current location passed to each modifier to be updated
    //! at each step once a path has been connected via travels
    virtual void calculateModifiers(Point& currentLocation) = 0;

    //! \brief returns the minimun z-value of a step
    //! \return value of minimum Z
    virtual float getMinZ() = 0;

    //! \brief gets the last location in this layer
    //! \return the last location of (0, 0, 0) if there are no paths
    virtual Point getEndLocation() = 0;

    //! \brief sets bool for dirty status.  Used for caching to determine
    //! which steps must be recalculated upon re-slice
    //! \param dirty: whether or not step is dirty
    void setDirtyBit(bool dirty);

    //! \brief returns whether or not step is dirty
    //! \return bool that represents dirty status
    bool isDirty();

    //! \brief gets the step sync manager
    //! \return the manager for step synchronization
    QSharedPointer<SyncManager> getSync() const;

    //! \brief sets the step sync manager
    //! \param the new step synchronizer
    void setSync(const QSharedPointer<SyncManager>& sync);

    //! \brief Get step type
    //! \return step type
    StepType getType();

    //! \brief sets the type of this step
    //! \param type the ne type
    void setType(StepType type);

    //! \brief Get geometry for which pathing will be laid out
    //! \return Polygonlist comprising geometry
    const PolygonList& getGeometry() const;

    //! \brief Set geometry and average normal
    //! \param geometry: geometry to set
    //! \param averageNormal: average normal for layer
    void setGeometry(const PolygonList& geometry, const QVector3D& averageNormal);

    //! \brief Check if any settings have changed from the last slice
    //! \param sb SettingsBase to check against
    void flagIfDirtySettings(const QSharedPointer<SettingsBase>& sb);

    //! \brief Set orientation information for non-horizontal slicing
    //! \param normal: Normal of slicing plane
    //! \param shift: Amount to translate by
    void setOrientation(Plane slicing_plane, Point shift);

    //! \brief Add island for evaluation
    //! \param type: island type
    //! \param island: pointer to island
    void addIsland(IslandType type, QSharedPointer<IslandBase> island);

    //! \brief Update the layer's islands.
    //! \param type: island type
    //! \param islands: pointer to islands
    void updateIslands(IslandType type, QVector<QSharedPointer<IslandBase>> islands);

    //! \brief Get plane normal
    //! \return Vector representing normal
    QVector3D getNormal();

    //! \brief Get plane shift
    //! \return Point representing shift
    Point getShift();

    //! \brief Get slicing plane
    //! \return Plane
    Plane getSlicingPlane();

    //! \brief Set path for companion files for gcode output (i.e. scans)
    //! \param output_file: path for files
    void setCompanionFileLocation(QDir path);

    //! \brief Get islands
    //! \return list of pointers to islands of specific type
    QList<QSharedPointer<IslandBase>> getIslands(IslandType type = IslandType::kAll);

    //! \brief sets the amount this part should be shifted to account for raft layers
    //! \param distance to shift
    void setRaftShift(QVector3D shift);

    //! \brief gets the amount this part should be shifted to account for raft layers
    //! \return distance to shift
    QVector3D getRaftShift();

  protected:
    //! \brief Settings for the step.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief Manages syncing of this step with others
    QSharedPointer<SyncManager> m_sync;

    //! \brief Step type
    StepType m_type;

    //! \brief plane used to generate this layer
    Plane m_slicing_plane;

    //! \brief amount layer shifted before clipping math, used to undo shift after math
    Point m_shift_amount;

    //! \brief amount this part should be shifted to account for raft layers
    QVector3D m_raft_shift;

    //! \brief Islands to evaluate
    QMultiHash<int, QSharedPointer<IslandBase>> m_islands;

    //! \brief Path for companion file gcode output
    QDir m_path;

    //! \brief The geometry on the layer.
    PolygonList m_geometry;

  private:
    //! \brief bool that holds dirty status
    bool m_dirty_bit;
};
} // namespace ORNL
