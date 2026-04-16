#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
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
class InfillSector : public RegionBase {
  public:
    //! \brief Constructor
    //! \param sb: the settings
    //!  //! \param index: index based on region ordering
    //! \param settings_polygons: a vector of settings polygons to apply
    InfillSector(const QSharedPointer<SettingsBase>& sb, const int index,
                 const QVector<SettingsPolygon>& settings_polygons);

    //! \brief Writes the gcode for the perimeter.
    //! \param writer Writer type to use for gcode output
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the perimeter region.
    void compute(uint layer_num, QSharedPointer<SyncManager>& sync) override;

    //! \brief Optimizes the region.
    //! \param layerNumber: current layer number
    //! \param innerMostClosedContour: used for subsequent path modifiers
    //! \param outerMostClosedContour: used for subsequent path modifiers
    //! \param current_location: most recent location
    //! \param shouldNextPathBeCCW: state as to CW or CCW of previous path for use with additional DOF
    void optimize(int layerNumber, Point& current_location, QVector<Path>& innerMostClosedContour,
                  QVector<Path>& outerMostClosedContour, bool& shouldNextPathBeCCW) override;

    //! \brief Creates paths for the infill sector region.
    //! \param line: polyline representing path
    //! \return Polyline converted to path
    Path createPath(Polyline line) override;

    //! \brief Set sector angle to manipulate geometry
    //! \param angle: angle that describes geometric rotation for sector
    void setSectorAngle(Angle angle);

    //! \brief Set starting vector
    //! \param p: point that represents "starting" vector for circular ordering
    void setStartVector(Point p);

  private:
    //! \brief Helper functions to optimize infill.  By default, infill alternates
    //! start/end points. Uniform will align all start/ends, skip will provide alternating
    //! start/end of every other line, stripped will provide alternating start/ends while
    //! splitting the region into multiple sub-sectors.
    void uniform(QVector<Polyline>& sector);

    //! \brief Creates modifiers
    //! \param path: Current path to add modifiers to
    //! \param supportsG3: Whether or not G2/G3 is supported for spiral lift
    //! \param innerMostClosedContour: used for Prestarts (currently only skins/infill)
    void calculateModifiers(Path& path, bool supportsG3, QVector<Path>& innerMostClosedContour) override;

    //! \brief Holds the computed geometry (perimeters) before it is converted into paths
    QVector<Polyline> m_computed_geometry;

    //! \brief Holds sector angle
    Angle m_sector_angle;

    //! \brief Hold starting vector for sector
    Point m_start_vec;
};
} // namespace ORNL
