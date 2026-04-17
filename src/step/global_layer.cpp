#include "step/global_layer.h"

#include <limits>

#include <qassert.h>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qmap.h>
#include <qsharedpointer.h>
#include <quuid.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/point.h"
#include "optimizers/island_order_optimizer.h"
#include "part/part.h"
#include "step/layer/layer.h"
#include "step/layer/regions/region_base.h"
#include "step/layer/scan_layer.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {

GlobalLayer::GlobalLayer(int layer_number) {
    // make a new empty list for the step groups
    m_step_pairs = QMap<QUuid, QSharedPointer<Part::StepPair>>();
    m_layer_number = layer_number;
}

void GlobalLayer::unorient() {
    QMap<QUuid, QSharedPointer<Part::StepPair>>::ConstIterator itr;
    for (itr = m_step_pairs.constBegin(); itr != m_step_pairs.constEnd(); ++itr) {
        auto step_pair = itr.value();
        step_pair->printing_layer->unorient();
    }
}

void GlobalLayer::reorient() {
    QMap<QUuid, QSharedPointer<Part::StepPair>>::ConstIterator itr;
    for (itr = m_step_pairs.constBegin(); itr != m_step_pairs.constEnd(); ++itr) {
        auto step_pair = itr.value();
        step_pair->printing_layer->reorient();
    }
}

void GlobalLayer::calculateModifiers(QSharedPointer<SettingsBase> global_sb, QVector<Point>& current_location,
                                     int layer_num) {
    // Retrieve relevant settings
    int num_nozzles = global_sb->setting<int>(ES::MultiNozzle::kNozzleCount);

    for (int tool = 0; tool < num_nozzles; ++tool) {
        if (!m_island_order[tool].isEmpty()) {
            QSharedPointer<IslandBase> firstIsland = m_island_order[tool].front();

            // Retrieve relevant settings
            bool perimeter_enabled = firstIsland->getSb()->setting<bool>(PS::Perimeter::kEnable);
            bool perimeter_lead_in_enabled = firstIsland->getSb()->setting<bool>(PS::Perimeter::kEnableLeadIn);
            bool perimeter_lead_in_first_layer_only = global_sb->setting<bool>(PS::Perimeter::kLeadInFirstLayerOnly);

            if (perimeter_enabled && perimeter_lead_in_enabled &&
                (layer_num == 0 || !perimeter_lead_in_first_layer_only)) {
                Point leadIn = Point(firstIsland->getSb()->setting<Distance>(PS::Perimeter::kEnableLeadInX),
                                     firstIsland->getSb()->setting<Distance>(PS::Perimeter::kEnableLeadInY), 0.0);

                Q_ASSERT(firstIsland->getType() == IslandType::kPolymer);
                QSharedPointer<RegionBase> firstRegion = (firstIsland->getRegions()).front();

                PathModifierGenerator::GenerateLayerLeadIn(firstRegion->getPaths().front(), leadIn, global_sb);
            }
        }
    }

    if (global_sb->setting<bool>(MS::SpiralLift::kLayerEnable)) {
        for (int tool = 0; tool < num_nozzles; ++tool) {
            auto lastIsland = m_island_order[tool].back();
            Q_ASSERT(lastIsland->getType() == IslandType::kPolymer);
            QList<QSharedPointer<RegionBase>> regions = lastIsland->getRegions();
            QSharedPointer<RegionBase> lastRegion = regions.back();
            Path finalPath = lastRegion->getPaths().back();

            if (finalPath.back()->getSb()->setting<PathModifiers>(SS::kPathModifiers) != PathModifiers::kSpiralLift) {
                PathModifierGenerator::GenerateSpiralLift(finalPath,
                                                          global_sb->setting<Distance>(MS::SpiralLift::kLiftRadius),
                                                          global_sb->setting<Distance>(MS::SpiralLift::kLiftHeight),
                                                          global_sb->setting<int>(MS::SpiralLift::kLiftPoints),
                                                          global_sb->setting<Velocity>(MS::SpiralLift::kLiftSpeed),
                                                          global_sb->setting<bool>(PRS::MachineSetup::kSupportG3));

                // move current location to the end of the spiral lift
                current_location[tool] = getIslands().last()->getRegions().last()->getPaths().last().back()->end();
            }
        }
    }
}

