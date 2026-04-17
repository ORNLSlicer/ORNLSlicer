#pragma once

#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/gcode_meta.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class AdamantineWriter
 * \brief Writing the scan path file for Adamantine
 */
class AdamantineWriter : public WriterBase {
    // WriterBase interface
  public:
    //! \brief Constructor
    AdamantineWriter(GcodeMeta meta, const QSharedPointer<SettingsBase>& sb);
    // Required functions
    QString writeInitialSetup(Distance minimum_x, Distance minimum_y, Distance maximum_x, Distance maximum_y,
                              int num_layers) override;
    QString writeBeforeLayer(float min_z, QSharedPointer<SettingsBase> sb) override;
    QString writeBeforePart(QVector3D normal) override;
    QString writeBeforeIsland() override;
    QString writeBeforeRegion(RegionType type, int pathSize) override;
    QString writeBeforePath(RegionType type) override;
    QString writeTravel(Point start_location, Point target_location, TravelLiftType lType,
                        QSharedPointer<SettingsBase> params) override;
    QString writeLine(const Point& start_point, const Point& target_point,
                      const QSharedPointer<SettingsBase> params) override;
    QString writeAfterPath(RegionType type) override;
    QString writeAfterRegion(RegionType type) override;
    QString writeAfterIsland() override;
    QString writeAfterPart() override;
    QString writeAfterLayer() override;
    QString writeShutdown() override;
    QString writeDwell(Time time) override;

  private:
    //! \brief State variables
    RegionType m_region_type;
    int counter;
};
} // namespace ORNL
