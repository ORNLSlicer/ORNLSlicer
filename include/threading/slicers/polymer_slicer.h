#pragma once

#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "threading/traditional_ast.h"

namespace ORNL {
/*!
 * \class PolymerSlicer
 * \brief Implementation of SlicingThread for polymer slices.
 */
class PolymerSlicer : public TraditionalAST {
  public:
    PolymerSlicer(QString gcodeLocation);

  protected:
    //! \brief Creates layer steps by performing cross-sections.
    //! \param opt_data: optional sensor data
    void preProcess(nlohmann::json opt_data = nlohmann::json()) override;

    //! \brief Post processing including support, etc.
    //! \param opt_data: optional sensor data
    void postProcess(nlohmann::json opt_data = nlohmann::json()) override;

    //! \brief Parent override.  Writes out gcode.
    //! \param file: File pointer to write gcode out to
    //! \param base: WriterBase that creates actual gcode output
    void writeGCode() override;

  private:
    //! \brief Processes perimeters on a part. Gives each perimeter the total layer count.
    //! \param part: The part whose perimeters need processing
    //! \param part_start: The first layer number of the part
    //! \param last_layer_count: The total layer count of the part
    void processPerimeter(QSharedPointer<Part> part, int part_start, int last_layer_count);

    //! \brief Gives the infill the total layer count.
    //! \param part: The part whose perimeters need processing
    //! \param part_start: The first layer number of the part
    //! \param last_layer_count: The total layer count of the part
    void processInfill(QSharedPointer<Part> part, int part_start, int last_layer_count);

    //! \brief Creates layer steps for support structure
    //! \param part: Part to create supports for
    //! \param layer_count: Total number of layers
    //! \param partStart: Index for starting layer of current part
    void processSupport(QSharedPointer<Part> part, int layer_count, int partStart);

    //! \brief processes skins on a part
    //! \param part: the part to process
    //! \param part_start: the layer number to start on
    //! \param last_layer_count: the last layer number
    void processSkin(QSharedPointer<Part> part, int part_start, int last_layer_count);

    //! \brief adds raft to part if needed
    //! \param part: the part to process
    //! \param part_start: the layer number to start on
    //! \param part_sb :the settings to use
    void processRaft(QSharedPointer<Part> part, int part_start, QSharedPointer<SettingsBase> part_sb);

    //! \brief adds brim to part if needed
    //! \param part: the part to process
    //! \param part_sb: the settings to use
    void processBrim(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb);

    //! \brief adds skirt to part if needed
    //! \param part: the part to process
    //! \param part_sb: the settings to use
    void processSkirt(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb);

    //! \brief adds thermal scans to part if needed
    //! \param part: the part to process
    //! \param part_sb: the settings to use
    void processThermalScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb);

    //! \brief adds laser scans to part if needed
    //! \param part: the part to process
    //! \param part_sb: the settings to use
    void processLaserScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb);

    //! \brief adds anchors to part if needed
    //! \param part: the part to process
    //! \param part_sb: the settings to use
    void processAnchors(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb);

    //! \brief computes global layer assignments for a set of parts
    //! \param parts: parts to use
    //! \param settings: settings to use
    void processGlobalLayers(QVector<QSharedPointer<Part>> parts, const QSharedPointer<SettingsBase>& settings);

    void assignNozzles(const QSharedPointer<SettingsBase>& settings);

    //! \brief computes layer links for threading, used in SinglePath algorithm
    //! \param parts: parts to use
    void processLayerLinks(QVector<QSharedPointer<Part>> parts);

    //! \brief checks if any parts is the CSM are dirty
    //! \return if any part is dirty
    bool anythingDirty();

    //! \brief list of global layers
    QList<QSharedPointer<GlobalLayer>> m_global_layers;

    //! \brief cached layer settings
    QList<QSharedPointer<SettingsBase>> m_saved_layer_settings;

    //! \brief height of the half-height bead in the first layer
    int m_half_layer_height = 0;

    //! \brief layer that we are currently on
    uint m_layer_num;
};
} // namespace ORNL
