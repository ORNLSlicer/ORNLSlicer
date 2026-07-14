
#include "threading/slicers/planar_slicer.h"

#include <QtCore/QDir>
#include <QtCore/QSharedPointer>
#include <nlohmann/json_fwd.hpp>
#include <qcontainerfwd.h>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qminmax.h>
#include <qtmetamacros.h>
#include <qvectornd.h>

#include "geometry/mesh/mesh_base.h"
#include "geometry/plane.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "optimizers/layer_order_optimizer.h"
#include "part/part.h"
#include "slicing/buffered_slicer.h"
#include "slicing/layer_additions.h"
#include "slicing/preprocessor.h"
#include "slicing/slicing_utilities.h"
#include "step/layer/island/island_base.h"
#include "step/layer/island/polymer_island.h"
#include "step/layer/island/support_island.h"
#include "step/layer/layer.h"
#include "step/layer/regions/region_base.h"
#include "step/layer/regions/skin.h"
#include "threading/traditional_ast.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {

PlanarSlicer::PlanarSlicer(QString gcodeLocation) : TraditionalAST(gcodeLocation) {}

void PlanarSlicer::preProcess(nlohmann::json opt_data) {
    Preprocessor pp(false, true);

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

    pp.addStepBuilder(
        [this](QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta, Preprocessor::ActivePartMeta& meta) {
            auto addNewLayer = [this](QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta,
                                      Preprocessor::ActivePartMeta& meta, QSharedPointer<Layer>& new_layer) {
                // Save settings
                m_saved_layer_settings.push_back(next_layer_meta->settings);

                new_layer = QSharedPointer<Layer>::create(next_layer_meta->number, next_layer_meta->settings);

                new_layer->setSettingsPolygons(next_layer_meta->settings_polygons);

                // add data from cross-sectioning to a layer
                new_layer->setGeometry(next_layer_meta->geometry, next_layer_meta->average_normal);

                new_layer->setOrientation(next_layer_meta->plane,
                                          next_layer_meta->shift_amount + next_layer_meta->additional_shift);
                meta.part->appendStep(new_layer);

                // Create the islands from the geometry.
                QVector<PolygonList> split_geometry = next_layer_meta->geometry.splitIntoParts();

                for (const PolygonList& island_geometry : split_geometry) {
                    // Planar builds use polymer islands.
                    QSharedPointer<PolymerIsland> poly_isl = QSharedPointer<PolymerIsland>::create(
                        island_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
                    new_layer->addIsland(IslandType::kPolymer, poly_isl);
                }
            };

            // must add new
            if (next_layer_meta->number >= meta.steps_processed) {
                QSharedPointer<Layer> layer;

                next_layer_meta->number++;
                addNewLayer(next_layer_meta, meta, layer);
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

                    QVector<QSharedPointer<IslandBase>> newIslands;
                    for (const PolygonList& island_geometry : split_geometry) {
                        // Planar builds use polymer islands.
                        QSharedPointer<PolymerIsland> poly_isl = QSharedPointer<PolymerIsland>::create(
                            island_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
                        newIslands.append(poly_isl);
                    }
                    newLayer->updateIslands(IslandType::kPolymer, newIslands);
                }
            }

            return false; // No error, so continune slicing
        });

    pp.addCrossSectionProcessing([this](Preprocessor::ActivePartMeta& meta) {
        // If fewer layers than last slice, remove all steps from that layer onwards
        meta.part->clearStepsFromIndex(meta.last_step_count + meta.part_start);

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

            return false; // No error, so continune slicing
        });

    pp.processAll();
}

void PlanarSlicer::processSkin(QSharedPointer<Part> part, int part_start, int last_layer_count) {
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

void PlanarSlicer::processRaft(QSharedPointer<Part> part, int part_start, QSharedPointer<SettingsBase> part_sb) {
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

void PlanarSlicer::processBrim(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (part_sb->setting<bool>(MS::PlatformAdhesion::kBrimEnable)) {
        QList<QSharedPointer<Step>> steps = part->steps(StepType::kLayer);
        for (int i = 0, end = steps.size(); i < end; ++i) {
            if (i < part_sb->setting<int>(MS::PlatformAdhesion::kBrimLayers))
                LayerAdditions::addBrim(steps[i].dynamicCast<Layer>());
        }
    }
}

void PlanarSlicer::processSkirt(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (part_sb->setting<bool>(MS::PlatformAdhesion::kSkirtEnable)) {
        QList<QSharedPointer<Step>> steps = part->steps(StepType::kLayer);
        for (int i = 0, end = steps.size(); i < end; ++i) {
            if (i < part_sb->setting<int>(MS::PlatformAdhesion::kSkirtLayers))
                LayerAdditions::addSkirt(steps[i].dynamicCast<Layer>());
        }
    }
}

void PlanarSlicer::processThermalScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
    if (part_sb->setting<bool>(PS::ThermalScanner::kThermalScanner)) {
        int first_layer = 0;

        // If bed scan is enabled for laser scan, the first layer for the thermal scan is layer 1
        if (part_sb->setting<bool>(PS::LaserScanner::kLaserScanner) &&
            part_sb->setting<bool>(PS::LaserScanner::kEnableBedScan))
            first_layer = 1;

        int total_layers = part->countStepPairs();
        for (int current_layer = first_layer; current_layer < total_layers; ++current_layer) {
            auto layer = part->step(current_layer, StepType::kLayer).dynamicCast<Layer>();
            if (!layer.isNull() && !layer->getIslands(IslandType::kPolymer).isEmpty())
                LayerAdditions::addThermalScan(layer);
        }
    }
}

void PlanarSlicer::processLaserScan(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb) {
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

                auto currentLayer = part->step(current_layer, StepType::kLayer).dynamicCast<Layer>();
                const bool has_model =
                    !currentLayer.isNull() && !currentLayer->getIslands(IslandType::kPolymer).isEmpty();
                if (!has_model || scan_layer_skip <= 0 || (current_layer - 1) % scan_layer_skip != 0)
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

void PlanarSlicer::processGlobalLayers(QVector<QSharedPointer<Part>> parts,
                                       const QSharedPointer<SettingsBase>& settings) {
    if (anythingDirty()) {
        // create global layers from all the part layers
        m_global_layers = LayerOrderOptimizer::populateSteps(settings, parts);
    }
}

bool PlanarSlicer::anythingDirty() {
    bool anything_dirty = false;
    for (QSharedPointer<Part> curr_part : CSM->parts().values()) {
        if (curr_part->isPartDirty()) {
            anything_dirty = true;
            break;
        }
    }
    return anything_dirty;
}

void PlanarSlicer::processSupport(QSharedPointer<Part> part, int layer_count, int partStart) {
    if (layer_count < 2 || part->steps().isEmpty())
        return;

    QVector<QSharedPointer<Layer>> layers;
    QVector<PolygonList> model_geometry;
    layers.reserve(layer_count);
    model_geometry.reserve(layer_count);
    for (int i = 0; i < layer_count; ++i) {
        auto layer = part->step(partStart + i, StepType::kLayer).dynamicCast<Layer>();
        if (layer.isNull())
            return;
        layers.push_back(layer);
        model_geometry.push_back(layer->getGeometry());
    }

    auto removeSmallAreas = [](PolygonList geometry, Area minimum_area) {
        if (minimum_area <= 0)
            return geometry;

        PolygonList filtered;
        for (PolygonList island : geometry.splitIntoParts(true)) {
            if (island.netArea() >= minimum_area)
                filtered |= island;
        }
        return filtered;
    };

    struct OrganicBranch {
        int target_layer;
        Point contact;
        Point root;
        Distance diameter;
        Angle angle;
    };

    auto makeCircle = [](const Point& center, Distance radius) {
        Polygon circle;
        constexpr int kSegments = 20;
        circle.reserve(kSegments);
        for (int i = 0; i < kSegments; ++i) {
            const double theta = (2.0 * M_PI * i) / kSegments;
            circle.push_back(Point(center.x() + radius() * std::cos(theta), center.y() + radius() * std::sin(theta)));
        }
        PolygonList result;
        result += circle;
        return result;
    };

    auto makeRectangle = [](double minimum_x, double minimum_y, double maximum_x, double maximum_y) {
        Polygon rectangle;
        rectangle << Point(minimum_x, minimum_y) << Point(maximum_x, minimum_y)
                  << Point(maximum_x, maximum_y) << Point(minimum_x, maximum_y);
        PolygonList result;
        result += rectangle;
        return result;
    };

    auto isBridgeable = [&makeRectangle](PolygonList component, const PolygonList& model_below,
                                         Distance maximum_length, Distance anchor_width) {
        if (component.isEmpty() || model_below.isEmpty() || maximum_length <= 0)
            return false;

        const Point minimum = component.min();
        const Point maximum = component.max();
        const double width = maximum.x() - minimum.x();
        const double height = maximum.y() - minimum.y();
        const double anchor = qMax(anchor_width(), 1.0);

        auto intersectsModel = [&model_below](PolygonList band) {
            PolygonList model = model_below;
            return !(model & band).isEmpty();
        };

        if (Distance(width) <= maximum_length) {
            const PolygonList left = makeRectangle(minimum.x() - anchor, minimum.y() - anchor,
                                                   minimum.x() + anchor, maximum.y() + anchor);
            const PolygonList right = makeRectangle(maximum.x() - anchor, minimum.y() - anchor,
                                                    maximum.x() + anchor, maximum.y() + anchor);
            if (intersectsModel(left) && intersectsModel(right))
                return true;
        }

        if (Distance(height) <= maximum_length) {
            const PolygonList bottom = makeRectangle(minimum.x() - anchor, minimum.y() - anchor,
                                                     maximum.x() + anchor, minimum.y() + anchor);
            const PolygonList top = makeRectangle(minimum.x() - anchor, maximum.y() - anchor,
                                                  maximum.x() + anchor, maximum.y() + anchor);
            if (intersectsModel(bottom) && intersectsModel(top))
                return true;
        }
        return false;
    };

    QVector<PolygonList> support_geometry(layer_count);
    QVector<PolygonList> taper_hole_geometry(layer_count);
    QVector<PolygonList> taper_start_geometry(layer_count);
    QVector<PolygonList> interface_geometry(layer_count);
    QVector<PolygonList> base_geometry(layer_count);
    QVector<PolygonList> blocker_geometry(layer_count);
    QVector<PolygonList> enforcer_geometry(layer_count);
    QVector<OrganicBranch> organic_branches;

    // Settings meshes provide planar manual control.  A local support=false
    // region is a blocker; support=true is an enforcer at model contact areas.
    for (int layer_index = 0; layer_index < layer_count; ++layer_index) {
        for (const SettingsPolygon& polygon : layers[layer_index]->getSettingsPolygons()) {
            const auto settings = polygon.getSettings();
            if (!settings->contains(PS::Support::kEnable))
                continue;
            if (settings->setting<bool>(PS::Support::kEnable))
                enforcer_geometry[layer_index] |= polygon;
            else
                blocker_geometry[layer_index] |= polygon;
        }
    }

    // Find every surface that exceeds the configured self-supporting angle.
    // The demand is placed below the surface after the requested vertical gap.
    for (int upper_index = 1; upper_index < layer_count; ++upper_index) {
        const auto& upper_layer = layers[upper_index];
        if (!upper_layer->getSb()->setting<bool>(PS::Support::kEnable) || model_geometry[upper_index].isEmpty())
            continue;

        const double vertical_distance =
            upper_layer->getSlicingPlane().point().distance(layers[upper_index - 1]->getSlicingPlane().point())();
        const double threshold =
            qBound(0.0, upper_layer->getSb()->setting<Angle>(PS::Support::kThresholdAngle)(), (89.0 * deg)());
        const Distance self_supporting_offset(vertical_distance * std::tan(threshold));

        PolygonList supported_from_below = model_geometry[upper_index - 1];
        if (!supported_from_below.isEmpty() && self_supporting_offset > 0)
            supported_from_below = supported_from_below.offset(self_supporting_offset);

        PolygonList overhang = model_geometry[upper_index] - supported_from_below;
        const Area minimum_area = upper_layer->getSb()->setting<Area>(PS::Support::kMinArea);
        overhang = removeSmallAreas(overhang, minimum_area);

        const bool suppress_bridges = upper_layer->getSb()->setting<bool>(PS::Support::kBridgeSuppression);
        const Distance maximum_bridge_length =
            upper_layer->getSb()->setting<Distance>(PS::Support::kBridgeMaxLength);
        if (suppress_bridges && maximum_bridge_length > 0 && !overhang.isEmpty()) {
            PolygonList unsupported_overhang;
            const Distance anchor_width = upper_layer->getSb()->setting<Distance>(PS::Layer::kBeadWidth);
            for (PolygonList component : overhang.splitIntoParts(true)) {
                if (!isBridgeable(component, model_geometry[upper_index - 1], maximum_bridge_length, anchor_width))
                    unsupported_overhang |= component;
            }
            overhang = unsupported_overhang;
        }

        PolygonList forced_contact = model_geometry[upper_index] & enforcer_geometry[upper_index];
        overhang |= forced_contact;

        const int layer_offset = qMax(0, upper_layer->getSb()->setting<int>(PS::Support::kLayerOffset));
        const int target_layer = upper_index - layer_offset - 1;
        if (target_layer < 0)
            continue;

        overhang -= blocker_geometry[target_layer];
        if (overhang.isEmpty())
            continue;

        support_geometry[target_layer] |= overhang;

        // Record only the layers immediately below this contact as interface
        // layers.  The masks are clipped against the final support geometry
        // after tapering and collision avoidance have been applied.
        const int interface_layers = qMax(0, upper_layer->getSb()->setting<int>(PS::Support::kInterfaceLayers));
        const Distance interface_expansion =
            max(Distance(0), upper_layer->getSb()->setting<Distance>(PS::Support::kInterfaceExpansion));
        const PolygonList interface_contact =
            interface_expansion > 0 ? overhang.offset(interface_expansion) : overhang;
        for (int depth = 0; depth < interface_layers && target_layer - depth >= 0; ++depth)
            interface_geometry[target_layer - depth] |= interface_contact;

        // Anchor the taper directly below the dense interface.  With no
        // interface configured, the taper begins at the top support layer.
        const int taper_start_layer = target_layer - interface_layers;
        if (taper_start_layer >= 0)
            taper_start_geometry[taper_start_layer] |= interface_contact;

        if (upper_layer->getSb()->setting<int>(PS::Support::kStructure) == 1) {
            Distance diameter = upper_layer->getSb()->setting<Distance>(PS::Support::kOrganicBranchDiameter);
            const Distance bead_width = upper_layer->getSb()->setting<Distance>(PS::Layer::kBeadWidth);
            if (diameter <= 0)
                diameter = bead_width * 3.0;

            Distance spacing = upper_layer->getSb()->setting<Distance>(PS::Support::kOrganicBranchSpacing);
            if (spacing <= 0)
                spacing = max(diameter * 4.0, upper_layer->getSb()->setting<Distance>(PS::Support::kLineSpacing) * 4.0);
            if (spacing <= 0)
                spacing = diameter * 2.0;

            Angle branch_angle = upper_layer->getSb()->setting<Angle>(PS::Support::kOrganicBranchAngle);
            if (branch_angle <= 0)
                branch_angle = 25.0 * deg;

            for (PolygonList contact_area : overhang.splitIntoParts(true)) {
                Point root = contact_area.boundingRectCenter();
                if (!contact_area.inside(root, true))
                    root = contact_area.first().first();

                QVector<Point> contacts;
                const Point minimum = contact_area.min();
                const Point maximum = contact_area.max();
                for (double x = minimum.x() + spacing() / 2.0; x < maximum.x(); x += spacing()) {
                    for (double y = minimum.y() + spacing() / 2.0; y < maximum.y(); y += spacing()) {
                        Point candidate(x, y);
                        if (contact_area.inside(candidate, true))
                            contacts.push_back(candidate);
                    }
                }
                if (contacts.isEmpty())
                    contacts.push_back(root);

                for (const Point& contact : contacts)
                    organic_branches.push_back({target_layer, contact, root, diameter, branch_angle});
            }
        }
    }

    // Carry each demand down until it lands on model geometry or reaches the
    // build plate.  The outer envelope remains vertical for stability.  The
    // tapered interior is calculated separately after collision avoidance so
    // its material can grow inward from the supported outer tube wall.
    for (int layer_index = layer_count - 2; layer_index >= 0; --layer_index) {
        if (!support_geometry[layer_index + 1].isEmpty())
            support_geometry[layer_index] |= support_geometry[layer_index + 1];

        if (!layers[layer_index]->getSb()->setting<bool>(PS::Support::kEnable)) {
            support_geometry[layer_index].clear();
            continue;
        }

        const Distance xy_distance =
            max(Distance(0), layers[layer_index]->getSb()->setting<Distance>(PS::Support::kXYDistance));
        if (!model_geometry[layer_index].isEmpty()) {
            PolygonList clearance = model_geometry[layer_index];
            if (xy_distance > 0)
                clearance = clearance.offset(xy_distance);
            support_geometry[layer_index] -= clearance;
        }
    }

    // Organic mode replaces the column envelope with round branches that
    // converge toward a shared trunk while remaining inside the collision-free
    // support envelope calculated above.
    const bool organic = !organic_branches.isEmpty();
    if (organic) {
        QVector<PolygonList> organic_geometry(layer_count);
        for (int layer_index = 0; layer_index < layer_count; ++layer_index) {
            for (const OrganicBranch& branch : organic_branches) {
                if (layer_index > branch.target_layer || support_geometry[layer_index].isEmpty())
                    continue;

                const double vertical_distance = layers[branch.target_layer]->getSlicingPlane().point().distance(
                    layers[layer_index]->getSlicingPlane().point())();
                const double dx = branch.root.x() - branch.contact.x();
                const double dy = branch.root.y() - branch.contact.y();
                const double distance_to_root = std::hypot(dx, dy);
                const double max_shift = vertical_distance * std::tan(qBound(0.0, branch.angle(), (60.0 * deg)()));
                const double shift = qMin(distance_to_root, max_shift);
                const double ratio = distance_to_root > 0.0 ? shift / distance_to_root : 0.0;
                Point center(branch.contact.x() + dx * ratio, branch.contact.y() + dy * ratio);

                // Branches widen gently toward the bed and naturally merge when
                // their circular cross-sections overlap.
                const Distance radius =
                    branch.diameter / 2.0 + Distance(vertical_distance * std::tan(branch.angle() * 0.2));
                organic_geometry[layer_index] |= makeCircle(center, radius);
            }

            if (!organic_geometry[layer_index].isEmpty()) {
                const Distance envelope_margin =
                    max(layers[layer_index]->getSb()->setting<Distance>(PS::Support::kOrganicBranchDiameter),
                        layers[layer_index]->getSb()->setting<Distance>(PS::Layer::kBeadWidth) * 3.0);
                PolygonList envelope = support_geometry[layer_index].offset(envelope_margin);
                organic_geometry[layer_index] &= envelope;
            }
        }
        support_geometry = organic_geometry;
    }

    // Dense interfaces may extend a short distance beyond the sparse support
    // envelope.  Clip that expansion against blockers and model clearance, but
    // do not carry it down the full support column.
    for (int layer_index = 0; layer_index < layer_count; ++layer_index) {
        support_geometry[layer_index] |= interface_geometry[layer_index];

        PolygonList forbidden = blocker_geometry[layer_index];
        const Distance xy_distance =
            max(Distance(0), layers[layer_index]->getSb()->setting<Distance>(PS::Support::kXYDistance));
        if (!model_geometry[layer_index].isEmpty()) {
            PolygonList model_clearance = model_geometry[layer_index];
            if (xy_distance > 0)
                model_clearance = model_clearance.offset(xy_distance);
            forbidden |= model_clearance;
        }
        support_geometry[layer_index] -= forbidden;
        interface_geometry[layer_index] -= forbidden;
    }

    // Add an optional dense, expanded foot to support structures that reach
    // the build plate.  The footprint remains constant through the configured
    // base layers and is clipped with the same collision rules as support.
    const int base_layers = qMax(0, layers[0]->getSb()->setting<int>(PS::Support::kBaseLayers));
    if (base_layers > 0 && !support_geometry[0].isEmpty()) {
        const Distance base_expansion =
            max(Distance(0), layers[0]->getSb()->setting<Distance>(PS::Support::kBaseExpansion));
        PolygonList base_footprint = support_geometry[0];
        if (base_expansion > 0)
            base_footprint = base_footprint.offset(base_expansion);

        for (int layer_index = 0; layer_index < qMin(layer_count, base_layers); ++layer_index) {
            PolygonList layer_base = base_footprint;
            layer_base -= blocker_geometry[layer_index];

            if (!model_geometry[layer_index].isEmpty()) {
                const Distance xy_distance =
                    max(Distance(0), layers[layer_index]->getSb()->setting<Distance>(PS::Support::kXYDistance));
                PolygonList model_clearance = model_geometry[layer_index];
                if (xy_distance > 0)
                    model_clearance = model_clearance.offset(xy_distance);
                layer_base -= model_clearance;
            }

            support_geometry[layer_index] |= layer_base;
            base_geometry[layer_index] |= layer_base;
        }
    }

    // Validate support from the build plate upward.  In Everywhere mode a
    // component may also land on model geometry; Build Plate Only deliberately
    // excludes that foundation.  Invalid components are removed before path
    // generation so disconnected islands cannot silently print in midair.
    const int placement = layers[0]->getSb()->setting<int>(PS::Support::kPlacement);
    const bool build_plate_only = placement == 1;
    const bool validate_support = layers[0]->getSb()->setting<bool>(PS::Support::kValidation);
    if (validate_support || build_plate_only) {
        const double minimum_overlap =
            qBound(0.0, layers[0]->getSb()->setting<double>(PS::Support::kValidationMinOverlap) / 100.0, 1.0);
        const Area minimum_base_area =
            validate_support ? layers[0]->getSb()->setting<Area>(PS::Support::kValidationMinBaseArea) : Area(0);
        QVector<PolygonList> valid_support(layer_count);
        int rejected_components = 0;

        for (PolygonList component : support_geometry[0].splitIntoParts(true)) {
            if (minimum_base_area > 0 && component.netArea() < minimum_base_area) {
                ++rejected_components;
                continue;
            }
            valid_support[0] |= component;
        }

        for (int layer_index = 1; layer_index < layer_count; ++layer_index) {
            PolygonList foundation = valid_support[layer_index - 1];
            if (!build_plate_only)
                foundation |= model_geometry[layer_index - 1];

            for (PolygonList component : support_geometry[layer_index].splitIntoParts(true)) {
                PolygonList overlap_source = component;
                PolygonList overlap = overlap_source & foundation;
                const double component_area = component.netArea()();
                const double overlap_ratio =
                    component_area > 0.0 ? qMax(0.0, overlap.netArea()() / component_area) : 0.0;
                if (overlap.isEmpty() || overlap_ratio < minimum_overlap) {
                    ++rejected_components;
                    continue;
                }
                valid_support[layer_index] |= component;
            }
        }

        support_geometry = valid_support;
        if (rejected_components > 0 && validate_support)
            qWarning() << "Support validation removed" << rejected_components
                       << "disconnected or insufficiently supported components";
    }

    auto makeTaperSeed = [](const PolygonList& geometry, Distance growth) {
        PolygonList seeds;
        if (geometry.isEmpty() || growth <= 0)
            return seeds;

        // Erode each component to its innermost region, then back off by one
        // layer of taper growth.  This creates a small central void immediately
        // below the interface without assuming that the support is convex or
        // that its bounding-box center lies inside it.
        for (PolygonList component : geometry.splitIntoParts(true)) {
            const Point minimum = component.min();
            const Point maximum = component.max();
            double lower_inset = 0.0;
            double upper_inset = std::hypot(maximum.x() - minimum.x(), maximum.y() - minimum.y());

            for (int iteration = 0; iteration < 32; ++iteration) {
                const double inset = (lower_inset + upper_inset) / 2.0;
                if (component.offset(-Distance(inset)).isEmpty())
                    upper_inset = inset;
                else
                    lower_inset = inset;
            }

            const Distance seed_inset(qMax(0.0, lower_inset - growth()));
            seeds |= component.offset(-seed_inset);
        }
        return seeds;
    };

    // A conventional tapered support is represented by its empty interior,
    // not by a detached center core.  Start with a small void immediately below
    // each interface and expand it while moving downward.  Once the void
    // reaches the minimum tube wall it remains at that size, keeping long lower
    // sections hollow instead of finishing the taper early and printing solid.
    if (!organic) {
        for (int layer_index = layer_count - 1; layer_index >= 0; --layer_index) {
            const auto& settings = layers[layer_index]->getSb();
            const bool hollow_taper = settings->setting<bool>(PS::Support::kTaper) &&
                                      settings->setting<int>(PS::Support::kStructure) == 0;
            if (!hollow_taper || support_geometry[layer_index].isEmpty())
                continue;

            const Distance bead_width = settings->setting<Distance>(PS::Layer::kBeadWidth);
            const int wall_contours = qMax(1, settings->setting<int>(PS::Support::kTaperWallContours));
            const Distance wall_width = bead_width * wall_contours;
            const PolygonList maximum_hole = support_geometry[layer_index].offset(-wall_width);
            if (maximum_hole.isEmpty())
                continue;

            PolygonList allowed_hole = maximum_hole;
            PolygonList dense_mask = interface_geometry[layer_index];
            dense_mask |= base_geometry[layer_index];
            allowed_hole -= support_geometry[layer_index] & dense_mask;
            if (allowed_hole.isEmpty())
                continue;

            Distance taper_step(0);
            if (layer_index + 1 < layer_count) {
                const double vertical_distance = layers[layer_index + 1]->getSlicingPlane().point().distance(
                    layers[layer_index]->getSlicingPlane().point())();
                const double taper_angle =
                    qBound(0.0, settings->setting<Angle>(PS::Support::kTaperAngle)(), (89.0 * deg)());
                taper_step = Distance(vertical_distance * std::tan(taper_angle));
            }

            PolygonList holes;
            if (layer_index + 1 < layer_count && !taper_hole_geometry[layer_index + 1].isEmpty()) {
                PolygonList propagated_holes = taper_hole_geometry[layer_index + 1];
                if (taper_step > 0)
                    propagated_holes = propagated_holes.offset(taper_step);
                propagated_holes &= allowed_hole;
                holes |= propagated_holes;
            }

            PolygonList taper_starts = taper_start_geometry[layer_index];
            taper_starts &= allowed_hole;
            holes |= makeTaperSeed(taper_starts, taper_step);
            holes &= allowed_hole;
            taper_hole_geometry[layer_index] = holes;
        }
    }

    for (int layer_index = 0; layer_index < layer_count; ++layer_index) {
        QVector<QSharedPointer<IslandBase>> support_islands;
        const PolygonList interface_dense_geometry = support_geometry[layer_index] & interface_geometry[layer_index];
        PolygonList base_dense_geometry = support_geometry[layer_index] & base_geometry[layer_index];
        base_dense_geometry -= interface_dense_geometry;
        PolygonList dense_geometry = interface_dense_geometry;
        dense_geometry |= base_dense_geometry;
        PolygonList tube_wall_geometry;
        PolygonList sparse_geometry;

        const bool hollow_taper = !organic && layers[layer_index]->getSb()->setting<bool>(PS::Support::kTaper) &&
                                  layers[layer_index]->getSb()->setting<int>(PS::Support::kStructure) == 0;
        if (hollow_taper) {
            const int wall_contours =
                qMax(1, layers[layer_index]->getSb()->setting<int>(PS::Support::kTaperWallContours));
            const Distance wall_width =
                layers[layer_index]->getSb()->setting<Distance>(PS::Layer::kBeadWidth) * wall_contours;
            const PolygonList tube_interior = support_geometry[layer_index].offset(-wall_width);

            tube_wall_geometry = support_geometry[layer_index] - tube_interior;
            tube_wall_geometry -= dense_geometry;

            sparse_geometry = support_geometry[layer_index] - taper_hole_geometry[layer_index];
            sparse_geometry -= tube_wall_geometry;
            sparse_geometry -= dense_geometry;
        }
        else {
            sparse_geometry = support_geometry[layer_index] - dense_geometry;
        }

        for (const PolygonList& geometry : sparse_geometry.splitIntoParts(true)) {
            auto settings = QSharedPointer<SettingsBase>::create(*layers[layer_index]->getSb());
            settings->setSetting(PS::Support::kInterfaceRegion, false);
            settings->setSetting(PS::Support::kBaseRegion, false);
            settings->setSetting(PS::Support::kTubeWallRegion, false);
            support_islands.push_back(
                QSharedPointer<SupportIsland>::create(geometry, settings, layers[layer_index]->getSettingsPolygons()));
        }

        for (const PolygonList& geometry : tube_wall_geometry.splitIntoParts(true)) {
            auto settings = QSharedPointer<SettingsBase>::create(*layers[layer_index]->getSb());
            settings->setSetting(PS::Support::kInterfaceRegion, false);
            settings->setSetting(PS::Support::kBaseRegion, false);
            settings->setSetting(PS::Support::kTubeWallRegion, true);
            settings->setSetting(PS::Support::kMinInfillArea, Area(0));
            support_islands.push_back(
                QSharedPointer<SupportIsland>::create(geometry, settings, layers[layer_index]->getSettingsPolygons()));
        }

        for (const PolygonList& geometry : base_dense_geometry.splitIntoParts(true)) {
            auto settings = QSharedPointer<SettingsBase>::create(*layers[layer_index]->getSb());
            settings->setSetting(PS::Support::kInterfaceRegion, false);
            settings->setSetting(PS::Support::kBaseRegion, true);
            settings->setSetting(PS::Support::kTubeWallRegion, false);
            settings->setSetting(PS::Support::kPattern, static_cast<int>(InfillPatterns::kLines));
            settings->setSetting(PS::Support::kLineSpacing,
                                 settings->setting<Distance>(PS::Layer::kBeadWidth));
            settings->setSetting(PS::Support::kMinInfillArea, Area(0));
            support_islands.push_back(
                QSharedPointer<SupportIsland>::create(geometry, settings, layers[layer_index]->getSettingsPolygons()));
        }

        for (const PolygonList& geometry : interface_dense_geometry.splitIntoParts(true)) {
            auto settings = QSharedPointer<SettingsBase>::create(*layers[layer_index]->getSb());
            Distance interface_spacing = settings->setting<Distance>(PS::Support::kInterfaceLineSpacing);
            if (interface_spacing <= 0)
                interface_spacing = settings->setting<Distance>(PS::Layer::kBeadWidth);

            settings->setSetting(PS::Support::kInterfaceRegion, true);
            settings->setSetting(PS::Support::kBaseRegion, false);
            settings->setSetting(PS::Support::kTubeWallRegion, false);
            settings->setSetting(PS::Support::kPattern, static_cast<int>(InfillPatterns::kLines));
            settings->setSetting(PS::Support::kLineSpacing, interface_spacing);
            settings->setSetting(PS::Support::kMinInfillArea, Area(0));
            support_islands.push_back(
                QSharedPointer<SupportIsland>::create(geometry, settings, layers[layer_index]->getSettingsPolygons()));
        }
        layers[layer_index]->updateIslands(IslandType::kSupport, support_islands);
    }
}

void PlanarSlicer::postProcess(nlohmann::json opt_data) {
    if (anythingDirty()) {
        QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
        global_sb->makeGlobalAdjustments();

        // set up the start point, first region index, and previous region list
        // used by island and path order optimizer to generate travels
        Point current_point(0, 0, 0);
        int start_index = -1;
        QVector<QSharedPointer<RegionBase>> previous_regions;

        for (int g_layer_num = 0, max_layers = m_global_layers.size(); g_layer_num < max_layers; ++g_layer_num) {
            m_global_layers[g_layer_num]->unorient();

            // current_point, start_index, & previous_regions are updated during method execution
            // so that each layer starts where the last layer ended
            m_global_layers[g_layer_num]->connectPaths(global_sb, current_point, start_index, previous_regions);

            m_global_layers[g_layer_num]->calculateModifiers(global_sb, current_point, g_layer_num);

            m_global_layers[g_layer_num]->reorient();

            // update status in UI
            emit statusUpdate(StatusUpdateStepType::kPostProcess,
                              qRound(static_cast<double>(g_layer_num + 1) / static_cast<double>(max_layers) * 100.0));
        }
    }
    else {
        emit statusUpdate(StatusUpdateStepType::kPostProcess, 100); // Mark layerbar as done
    }
}

void PlanarSlicer::writeGCode() {
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