void GlobalLayer::adjustFixedMultiNozzle() {
    QMap<QUuid, QSharedPointer<Part::StepPair>>::ConstIterator itr;
    for (itr = m_step_pairs.constBegin(); itr != m_step_pairs.constEnd(); ++itr) {
        auto step_pair = itr.value();
        step_pair->printing_layer->adjustMultiNozzle();
    }
}

void GlobalLayer::connectPaths(QSharedPointer<SettingsBase> global_sb, QVector<Point>& start, QVector<int>& start_index,
                               QVector<QVector<QSharedPointer<RegionBase>>>& previous_regions) {
    // this function "connects" paths by inserting travels between disconnected pathing. Also orders parts & islands.
    // Does this individually for each nozzle.

    // all the params should be the same length
    // and that length is the number of nozzles
    int num_nozzles = global_sb->setting<int>(ES::MultiNozzle::kNozzleCount);
    Q_ASSERT(start.size() == num_nozzles);
    Q_ASSERT(start_index.size() == num_nozzles);
    Q_ASSERT(previous_regions.size() == num_nozzles);

    // get the island order method from the settings
    IslandOrderOptimization islandOrderMethod =
        static_cast<IslandOrderOptimization>(global_sb->setting<int>(PS::Optimizations::kIslandOrder));

    // 1) Connect paths for all the scans first.
    //    Assume all the scans go on tool #0.
    if (containsScanLayers()) {
        // 1.1) Order the scan layers.
        //      Scan layers have a raster pattern that covers the entire part. So,
        //      use the island order optimizers "part order" mode (mode #2) to determine
        //      scan layer order.

        // 1.1.1) Get a list of all the islands and the parts they belong to
        QHash<QSharedPointer<IslandBase>, QUuid> islands_for_all_parts;
        for (auto i = m_step_pairs.constBegin(); i != m_step_pairs.constEnd(); ++i) {
            QUuid part_id = i.key();
            QSharedPointer<Part::StepPair> step_pair = i.value();

            auto part_islands = step_pair->printing_layer->getIslands().toVector();
            for (auto island : part_islands) {
                islands_for_all_parts.insert(island, part_id);
            }
        }

        // 1.1.2) Get the right start point for the Island Order Optimizer part ordering
        Point start_point = start[0]; // always 0th tool for scans
        if (islandOrderMethod == IslandOrderOptimization::kCustomPoint) {
            //! \note  Calls to CustomIsland location may need to vary by nozzle/tool number
            start_point = Point(global_sb->setting<double>(PS::Optimizations::kCustomIslandXLocation),
                                global_sb->setting<double>(PS::Optimizations::kCustomIslandYLocation));
        }

        // 1.1.3) Make the IOO and get the order results
        IslandBaseOrderOptimizer part_oo(start_point, islands_for_all_parts, islandOrderMethod); // mode 2
        m_part_order = part_oo.computePartOrder();

        // 1.2) Connect the scans in determined order
        bool is_first_scan = true;
        for (auto part : m_part_order) {
            QSharedPointer<Part::StepPair> step_pair = m_step_pairs[part];
            if (step_pair->scan_layer != nullptr) {
                if (is_first_scan) {
                    step_pair->scan_layer->setFirst();
                    is_first_scan = false;
                }

                step_pair->scan_layer->unorient();
                step_pair->scan_layer->connectPaths(start[0], start_index[0],
                                                    previous_regions[0]); // always 0th tool for scans

                step_pair->scan_layer->reorient();
            }
        }
    }

    // 2) For each tool
    //       Get a list of all the printed islands for this tool
    //       Order the islands
    //       Connect the islands
    m_island_order.clear();
    m_island_order.reserve(num_nozzles);
    for (int tool = 0; tool < num_nozzles; ++tool) {
        // clear the resulting island order (in case of re-slice)
        // or make a new list to store reuslts
        if (m_island_order.size() > tool)
            m_island_order[tool].clear();
        else
            m_island_order.push_back(QList<QSharedPointer<IslandBase>>());

        // get a list of all the islands that this tool needs to print
        QMultiMap<int, QSharedPointer<IslandBase>> islands_for_current_tool;
        for (auto island : getIslands()) {
            if (island->getExtruder() == tool)
                islands_for_current_tool.insert(static_cast<int>(island->getType()), island);
        }

        // make an optimizer to do the ordering
        IslandBaseOrderOptimizer island_optimizer(start[tool], islands_for_current_tool.values(), start_index[tool],
                                                  islandOrderMethod); // mode 1 - ordering islands

        // Do seam adjustment if necessary
        if (islandOrderMethod == IslandOrderOptimization::kCustomPoint) {
            Point start_override(global_sb->setting<double>(PS::Optimizations::kCustomIslandXLocation),
                                 global_sb->setting<double>(PS::Optimizations::kCustomIslandYLocation));

            island_optimizer.setStartPoint(start_override);
        }

        QList<QSharedPointer<IslandBase>> wire_feed_islands =
            islands_for_current_tool.values(static_cast<int>(IslandType::kWireFeed));

        if (wire_feed_islands.size() > 0) {
            island_optimizer.setIslands(wire_feed_islands);
            while (wire_feed_islands.size() > 0) {
                int index = island_optimizer.computeNextIndex();
                QSharedPointer<IslandBase> isl = wire_feed_islands[index];
                wire_feed_islands.removeAt(index);
                isl->optimize(m_layer_number, start[tool], previous_regions[tool]);
                m_island_order[tool].push_back(isl);
                island_optimizer.setStartPoint(start[tool]);
            }
        }

        // Handle skirts
        QList<QSharedPointer<IslandBase>> skirt_islands;
        skirt_islands = islands_for_current_tool.values(static_cast<int>(IslandType::kSkirt));

        if (skirt_islands.size() > 0) {
            m_island_order[tool].push_back(skirt_islands[0]);
            m_island_order[tool].last()->optimize(m_layer_number, start[tool], previous_regions[tool]);
        }

        QList<QList<QSharedPointer<IslandBase>>> ordered_islands_to_process;
        QList<QSharedPointer<IslandBase>> brim_islands;
        if (islands_for_current_tool.values(static_cast<int>(IslandType::kBrim)).size() > 0)
            brim_islands = islands_for_current_tool.values(static_cast<int>(IslandType::kBrim));

        // mutually exclusive, either a layer has rafts or build/support
        // rafts are global settings, so this is still true for global layers
        if (islands_for_current_tool.values(static_cast<int>(IslandType::kRaft)).size() > 0) {
            ordered_islands_to_process.push_back(islands_for_current_tool.values(static_cast<int>(IslandType::kRaft)));
        }
        else {
            // check setting, if first, get supports then actual build islands, else the opposite order
            if (global_sb->setting<bool>(PS::Support::kPrintFirst)) {
                if (islands_for_current_tool.values(static_cast<int>(IslandType::kSupport)).size() > 0)
                    ordered_islands_to_process.push_back(
                        islands_for_current_tool.values(static_cast<int>(IslandType::kSupport)));

                if (islands_for_current_tool.values(static_cast<int>(IslandType::kPolymer)).size() > 0)
                    ordered_islands_to_process.push_back(
                        islands_for_current_tool.values(static_cast<int>(IslandType::kPolymer)));
            }
            else {
                if (islands_for_current_tool.values(static_cast<int>(IslandType::kPolymer)).size() > 0)
                    ordered_islands_to_process.push_back(
                        islands_for_current_tool.values(static_cast<int>(IslandType::kPolymer)));

                if (islands_for_current_tool.values(static_cast<int>(IslandType::kSupport)).size() > 0)
                    ordered_islands_to_process.push_back(
                        islands_for_current_tool.values(static_cast<int>(IslandType::kSupport)));
            }
        }

        // create small tree-like structure of seqence between brims and contained islands (raft, support, polymer)
        QList<QMap<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>> impliedOrder =
            createSequence(brim_islands, ordered_islands_to_process);

        // if brims exist, travel to those first, then contained islands
        // otherwise, the order will simply be determined by all the islands in a particular precendence level and the
        // set optimization strategy ie. shortest distance to next polymer island
        QList<QSharedPointer<IslandBase>> visited_islands;
        for (QMap<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>> level : impliedOrder) {
            QList<QSharedPointer<IslandBase>> islandSet = level.keys();
            island_optimizer.setIslands(islandSet);
            while (islandSet.size() > 0) {
                int index = island_optimizer.computeNextIndex();
                QSharedPointer<IslandBase> currentIsland = islandSet[index];
                if (!visited_islands.contains(currentIsland)) {
                    currentIsland->optimize(m_layer_number, start[tool], previous_regions[tool]);
                    m_island_order[tool].push_back(currentIsland);
                    visited_islands.push_back(currentIsland);
                }
                if (level[currentIsland].size() > 0) {
                    QList<QSharedPointer<IslandBase>> childrenSet = level[currentIsland];
                    island_optimizer.setIslands(childrenSet);
                    while (childrenSet.size() > 0) {
                        int index = island_optimizer.computeNextIndex();
                        QSharedPointer<IslandBase> currentIsland = childrenSet[index];
                        currentIsland->optimize(m_layer_number, start[tool], previous_regions[tool]);
                        m_island_order[tool].push_back(currentIsland);
                        childrenSet.removeAt(index);
                    }
                }
                islandSet.removeAt(index);
            }
        }

        // Handle the thermal scan islands
        QList<QSharedPointer<IslandBase>> thermal_scan_islands =
            islands_for_current_tool.values(static_cast<int>(IslandType::kThermalScan));
        if (thermal_scan_islands.size() > 0) {
            island_optimizer.setIslands(thermal_scan_islands);
            while (thermal_scan_islands.size() > 0) {
                int index = island_optimizer.computeNextIndex();
                QSharedPointer<IslandBase> isl = thermal_scan_islands[index];
                thermal_scan_islands.removeAt(index);
                isl->optimize(m_layer_number, start[tool], previous_regions[tool]);
                m_island_order[tool].push_back(isl);
            }
        }

        if (islandOrderMethod == IslandOrderOptimization::kLeastRecentlyVisited)
            start_index[tool] = island_optimizer.getFirstIndexSelected();

        for (QSharedPointer<IslandBase> island : islands_for_current_tool)
            island->markRegionStartSegment();

    } // end of for each tool/nozzle
}

