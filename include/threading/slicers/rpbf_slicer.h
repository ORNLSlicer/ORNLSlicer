#pragma once

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "step/layer/island/powder_sector_island.h"
#include "threading/traditional_ast.h"

namespace ORNL {

/*!
 * \brief The RPBFSlicer supports unique build constraints: multi-nozzle metal powder
 * with circular, rotating build volume
 */
class RPBFSlicer : public TraditionalAST {
  public:
    //! \brief Constructor
    RPBFSlicer(QString gcodeLocation);

  protected:
    //! \brief Creates layers and regions
    //! \param opt_data optional sensor data
    void preProcess(nlohmann::json opt_data = nlohmann::json()) override;

    //! \brief Connects paths
    //! \param opt_data optional sensor data
    void postProcess(nlohmann::json opt_data = nlohmann::json()) override;

    //! \brief Writes layers/ regions to file
    void writeGCode() override;

  private:
    //! \brief Splits geometry into sectors
    //! \param perimeters: global vector of all perimeters represented as polylines
    //! \param layer_specific_settings: settings base
    //! \param infill_geometry: polygonal representation of remaining area for infill
    //! \param sectors: output vector with necessary vector information - perimeters, starting vector, sector angle
    //! rotation, and infill area
    void splitIntoSectors(QVector<Polyline> perimeters, QSharedPointer<SettingsBase> layer_specific_settings,
                          PolygonList infill_geometry, QVector<QVector<SectorInformation>>& sectors, int layer_count);
};
} // namespace ORNL
