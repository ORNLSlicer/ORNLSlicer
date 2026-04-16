#pragma once

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qobject.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/point.h"
#include "slicing/preprocessor.h"
#include "step/global_layer.h"
#include "step/layer/regions/region_base.h"
#include "threading/real_time_ast.h"
#include "units/unit.h"

namespace ORNL {

//!
//! \class RealTimePolymerSlicer
//! \brief A slicer that computes single layers at a time, waiting for feedback from sensors
class RealTimePolymerSlicer : public RealTimeAST {
  public:
    //! \brief Constructor
    //! \param gcodeLocation the location to write gcode to
    RealTimePolymerSlicer(QString gcodeLocation);

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
    //! \brief adds brim to part if needed
    //! \param part the part to process
    //! \param totalLayers numbers of layers in part
    //! \param sb the settings to use
    void processBrim(QSharedPointer<Part> part, int totalLayers, QSharedPointer<SettingsBase> sb);

    //! \brief adds skirt to part if needed
    //! \param part the part to process
    //! \param totalLayers numbers of layers in part
    //! \param sb the settings to use
    void processSkirt(QSharedPointer<Part> part, int totalLayers, QSharedPointer<SettingsBase> sb);

    //! \brief adds laser scans to part if needed
    //! \param part the part to process
    //! \param totalLayers numbers of layers in part
    //! \param sb the settings to use
    //! \param total_height the current height of the sliced section of the part
    void processLaserScan(QSharedPointer<Part> part, int totalLayers, QSharedPointer<SettingsBase> sb,
                          Distance total_height);

    //! \brief adds thermal scans to part if needed
    //! \param part the part to process
    //! \param sb the settings to use
    void processThermalScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> sb);

    //! \brief adds raft to part if needed
    //! \param part the part to process
    //! \param totalLayers numbers of layers in part
    //! \param part_sb the settings to use
    void processRaft(QSharedPointer<Part> part, int totalLayers, QSharedPointer<SettingsBase> sb);

    //! \brief Global layer
    QSharedPointer<GlobalLayer> m_current_layer;

    //! \brief tracks if m_current_points, m_start_indicies, and m_previous_regions list have been initialized
    bool m_connect_path_initialized = false;

    //! \brief maintains the end points of last layer; used to connect to next layer
    QVector<Point> m_current_points;

    //! \brief maintains the start_indicies for regions; used to connect to next layer
    QVector<int> m_start_indices;

    //! \brief maintains the previous regions list; used to connect to next layer
    QVector<QVector<QSharedPointer<RegionBase>>> m_previous_regions_list;

    //! \brief The Preprocessor used by this slicer
    QSharedPointer<Preprocessor> m_preprocessor;

    //! \brief The extra distance the part is shifted off the bed from rafts
    Distance m_raft_shift;
};
} // namespace ORNL