QList<QMap<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>>
GlobalLayer::createSequence(QList<QSharedPointer<IslandBase>> parent,
                            QList<QList<QSharedPointer<IslandBase>>> children) {
    QList<QMap<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>> result;
    int children_size = children.size();
    result.reserve(children_size);
    for (int i = 0; i < children_size; ++i)
        result.append(QMap<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>());
    if (parent.size() == 0) {
        for (int i = 0; i < children_size; ++i) {
            QList<QSharedPointer<IslandBase>> islandSet = children[i];
            for (QSharedPointer<IslandBase> isl : islandSet)
                result[i].insert(isl, QList<QSharedPointer<IslandBase>>());
        }
    }
    else {
        for (QSharedPointer<IslandBase> brim : parent) {
            for (int i = 0; i < children_size; ++i) {
                QList<QSharedPointer<IslandBase>> islandSet = children[i];
                for (QSharedPointer<IslandBase> isl : islandSet) {
                    if (brim->getGeometry().first().inside(isl->getGeometry().first().first())) {
                        result[i][brim].append(isl);
                    }
                }
            }
        }
    }
    return result;
}

bool GlobalLayer::containsScanLayers() {
    QMap<QUuid, QSharedPointer<Part::StepPair>>::ConstIterator itr;
    for (itr = m_step_pairs.constBegin(); itr != m_step_pairs.constEnd(); ++itr) {
        auto step_pair = itr.value();
        if (step_pair->scan_layer != nullptr)
            return true;
    }
    return false;
}

