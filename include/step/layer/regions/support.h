#pragma once

#include <qcontainerfwd.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polyline.h"
#include "geometry/settings_polygon.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/regions/region_base.h"
#include "units/unit.h"

namespace ORNL {
class Support : public RegionBase {
  public:
    //! \brief Constructor
    //! \param sb: the settings
    //! \param settings_polygons: a vector of settings polygons to apply
    Support(const QSharedPointer<SettingsBase>& sb, const QVector<SettingsPolygon>& settings_polygons);

    //! \brief Writes the gcode for the support.
    //! \param writer Writer type to use for gcode output
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the support region.
    void compute(uint layer_num, QSharedPointer<SyncManager>& sync) override;

    //! \brief Optimizes the region.
    //! \param layerNumber: current layer number
    //! \param innerMostClosedContour: used for subsequent path modifiers
    //! \param outerMostClosedContour: used for subsequent path modifiers
    //! \param current_location: most recent location
    //! \param shouldNextPathBeCCW: state as to CW or CCW of previous path for use with additional DOF
    void optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                  QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) override;

    //! \brief Creates paths for the support region.
    //! \param line: polyline representing path
    //! \return Polyline converted to path
    Path createPath(Polyline line) override;

  private:
    //! \brief Creates modifiers
    //! \param path Current path to add modifiers to
    //! \param supportsG3 Whether or not G2/G3 is supported for spiral lift
    //! \param innerMostClosedContour used for Prestarts (currently only skins/infill)
    void calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) override;

    //! \brief Computes the support infill region
    void computeLine(Distance line_spacing, Angle rotation = 0 * deg);
    void computeGrid(Distance line_spacing, Angle rotation = 0 * deg);

    //! \brief Holds the computed geometry before it is converted into paths
    //! Vector of vector used to separate perimeter geometry from infill geometry
    QVector<Polyline> m_computed_perimeter_geometry;
    QVector<Polyline> m_computed_infill_geometry;
};
} // namespace ORNL
