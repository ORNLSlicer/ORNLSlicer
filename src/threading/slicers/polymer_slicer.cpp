
#include "threading/slicers/polymer_slicer.h"

#include "QtCore/QDir"
#include "QtCore/QSharedPointer"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "optimizers/layer_order_optimizer.h"
#include "optimizers/multi_nozzle_optimizer.h"
#include "slicing/buffered_slicer.h"
#include "slicing/layer_additions.h"
#include "slicing/preprocessor.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/island/polymer_island.h"
#include "step/layer/island/support_island.h"
#include "step/layer/island/wire_feed_island.h"
#include "step/layer/regions/infill.h"
#include "step/layer/regions/perimeter.h"
#include "step/layer/regions/skin.h"

namespace ORNL {

PolymerSlicer::PolymerSlicer(QString gcodeLocation) : TraditionalAST(gcodeLocation) {}

void PolymerSlicer::preProcess(nlohmann::json opt_data) {
    Preprocessor pp;

    pp.addInitialProcessing(
        [this](const Preprocessor::Parts& parts, const QSharedPointer<SettingsBase>& global_settings) {
            // Alter settings
            global_settings->makeGlobalAdjustments();

            // Check for overlaps of settings parts and prevent them
            if (SlicingUtilities::doPartsOverlap(parts.settings_parts, Plane(Point(1, 1, 1), QVector3D(0, 0, 1)))) {
                return true; // Cancel Slicing
            }

            return false; // No error, so continune slicing
        });

    pp.addPartProcessing([this](QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
        m_saved_layer_settings.clear();

        if (m_half_layer_height != 0) {
            m_half_layer_height = 0;
        }

        // Caching does not work correctly - just always clear.
        part->clearSteps();

        return false;
    });

    pp.addMeshProcessing([this](QSharedPointer<MeshBase> mesh, QSharedPointer<SettingsBase> part_sb) {
        // Clip meshes
        auto clipping_meshes = SlicingUtilities::GetMeshesByType(CSM->parts(), MeshType::kClipping);
        SlicingUtilities::ClipMesh(mesh, clipping_meshes);

        return false; // No error, so continune slicing
    });

    pp.addStepBuilder([this](QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta,
                             Preprocessor::ActivePartMeta& meta) {
        auto addNewLayer = [this](QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta,
                                  Preprocessor::ActivePartMeta& meta, QSharedPointer<Layer>& new_layer, int layerNum) {
            // Save settings
            m_saved_layer_settings.push_back(next_layer_meta->settings);

            new_layer = QSharedPointer<Layer>::create(next_layer_meta->number, next_layer_meta->settings);

            new_layer->setSettingsPolygons(next_layer_meta->settings_polygons);

            // add data from cross-sectioning to a layer
            new_layer->setGeometry(next_layer_meta->geometry, next_layer_meta->average_normal);

            if (layerNum == 2) {
                next_layer_meta->shift_amount.z(next_layer_meta->shift_amount.z() + m_half_layer_height);
            }

            new_layer->setOrientation(next_layer_meta->plane,
                                      next_layer_meta->shift_amount + next_layer_meta->additional_shift);
            meta.part->appendStep(new_layer);

            // Create the islands from the geometry.
            QVector<PolygonList> split_geometry = next_layer_meta->geometry.splitIntoParts();

            // If user wanted polygons manipulated by settings regions, use those instead of original
            if (!(next_layer_meta->modified_geometry.isEmpty() ||
                  next_layer_meta->setting_bounded_geometry.isEmpty())) {
                split_geometry = next_layer_meta->modified_geometry.splitIntoParts();
                split_geometry += next_layer_meta->setting_bounded_geometry.splitIntoParts();
            }

            // If wire feeding is turned on, have to create special island combinations
            if (next_layer_meta->settings->setting<bool>(ES::WireFeed::kWireFeedEnable)) {
                for (const PolygonList& island_geometry : split_geometry) {
                    // Polymer builds use polymer islands.
                    QSharedPointer<WireFeedIsland> poly_isl = QSharedPointer<WireFeedIsland>::create(
                        island_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
                    new_layer->addIsland(IslandType::kWireFeed, poly_isl);
                }
            }
            // Else, normal polymer islands
            else {
                for (const PolygonList& island_geometry : split_geometry) {
                    // Polymer builds use polymer islands.
                    QSharedPointer<PolymerIsland> poly_isl = QSharedPointer<PolymerIsland>::create(
                        island_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
                    new_layer->addIsland(IslandType::kPolymer, poly_isl);
                }
            }
        };

        // must add new
        if (next_layer_meta->number >= meta.steps_processed) {
            QSharedPointer<Layer> layer;
            QSharedPointer<Layer> layerWithSameZ;
            QSharedPointer<Layer> layer2;

            bool perimeter_enabled = next_layer_meta->settings->setting<bool>(PS::Perimeter::kEnable);
            bool shifted_beads_enabled = next_layer_meta->settings->setting<bool>(PS::Perimeter::kEnableShiftedBeads);
            bool infill_enabled = next_layer_meta->settings->setting<bool>(PS::Infill::kEnable);
            bool alternating_lines_enabled =
                next_layer_meta->settings->setting<bool>(PS::Infill::kEnableAlternatingLines);

            if ((perimeter_enabled && shifted_beads_enabled) || (infill_enabled && alternating_lines_enabled)) {
                // get height of the half sized layer on first layer creation
                if (m_half_layer_height == 0) {
                    next_layer_meta->number = (next_layer_meta->number * 2 + 1);
                    m_half_layer_height = next_layer_meta->shift_amount.z();
                }
                // if not creating the first layer
                else {
                    next_layer_meta->number = (next_layer_meta->number * 2 + 2);
                }
            }
            else {
                next_layer_meta->number++;
            }

            addNewLayer(next_layer_meta, meta, layer, 1);

            if ((perimeter_enabled && shifted_beads_enabled) || (infill_enabled && alternating_lines_enabled)) {
                // creation of layer on the same z as the layer number 1
                if (next_layer_meta->number == 1) {
                    next_layer_meta->number = next_layer_meta->number + 1;
                    addNewLayer(next_layer_meta, meta, layerWithSameZ, 3);
                }

                // creation of the next half layer
                next_layer_meta->number = next_layer_meta->number + 1;
                addNewLayer(next_layer_meta, meta, layer2, 2);
            }
        }
        else {
            // Save settings
            m_saved_layer_settings.push_back(next_layer_meta->settings);

            QSharedPointer<Layer> layer =
                meta.part->step(next_layer_meta->number + meta.part_start, StepType::kLayer).dynamicCast<Layer>();
            layer->flagIfDirtySettings(next_layer_meta->settings);
            layer->flagIfDirtySettingsPolygons(next_layer_meta->settings_polygons);

            // if already dirty, must be because of user manipulation of geometry
            // otherwise, check if settings have changed
            // if either is true, need new layer
            // TODO: make dirty recalc less restrictive
            if (layer->isDirty()) {
                QSharedPointer<Layer> newLayer =
                    QSharedPointer<Layer>::create(next_layer_meta->number + 1, next_layer_meta->settings);
                // add data from cross-sectioning to a layer
                newLayer->setGeometry(next_layer_meta->geometry, next_layer_meta->average_normal);
                newLayer->setSettingsPolygons(next_layer_meta->settings_polygons);
                newLayer->setOrientation(next_layer_meta->plane,
                                         next_layer_meta->shift_amount + next_layer_meta->additional_shift);
                meta.part->replaceStep(next_layer_meta->number + meta.part_start, newLayer);

                // Create the islands from the geometry.
                QVector<PolygonList> split_geometry = next_layer_meta->geometry.splitIntoParts();

                // If user wanted polygons manipulated by settings regions, use those instead of original
                if (!(next_layer_meta->modified_geometry.isEmpty() ||
                      next_layer_meta->setting_bounded_geometry.isEmpty())) {
                    split_geometry = next_layer_meta->modified_geometry.splitIntoParts();
                    split_geometry += next_layer_meta->setting_bounded_geometry.splitIntoParts();
                }

                // If wire feeding is turned on, have to create special island combinations
                if (next_layer_meta->settings->setting<bool>(ES::WireFeed::kWireFeedEnable)) {
                    QVector<QSharedPointer<IslandBase>> newIslands;

                    for (const PolygonList& island_geometry : split_geometry) {
                        // Polymer builds use polymer islands.
                        QSharedPointer<WireFeedIsland> poly_isl = QSharedPointer<WireFeedIsland>::create(
                            island_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
                        newIslands.append(poly_isl);
                    }
                    newLayer->updateIslands(IslandType::kWireFeed, newIslands);
                }
                else {
                    QVector<QSharedPointer<IslandBase>> newIslands;
                    for (const PolygonList& island_geometry : split_geometry) {
                        // Polymer builds use polymer islands.
                        QSharedPointer<PolymerIsland> poly_isl = QSharedPointer<PolymerIsland>::create(
                            island_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
                        newIslands.append(poly_isl);
                    }
                    newLayer->updateIslands(IslandType::kPolymer, newIslands);
                }
            }
        }

        return false; // No error, so continune slicing
    });

    pp.addCrossSectionProcessing([this](Preprocessor::ActivePartMeta& meta) {
        // If fewer layers than last slice, remove all steps from that layer onwards
        meta.part->clearStepsFromIndex(meta.last_step_count + meta.part_start);

        //! If perimeters are enabled, give each perimeter the total number of layers
        if (meta.part_sb->setting<bool>(PS::Perimeter::kEnable)) {
            processPerimeter(meta.part, meta.part_start, meta.last_step_count);
        }

        //! If infill alternating lines are enabled, give the infill the total number of layers
        if (meta.part_sb->setting<bool>(PS::Infill::kEnableAlternatingLines)) {
            processInfill(meta.part, meta.part_start, meta.last_step_count);
        }

        //! If skins are enabled, give each skin its upper and lower geometry
        if (meta.part_sb->setting<bool>(PS::Skin::kEnable)) {
            processSkin(meta.part, meta.part_start, meta.last_step_count);
        }

        //! If supports are enabled, find overhangs and add support_islands to layer below overhangs
        if (meta.part_sb->setting<bool>(PS::Support::kEnable) && meta.last_step_count > 0) {
            processSupport(meta.part, meta.last_step_count, meta.part_start);
        }

        // Layer Additions
        processRaft(meta.part, meta.part_start, meta.part_sb);
        processBrim(meta.part, meta.part_sb);
        processSkirt(meta.part, meta.part_sb);
        processLaserScan(meta.part, meta.part_sb);
        processThermalScan(meta.part, meta.part_sb);
        processAnchors(meta.part, meta.part_sb);

        // Update max steps
        if (meta.part->countStepPairs() > this->getMaxSteps()) {
            this->setMaxSteps(meta.part->countStepPairs());
        }

        return false; // No error, so continune slicing
    });

    pp.addStatusUpdate([this](double percentage) { emit statusUpdate(StatusUpdateStepType::kPreProcess, percentage); });

    pp.addFinalProcessing(
        [this](const Preprocessor::Parts& parts, const QSharedPointer<SettingsBase>& global_settings) {
            // Compute and populate global layers
            processGlobalLayers(parts.build_parts, global_settings);

            // Assign pathing to nozzles
            assignNozzles(global_settings);

            // Adds thread links for single path zippering and exclusion
            processLayerLinks(parts.build_parts);

            return false; // No error, so continune slicing
        });

    pp.processAll();
}

void PolymerSlicer::processPerimeter(QSharedPointer<Part> part, int part_start, int last_layer_count) {
    m_layer_num = last_layer_count;
    for (int layer_nbr = part_start; layer_nbr < last_layer_count; ++layer_nbr) {
        if (layer_nbr < part->countStepPairs()) {
            QSharedPointer<Layer> layer = part->step(layer_nbr, StepType::kLayer).dynamicCast<Layer>();

            if (layer->isDirty()) {
                for (QSharedPointer<IslandBase> isl : layer->getIslands()) {
                    QSharedPointer<Perimeter> perimeter =
                        isl->getRegion(RegionType::kPerimeter).dynamicCast<Perimeter>();
                }
            }
        }
    }
}

void PolymerSlicer::processSkin(QSharedPointer<Part> part, int part_start, int last_layer_count) {
    for (int layer_nr = part_start; layer_nr < last_layer_count; layer_nr++) {
        if (layer_nr < part->countStepPairs()) {
            QSharedPointer<Layer> layer = part->step(layer_nr, StepType::kLayer).dynamicCast<Layer>();

            if (layer->isDirty()) {
                int gradual_steps = 0;
                if (layer->getSb()->setting<bool>(PS::Skin::kInfillEnable))
                    gradual_steps = layer->getSb()->setting<int>(PS::Skin::kInfillSteps);

                //! Gather skin counts
                int bottom_count = layer->getSb()->setting<int>(PS::Skin::kBottomCount);
                int top_count = layer->getSb()->setting<int>(PS::Skin::kTopCount);

                //! Set bounds
                int upper_bound = qMin(layer_nr + top_count, last_layer_count + part_start - 1);
                int lower_bound = qMax(layer_nr - bottom_count, part_start);
                int gradual_bound = qMin(upper_bound + gradual_steps, last_layer_count + part_start - 1);

                //! Determine if upper and lower ranges include top and bottom layer respectively
                bool top {upper_bound == last_layer_count + part_start - 1};
                bool bottom {lower_bound == part_start};
                bool gradual = (gradual_bound == last_layer_count + part_start - 1) ? true : false;

                //! Gather upper and lower geometries
                for (QSharedPointer<IslandBase> isl : layer->getIslands()) {
                    QSharedPointer<Skin> skin = isl->getRegion(RegionType::kSkin).dynamicCast<Skin>();
                    skin->setGeometryIncludes(top, bottom, gradual);

                    //! Upper geometry
                    for (int i = layer_nr + 1; i <= upper_bound; ++i)
                        skin->addUpperGeometry(part->step(i, StepType::kLayer).dynamicCast<Layer>()->getGeometry());

                    //! Gradual geometry
                    for (int i = upper_bound + 1; i <= gradual_bound; ++i)
                        skin->addGradualGeometry(part->step(i, StepType::kLayer).dynamicCast<Layer>()->getGeometry());

                    //! Lower geometry
                    for (int i = lower_bound; i < layer_nr; ++i)
                        skin->addLowerGeometry(part->step(i, StepType::kLayer).dynamicCast<Layer>()->getGeometry());
                }
            }
        }
    }
}

void PolymerSlicer::processInfill(QSharedPointer<Part> part, int part_start, int last_layer_count) {
    m_layer_num = last_layer_count;
    for (int layer_nbr = part_start; layer_nbr < last_layer_count; ++layer_nbr) {
        if (layer_nbr < part->countStepPairs()) {
            QSharedPointer<Layer> layer = part->step(layer_nbr, StepType::kLayer).dynamicCast<Layer>();

            if (layer->isDirty()) {
                for (QSharedPointer<IslandBase> isl : layer->getIslands()) {
                    QSharedPointer<Infill> infill = isl->getRegion(RegionType::kInfill).dynamicCast<Infill>();
                    infill->setLayerCount(last_layer_count);
                }
            }
        }
    }
}

void PolymerSlicer::processRaft(QSharedPointer<Part> part, int part_start, QSharedPointer<SettingsBase> part_sb) {
    if (!part->steps(StepType::kLayer).empty()) {
        if (part_sb->setting<bool>(MS::PlatformAdhesion::kRaftEnable)) {
            int raft_layers = part_sb->setting<int>(MS::PlatformAdhesion::kRaftLayers);
            Distance height_offset = 0.0;

            auto steps = part->steps(StepType::kLayer);
            auto first_layer = steps.first().dynamicCast<Layer>();
            QVector<QSharedPointer<Layer>> new_raft_layers;
            for (int i = 0; i < raft_layers; ++i) {
                auto raft_layer = LayerAdditions::createRaft(first_layer);

                // Offset raft height based on how many layers have been completed
                raft_layer->setRaftShift(first_layer->getSlicingPlane().normal() * height_offset());
                new_raft_layers.push_back(raft_layer);

                height_offset += raft_layer->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
            }

            // Offset steps based on height added by raft layers
            for (auto step : steps)
                step->setRaftShift(first_layer->getSlicingPlane().normal() * height_offset());

            // Add new raft steps
            for (int i = new_raft_layers.size() - 1; i >= 0; --i)
                part->prependStep(new_raft_layers[i]);
        }
        else {
            if (part_start != 0) {
                for (int i = 0; i < part_start; ++i) {
                    part->removeStepAtIndex(0);
                }
            }
        }
    }
}

void PolymerSlicer::processBrim(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (part_sb->setting<bool>(MS::PlatformAdhesion::kBrimEnable)) {
        QList<QSharedPointer<Step>> steps = part->steps(StepType::kLayer);
        for (int i = 0, end = steps.size(); i < end; ++i) {
            if (i < part_sb->setting<int>(MS::PlatformAdhesion::kBrimLayers))
                LayerAdditions::addBrim(steps[i].dynamicCast<Layer>());
        }
    }
}

void PolymerSlicer::processSkirt(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (part_sb->setting<bool>(MS::PlatformAdhesion::kSkirtEnable)) {
        QList<QSharedPointer<Step>> steps = part->steps(StepType::kLayer);
        for (int i = 0, end = steps.size(); i < end; ++i) {
            if (i < part_sb->setting<int>(MS::PlatformAdhesion::kSkirtLayers))
                LayerAdditions::addSkirt(steps[i].dynamicCast<Layer>());
        }
    }
}

void PolymerSlicer::processThermalScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (part_sb->setting<bool>(PS::ThermalScanner::kThermalScanner)) {
        int first_layer = 0;

        // If bed scan is enabled for laser scan, the first layer for the thermal scan is layer 1
        if (part_sb->setting<bool>(PS::LaserScanner::kLaserScanner) &&
            part_sb->setting<bool>(PS::LaserScanner::kEnableBedScan))
            first_layer = 1;

        int total_layers = part->countStepPairs();
        for (int current_layer = first_layer; current_layer < total_layers; ++current_layer) {
            LayerAdditions::addThermalScan(part->step(current_layer, StepType::kLayer).dynamicCast<Layer>());
        }
    }
}

void PolymerSlicer::processLaserScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (!m_saved_layer_settings.isEmpty() &&
        m_saved_layer_settings.first()->setting<bool>(PS::LaserScanner::kLaserScanner) && !part->steps().isEmpty()) {
        if (m_saved_layer_settings.first()->setting<bool>(PS::LaserScanner::kLaserScanner)) {
            double scan_height_total = 0;
            if (m_saved_layer_settings.first()->setting<bool>(PS::LaserScanner::kEnableBedScan)) {
                LayerAdditions::addLaserScan(part, 0, 0, part->step(0, StepType::kLayer), m_temp_gcode_dir);
            }
            else {
                part->removeStepFromGroup(0, StepType::kScan);
            }

            int scan_layer_skip = m_saved_layer_settings.first()->setting<int>(PS::LaserScanner::kScanLayerSkip);
            for (int current_layer = 1, layer_count = part->countStepPairs(); current_layer < layer_count;
                 ++current_layer) {
                QSharedPointer<Layer> previousLayer =
                    part->step(current_layer - 1, StepType::kLayer).dynamicCast<Layer>();
                scan_height_total += previousLayer->getSb()->setting<double>(PS::Layer::kLayerHeight);

                if ((current_layer - 1) % scan_layer_skip != 0)
                    part->removeStepFromGroup(current_layer, StepType::kScan);
                else
                    LayerAdditions::addLaserScan(part, current_layer, scan_height_total,
                                                 part->step(current_layer, StepType::kLayer), m_temp_gcode_dir);
            }
        }
        else {
            for (int i = part->countStepPairs() - 1; i >= 0; --i) {
                part->removeStepFromGroup(i, StepType::kScan);
            }
        }
    }
}

void PolymerSlicer::processAnchors(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (part_sb->setting<bool>(ES::WireFeed::kAnchorEnable)) {
        int total_layers = part->countStepPairs();
        for (int current_layer = 0; current_layer < total_layers; ++current_layer)
            LayerAdditions::addAnchors(part->step(current_layer, StepType::kLayer).dynamicCast<Layer>());
    }
}

void PolymerSlicer::processGlobalLayers(QVector<QSharedPointer<Part>> parts,
                                        const QSharedPointer<SettingsBase>& settings) {
    if (anythingDirty()) {
        // create global layers from all the part layers
        m_global_layers = LayerOrderOptimizer::populateSteps(settings, parts);
    }
}

void PolymerSlicer::assignNozzles(const QSharedPointer<SettingsBase>& settings_base) {
    int tool_count = settings_base->setting<int>(ES::MultiNozzle::kNozzleCount);
    if (tool_count == 1) {
        // default, set every island to extruder 0
        for (auto g_layer : m_global_layers) {
            QVector<QSharedPointer<IslandBase>> layer_islands = g_layer->getIslands();
            for (auto island : layer_islands)
                island->setExtruder(0);
        }
    }
    // more than one nozzle & nozzles are independent
    else if (settings_base->setting<bool>(ES::MultiNozzle::kEnableIndependentNozzles)) {
        NozzleAssignmentMethod assignment_method =
            settings_base->setting<NozzleAssignmentMethod>(ES::MultiNozzle::kNozzleAssignmentMethod);

        // iterate through global layers and assign nozzles
        for (auto global_layer : m_global_layers) {
            QVector<QSharedPointer<IslandBase>> layer_islands = global_layer->getIslands();
            switch (assignment_method) {
                case NozzleAssignmentMethod::kXLocation:
                    MultiNozzleOptimizer::assignByAxisLocation(layer_islands, tool_count, Axis::kX);
                    break;
                case NozzleAssignmentMethod::kYLocation:
                    MultiNozzleOptimizer::assignByAxisLocation(layer_islands, tool_count, Axis::kY);
                    break;
                case NozzleAssignmentMethod::kArea:
                    MultiNozzleOptimizer::assignByArea(layer_islands, tool_count);
                    break;
            }
        }
    }
}

void PolymerSlicer::processLayerLinks(QVector<QSharedPointer<Part>> parts) {
    for (auto& part : parts) {
        auto part_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal()); // Copy global
        part_sb->populate(part->getSb());                                       // Fill with part overrides
        bool enable_single_path = part_sb->setting<bool>(ES::SinglePath::kEnableSinglePath);
        bool enable_exclusion = part_sb->setting<bool>(ES::SinglePath::kEnableBridgeExclusion);
        bool enable_zippering = part_sb->setting<bool>(ES::SinglePath::kEnableZippering);

        // Link layer threads for zippering/ exclusion
        if (enable_single_path) {
            auto sync = part->getSync();
            sync->clearLinks();
            for (auto step : part->steps()) {
                QSharedPointer<Layer> layer = step.dynamicCast<Layer>();
                int layer_num = layer->getLayerNumber();

                // Exclusion links links
                if (enable_exclusion && (layer_num + 1 <= part->steps().size())) {
                    for (auto island : layer->getIslands()) {
                        // Add a link to next layer
                        sync->addLink(layer_num, layer_num + 1, LinkType::kPreviousLayerExclusionPerimeter);
                        sync->addLink(layer_num, layer_num + 1, LinkType::kPreviousLayerExclusionInset);
                    }
                }

                // Zipper links
                if (enable_zippering && (layer_num + 2 < part->steps().size())) {
                    for (auto island : layer->getIslands()) {
                        // Add a link to prev zipper layer
                        sync->addLink(layer_num, layer_num + 2, LinkType::kZipperingPerimeter);
                        sync->addLink(layer_num, layer_num + 2, LinkType::kZipperingInset);
                    }
                }
            }
        }
    }
}

bool PolymerSlicer::anythingDirty() {
    bool anything_dirty = false;
    for (QSharedPointer<Part> curr_part : CSM->parts().values()) {
        if (curr_part->isPartDirty()) {
            anything_dirty = true;
            break;
        }
    }
    return anything_dirty;
}

// Because of possible non-zero xy_offset and non-zero layer_offset, the original code that "added the overhangs
//  to lower_layer and use the modified lower_layer as upper_layer for the next layer down" no longer works
// The new approach breaks down the implementation in two loops iterating layers from top to bottom:
// 1. First loop, get overhangs defined for each layer without considering xy_offset and layer_offset
// 2. Second loop, apply the overhangs to each layer considering layer_offset and xy_offset
// layer_offet will be considered first, then xy_offset:
// If layer_offset=m, support at layer #N will be calculated as the difference between
// layer #N+1+m and layer #N+m, or the overhang of layer #N+1+m over the one below #N+m
// The calculated overhang is then applied to layer #N
// Get the overlap between the overhang and (layer #N with xy_offset)
// If the overlap > 0, remove the overlap from the overhang, and add the result to layer #N
void PolymerSlicer::processSupport(QSharedPointer<Part> part, int layer_count, int partStart) {
    if (!part->steps().isEmpty()) {
        auto part_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal()); // Copy global
        part_sb->populate(part->getSb());                                       // Fill with part overrides
        //! Determine the offset distance that should be used for support creation
        Distance support_xy_distance = part_sb->setting<Distance>(PS::Support::kXYDistance);
        Angle support_threshold_angle = part_sb->setting<Angle>(PS::Support::kThresholdAngle);
        Distance layer_height = part_sb->setting<Distance>(PS::Layer::kLayerHeight);
        Distance horizontal_offset = max(support_xy_distance, Distance(layer_height * tan(support_threshold_angle)));
        int layer_offset = part_sb->setting<int>(PS::Support::kLayerOffset);

        //! If tapering is enabled, set taper distance
        Distance taper = 0;
        if (part_sb->setting<bool>(PS::Support::kTaper))
            taper = part_sb->setting<Distance>(PS::Layer::kBeadWidth) / 2;

        Area minimum_support_area = part_sb->setting<Area>(PS::Support::kMinArea);

        // Compare each layer to the one below it looking for overhangs
        QVector<PolygonList> layers_overhangs; // for all but top layer, count = layer_count - 1

        // Layer #N, if XY offset > 0, there will be a gap between the part contour and the support region
        // When calculating the support next layer down (layer #N-1), the gap in the upper_layer (layer #N)
        // must be eliminated. Therefore,
        // upper_layer_islands is introduced to represent the upper layer with the gap eliminated
        // It is initialized as the very top layer, and will be updated each time for each layer
        PolygonList upper_layer_islands;
        QSharedPointer<Layer> upper_layer0 =
            part->step(layer_count + partStart - 1, StepType::kLayer).dynamicCast<Layer>();
        for (QSharedPointer<IslandBase> island : upper_layer0->getIslands()) {
            if (taper > 0)
                upper_layer_islands += island->getGeometry().offset(-taper);
            else
                upper_layer_islands += island->getGeometry();
        }
        // First iteration, without considering either XY_offset or layer_offset
        // this will populate vector layers_overhangs[]
        for (int current_layer = layer_count + partStart - 1; current_layer > partStart; --current_layer) {
            QSharedPointer<Layer> upper_layer = part->step(current_layer, StepType::kLayer).dynamicCast<Layer>();
            QSharedPointer<Layer> lower_layer = part->step(current_layer - 1, StepType::kLayer).dynamicCast<Layer>();
            if (upper_layer->isDirty()) {
                PolygonList lower_layer_islands;
                PolygonList overhangs;

                //! Gather lower layer island geometries
                for (QSharedPointer<IslandBase> island : lower_layer->getIslands()) {
                    lower_layer_islands += island->getGeometry();
                }

                // overhangs is to be added to the lower_layer_island
                // which is then served as the upper_layer_islands for the next layer down
                overhangs = upper_layer_islands - lower_layer_islands;
                layers_overhangs.push_front(overhangs);

                // prepare upper_layer_islands for the next layer down, initialize as lower_layer
                upper_layer_islands = lower_layer_islands;

                // add overhangs to upper_layer_islands only if overhangs is not null
                if (overhangs.count() > 0) {
                    QVector<PolygonList> support_islands = overhangs.splitIntoParts();

                    // Remove support island geometries that are smaller than the minimum support area
                    // overhangs >= real overhangs considering xy_offset, so no issue on applying the criteria here
                    for (int i = support_islands.size() - 1; i >= 0; --i) {
                        if (support_islands[i].netArea() < minimum_support_area)
                            support_islands.remove(i);
                    }

                    for (const PolygonList& island : support_islands) {
                        upper_layer_islands += island;
                    }
                }
                // consider tapering
                if (taper > 0) {
                    upper_layer_islands = upper_layer_islands.offset(-taper);
                }
            }
        }

        // Second top down iteration, assign already calculated overhangs[] to each layer
        // layer_offset determines which item is used: overhangs[layerId + layer_offset]
        // Also consider XY_offset by deleting any overlap between the assigned overhangs and the layer
        // Apply the modified overhangs to the layer
        for (int current_layer = layer_count + partStart - 1; current_layer > partStart; --current_layer) {
            QSharedPointer<Layer> upper_layer = part->step(current_layer, StepType::kLayer).dynamicCast<Layer>();
            QSharedPointer<Layer> lower_layer = part->step(current_layer - 1, StepType::kLayer).dynamicCast<Layer>();
            if (upper_layer->isDirty()) {
                // apply layers_overhangs[] to the lower layer, considering layer_offset
                // create support islands and add them to lower_layer only if layers_overhangs[layer_offset] is not null
                int overhangIndex = current_layer - 1 + layer_offset;

                if ((overhangIndex < layer_count - 1) && (layers_overhangs[overhangIndex].size() > 0)) {
                    // delete the overlap of layers_overhangs[layer_offset] and lower_layer_islands_offset
                    PolygonList lower_layer_islands_offset;
                    for (QSharedPointer<IslandBase> island : lower_layer->getIslands()) {
                        lower_layer_islands_offset += island->getGeometry().offset(horizontal_offset);
                    }
                    PolygonList overlap = layers_overhangs[overhangIndex] & lower_layer_islands_offset;
                    layers_overhangs[overhangIndex] = layers_overhangs[overhangIndex] - overlap;

                    QVector<PolygonList> support_islands = layers_overhangs[overhangIndex].splitIntoParts();

                    // Remove support island geometries that are smaller than the minimum support area
                    for (int i = support_islands.size() - 1; i >= 0; --i) {
                        if (support_islands[i].netArea() < minimum_support_area)
                            support_islands.remove(i);
                    }

                    if (support_islands.size() > 0) {
                        // if upper_layer == layer #1 of the part, lower_layer, if needed as the support underneath the
                        // part, is still empty SettingsBase must come from upper_layer Furthermore, if two or more
                        // support layers are underneath the part, upper_layer can be a newly added support layer
                        // without a kPolymer type island
                        QSharedPointer<SettingsBase> currentLocalSettings;
                        if (lower_layer->getIslands().count() > 0)
                            currentLocalSettings = QSharedPointer<SettingsBase>::create(
                                *lower_layer->getIslands(IslandType::kPolymer)[0]->getSb());
                        else {
                            if (upper_layer->getIslands(IslandType::kPolymer).count() > 0)
                                currentLocalSettings = QSharedPointer<SettingsBase>::create(
                                    *upper_layer->getIslands(IslandType::kPolymer)[0]->getSb());
                            else if (upper_layer->getIslands(IslandType::kSupport).count() > 0)
                                currentLocalSettings = QSharedPointer<SettingsBase>::create(
                                    *upper_layer->getIslands(IslandType::kSupport)[0]->getSb());
                            else {
                                qDebug() << "Error: cannot get SettingsBase from 1. lower_layer kPolymer island, 2. "
                                            "upper_layer kPolymer island, 3. upper_layer kSupport island";
                                continue;
                            }
                        }

                        // Create support islands and add them to the lower layer
                        for (const PolygonList& island : support_islands) {
                            QSharedPointer<SupportIsland> support_island = QSharedPointer<SupportIsland>::create(
                                island, currentLocalSettings, QVector<SettingsPolygon>());
                            lower_layer->addIsland(IslandType::kSupport, support_island);
                        }
                    }
                }
            }
        }
    }
}

void PolymerSlicer::postProcess(nlohmann::json opt_data) {
    if (anythingDirty()) {
        QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
        global_sb->makeGlobalAdjustments();

        // set up the start points, first region indicies, and previous region list for each tool
        // used by island and path order optimizer to generate travels
        // in these vectors, index 0 corresponds to tool 0, index 1 to tool 1, etc.
        QVector<Point> current_points;
        QVector<int> start_indices;
        QVector<QVector<QSharedPointer<RegionBase>>> previous_regions_list;

        int num_nozzles = global_sb->setting<int>(ES::MultiNozzle::kNozzleCount);
        for (int i = 0; i < num_nozzles; ++i) {
            current_points.push_back(Point(0, 0, 0));
            start_indices.push_back(-1);
            previous_regions_list.push_back(QVector<QSharedPointer<RegionBase>>());
        }

        for (int g_layer_num = 0, max_layers = m_global_layers.size(); g_layer_num < max_layers; ++g_layer_num) {
            m_global_layers[g_layer_num]->unorient();

            // if there are multiple nozzles that are NOT independent
            if (global_sb->setting<int>(ES::MultiNozzle::kNozzleCount) > 1 &&
                !global_sb->setting<bool>(ES::MultiNozzle::kEnableIndependentNozzles)) {
                m_global_layers[g_layer_num]->adjustFixedMultiNozzle();
            }

            // current_points, start_indices, & previous_regions_list are updated during method execution
            // so that each layer starts where the last layer ended
            m_global_layers[g_layer_num]->connectPaths(global_sb, current_points, start_indices, previous_regions_list);

            m_global_layers[g_layer_num]->calculateModifiers(global_sb, current_points, g_layer_num);

            m_global_layers[g_layer_num]->reorient();

            // update status in UI
            emit statusUpdate(StatusUpdateStepType::kPostProcess, (g_layer_num + 1) / max_layers * 100);
        }
    }
    else {
        emit statusUpdate(StatusUpdateStepType::kPostProcess, 100); // Mark layerbar as done
    }
}

void PolymerSlicer::writeGCode() {
    QTextStream stream(&m_temp_gcode_output_file);

    // for updating status window
    double current_layer = 0;
    double num_layers = m_global_layers.size();

    // have each layer write its own gcode
    for (auto g_layer : m_global_layers) {
        stream << m_base->writeLayerChange(current_layer);
        stream << m_base->writeBeforeLayer(g_layer->getMinZ(), GSM->getGlobal());

        stream << g_layer->writeGCode(m_base);
        g_layer->setDirtyBit(false);
        stream << m_base->writeAfterLayer();

        emit statusUpdate(StatusUpdateStepType::kGcodeGeneraton, (current_layer + 1) / num_layers * 100);
        ++current_layer;
    }

    stream << m_base->writeAfterPart();
}
} // namespace ORNL
