#include "threading/slicers/real_time_polymer_slicer.h"

#include <QSharedPointer>
#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qvectornd.h>

#include "geometry/mesh/mesh_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "optimizers/layer_order_optimizer.h"
#include "part/part.h"
#include "slicing/buffered_slicer.h"
#include "slicing/layer_additions.h"
#include "slicing/preprocessor.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/island/polymer_island.h"
#include "step/layer/layer.h"
#include "step/layer/regions/region_base.h"
#include "threading/real_time_ast.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {

RealTimePolymerSlicer::RealTimePolymerSlicer(QString gcodeLocation) : RealTimeAST(gcodeLocation) {}

void RealTimePolymerSlicer::initialSetup() {
    // Build a preprocessor
    m_preprocessor = QSharedPointer<Preprocessor>::create();

    // Add alter global settings, check for part overlap, segment root
    m_preprocessor->addInitialProcessing(
        [this](const Preprocessor::Parts& parts, const QSharedPointer<SettingsBase>& global_settings) {
            // Alter settings
            global_settings->makeGlobalAdjustments();

            // Check for overlaps of settings parts and prevent them
            if (SlicingUtilities::doPartsOverlap(parts.settings_parts, Plane(Point(1, 1, 1), QVector3D(0, 0, 1))))
                return true; // Cancel Slicing

            return false; // No error, so continune slicing
        });

    // Add mesh clipping
    m_preprocessor->addMeshProcessing([this](QSharedPointer<MeshBase> mesh, QSharedPointer<SettingsBase> part_sb) {
        // Clip meshes
        auto clipping_meshes = SlicingUtilities::GetMeshesByType(CSM->parts(), MeshType::kClipping);
        SlicingUtilities::ClipMesh(mesh, clipping_meshes);

        return false; // No error, so continune slicing
    });

    // Setup how to build steps
    m_preprocessor->addStepBuilder(
        [this](QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta, Preprocessor::ActivePartMeta& meta) {
            meta.part->clearSteps(); // Remove and old steps

            QSharedPointer<Layer> layer =
                QSharedPointer<Layer>::create(next_layer_meta->number + 1, next_layer_meta->settings);
            layer->setSettingsPolygons(next_layer_meta->settings_polygons);

            // Add data from cross-sectioning to a layer
            layer->setGeometry(next_layer_meta->geometry, next_layer_meta->average_normal);
            layer->setOrientation(next_layer_meta->plane,
                                  next_layer_meta->shift_amount + next_layer_meta->additional_shift);
            layer->setRaftShift(QVector3D(0, 0, 1) *
                                m_raft_shift()); // Offset this layer based on how many raftswe have added, rafts are
                                                 // always perpendicular to the build plate

            // Create the islands from the geometry.
            QVector<PolygonList> split_geometry = next_layer_meta->geometry.splitIntoParts();
            for (const PolygonList& island_geometry : split_geometry) {
                // Polymer builds use polymer islands.
                QSharedPointer<PolymerIsland> poly_isl = QSharedPointer<PolymerIsland>::create(
                    island_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
                layer->addIsland(IslandType::kPolymer, poly_isl);
            }

            // If rafts are enabled and we have not added enough of them yet, append a raft layer instead
            //! \note once enough rafts have been processed, the preprocessor needs to be instructed to start consuming
            //! those layers in the buffered slicer
            int raft_layers = meta.part_sb->setting<int>(MS::PlatformAdhesion::kRaftLayers);
            if (meta.part_sb->setting<bool>(MS::PlatformAdhesion::kRaftEnable) && meta.steps_processed <= raft_layers) {
                auto raft_layer = LayerAdditions::createRaft(layer);

                // Offset raft height based on how many layers have been completed
                raft_layer->setRaftShift(QVector3D(0, 0, 1) * m_raft_shift());
                meta.part->appendStep(raft_layer);

                Distance raft_layer_height = raft_layer->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
                meta.current_height += raft_layer_height;

                m_raft_shift += raft_layer_height;

                // Tell the preprocessor to start consuming layers for this part
                if (meta.steps_processed == raft_layers)
                    meta.consuming = true;
            }
            else {
                meta.part->appendStep(layer);
                meta.current_height += layer->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
            }

            return false; // No need to stop slicing
        });

    m_preprocessor->addCrossSectionProcessing([this](Preprocessor::ActivePartMeta& meta) {
        processBrim(meta.part, meta.steps_processed, meta.part_sb);
        processSkirt(meta.part, meta.steps_processed, meta.part_sb);
        processThermalScan(meta.part, meta.part_sb);
        processLaserScan(meta.part, meta.steps_processed, meta.part_sb, meta.current_height);

        return false;
    });

    m_preprocessor->addFinalProcessing(
        [this](const Preprocessor::Parts& parts, const QSharedPointer<SettingsBase>& global_settings) {
            m_current_layer = LayerOrderOptimizer::populateStep(parts.build_parts);

            return false; // No need to hault slicing
        });

    // Run inital processing
    m_preprocessor->processInital();
}

void RealTimePolymerSlicer::preProcess(nlohmann::json opt_data) {
    m_cross_section_generated = m_preprocessor->processNext();
}

void RealTimePolymerSlicer::postProcess(nlohmann::json opt_data) {
    QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
    global_sb->makeGlobalAdjustments();

    // if this is the first layer & we haven't set up the info for connecting paths, do the set up
    if (!m_connect_path_initialized) {
        int num_nozzles = global_sb->setting<int>(ES::MultiNozzle::kNozzleCount);
        for (int i = 0; i < num_nozzles; ++i) {
            m_current_points.push_back(Point(0, 0, 0));
            m_start_indices.push_back(-1);
            m_previous_regions_list.push_back(QVector<QSharedPointer<RegionBase>>());
        }
        m_connect_path_initialized = true;
    }

    // call to connect paths updates member variables for next call on next layer
    m_current_layer->connectPaths(global_sb, m_current_points, m_start_indices, m_previous_regions_list);
}

void RealTimePolymerSlicer::writeGCode() {
    m_gcode_output += m_base->writeLayerChange(m_steps_done);
    m_gcode_output += m_base->writeBeforeLayer(m_current_layer->getMinZ(), GSM->getGlobal());

    m_gcode_output += m_current_layer->writeGCode(m_base);
    m_current_layer->setDirtyBit(false);

    m_gcode_output += m_base->writeAfterLayer();
}

void RealTimePolymerSlicer::skip(int num_layers) {
    for (int i = 0, end = num_layers - 1; i < end; ++i)
        preProcess();
}

void RealTimePolymerSlicer::processBrim(QSharedPointer<Part> part, int totalLayers, QSharedPointer<SettingsBase> sb) {
    if (sb->setting<bool>(MS::PlatformAdhesion::kBrimEnable)) {
        if (totalLayers < sb->setting<int>(MS::PlatformAdhesion::kBrimLayers)) {
            auto layer = part->getLastStepPair().printing_layer;
            LayerAdditions::addBrim(layer);
        }
    }
}

void RealTimePolymerSlicer::processSkirt(QSharedPointer<Part> part, int totalLayers, QSharedPointer<SettingsBase> sb) {
    if (sb->setting<bool>(MS::PlatformAdhesion::kSkirtEnable)) {
        int skirt_layers = sb->setting<int>(MS::PlatformAdhesion::kSkirtLayers);
        if (totalLayers < skirt_layers) {
            auto layer = part->getLastStepPair().printing_layer;
            LayerAdditions::addSkirt(layer);
        }
    }
}

void RealTimePolymerSlicer::processLaserScan(QSharedPointer<Part> part, int totalLayers,
                                             QSharedPointer<SettingsBase> sb, Distance total_height) {
    if (sb->setting<bool>(PS::LaserScanner::kLaserScanner)) {
        if (sb->setting<bool>(PS::LaserScanner::kEnableBedScan) && totalLayers == 1)
            LayerAdditions::addLaserScan(part, 0, total_height(), part->step(0, StepType::kLayer), m_temp_gcode_dir);
        else {
            if ((totalLayers - 1) % sb->setting<int>(PS::LaserScanner::kScanLayerSkip) == 0)
                LayerAdditions::addLaserScan(part, totalLayers, total_height(),
                                             part->step(totalLayers, StepType::kLayer), m_temp_gcode_dir);
        }
    }
}

void RealTimePolymerSlicer::processThermalScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> sb) {
    // If thermal scanner is enabled, add thermal_scan_islands to appropriate layers
    if (sb->setting<bool>(PS::ThermalScanner::kThermalScanner))
        LayerAdditions::addThermalScan(part->getLastStepPair().printing_layer);
}

void RealTimePolymerSlicer::processRaft(QSharedPointer<Part> part, int totalLayers, QSharedPointer<SettingsBase> sb) {}
} // namespace ORNL