QVector<QSharedPointer<IslandBase>> GlobalLayer::getIslands() {
    QVector<QSharedPointer<IslandBase>> layer_islands;
    QMap<QUuid, QSharedPointer<Part::StepPair>>::ConstIterator itr;
    for (itr = m_step_pairs.constBegin(); itr != m_step_pairs.constEnd(); ++itr) {
        auto step_pair = itr.value();
        auto part_islands = step_pair->printing_layer->getIslands().toVector();
        layer_islands.append(part_islands);
    }
    return layer_islands;
}

QString GlobalLayer::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;

    for (int tool = 0, end = m_island_order.size(); tool < end; ++tool) {
        // should output gcode if there are scan layers to output
        bool shouldOutputGcode = (tool == 0 && containsScanLayers());
        // should output gcode if there are valid non-scan paths
        for (QSharedPointer<IslandBase> island : m_island_order[tool]) {
            if (island->getAnyValidPaths()) {
                shouldOutputGcode = true;
                break;
            }
        }

        if (shouldOutputGcode) {
            gcode += writer->writeCommentLine("Nozzle " + QString::number(tool));
            if (tool == 0 && containsScanLayers()) {
                for (auto part : m_part_order) {
                    QSharedPointer<ScanLayer> scan_layer = m_step_pairs[part]->scan_layer;
                    if (scan_layer != nullptr)
                        gcode += scan_layer->writeGCode(writer);
                }
                gcode += writer->writeAfterAllScans();
            }
            for (QSharedPointer<IslandBase> island : m_island_order[tool]) {
                gcode += writer->writeBeforeIsland();
                gcode += island->writeGCode(writer);
                gcode += writer->writeAfterIsland();
            }
        }
        else {
            gcode += writer->writeEmptyStep();
        }
    }

    return gcode;
}

