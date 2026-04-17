#pragma once

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qobject.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/polygon_list.h"
#include "geometry/polyline.h"
#include "optimizers/layer_order_optimizer.h"
#include "slicing/preprocessor.h"
#include "step/layer/island/powder_sector_island.h"
#include "threading/real_time_ast.h"

namespace ORNL {

//!
//! \class RealTimeRPBFSlicer
//! \brief A slicer that computes single layers at a time, waiting for feedback from sensors
class RealTimeRPBFSlicer : public RealTimeAST {
  public:
    //! \brief Constructor
    //! \param gcodeLocation the location to write gcode to
    RealTimeRPBFSlicer(QString gcodeLocation);

  protected:
    //! \brief called once before any layers are processed
    void initialSetup() override;

    //! \brief Populates parts with up to a single layer, depending on order and global layers
    //! \param opt_data optional sensor data
    void preProcess(nlohmann::json opt_data = nlohmann::json()) override;

    //! \brief Connects paths
    //! \param opt_data optional sensor data
    void postProcess(nlohmann::json opt_data = nlohmann::json()) override;

    //! \brief Writes out gcode
    void writeGCode() override;

    //! \brief skips processing up to a layer number
    //! \param layer_num layer number to skip to
    void skip(int layer_num) override;

  private:
    //! \brief Splits geometry into sectors
    //! \param perimeters: global vector of all perimeters represented as polylines
    //! \param layer_specific_settings: settings base
    //! \param infill_geometry: polygonal representation of remaining area for infill
    //! \param sectors: output vector with necessary vector information - perimeters, starting vector, sector angle
    //! rotation, and infill area
    void splitIntoSectors(QVector<Polyline> perimeters, QSharedPointer<SettingsBase> layer_specific_settings,
                          PolygonList infill_geometry, QVector<QVector<SectorInformation>>& sectors, int layer_count);

    //! \brief Layer optimizer for "global" layers
    QSharedPointer<LayerOrderOptimizer> m_layer_optimizer;

    //! \brief The Preprocessor used by this slicer
    QSharedPointer<Preprocessor> m_preprocessor;
};
} // namespace ORNL