void GlobalLayer::setDirtyBit(bool dirty) {
    QMap<QUuid, QSharedPointer<Part::StepPair>>::ConstIterator itr;
    for (itr = m_step_pairs.constBegin(); itr != m_step_pairs.constEnd(); ++itr) {
        auto step_pair = itr.value();
        if (step_pair->printing_layer != nullptr)
            step_pair->printing_layer->setDirtyBit(dirty);

        if (step_pair->scan_layer != nullptr)
            step_pair->scan_layer->setDirtyBit(dirty);
    }
}

void GlobalLayer::addStepPair(QUuid part_id, Part::StepPair step_group) {
    QSharedPointer<Part::StepPair> ptr = QSharedPointer<Part::StepPair>::create(step_group);
    m_step_pairs.insert(part_id, ptr);
}

double GlobalLayer::getMinZ() {
    double min = std::numeric_limits<double>::max();
    QMap<QUuid, QSharedPointer<Part::StepPair>>::ConstIterator itr;
    for (itr = m_step_pairs.constBegin(); itr != m_step_pairs.constEnd(); ++itr) {
        auto step_pair = itr.value();
        if (step_pair->printing_layer != nullptr) {
            double step_min = step_pair->printing_layer->getMinZ();
            if (step_min < min)
                min = step_min;
        }
    }
    return min;
}

Distance GlobalLayer::getLayerHeight() {
    //! \note assumes all co-planar layers have the same height
    QSharedPointer<Layer> first_layer = m_step_pairs.first()->printing_layer;
    Q_ASSERT(first_layer != nullptr);
    return first_layer->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
}
} // namespace ORNL
