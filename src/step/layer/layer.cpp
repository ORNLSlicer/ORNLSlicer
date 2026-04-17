#include "step/layer/layer.h"

#include <cstdlib>
#include <limits>

#include <qcontainerfwd.h>
#include <qhash.h>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/path.h"
#include "geometry/path_modifier.h"
#include "geometry/point.h"
#include "geometry/settings_polygon.h"
#include "optimizers/island_order_optimizer.h"
#include "step/layer/island/polymer_island.h"
#include "step/layer/regions/region_base.h"
#include "step/step.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
Layer::Layer(uint layer_nr, const QSharedPointer<SettingsBase>& sb) : Step(sb), m_layer_nr(layer_nr) {
    m_type = StepType::kLayer;
}

QString Layer::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;

    bool shouldOutputGcode = false;
    for (QSharedPointer<IslandBase> island : m_island_order) {
        if (island->getAnyValidPaths()) {
            shouldOutputGcode = true;
            break;
        }
    }
    if (shouldOutputGcode) {
        for (QSharedPointer<IslandBase> island : m_island_order) {
            writer->writeBeforeIsland();
            gcode += island->writeGCode(writer);
            writer->writeAfterIsland();
        }
    }
    else {
        gcode += writer->writeEmptyStep();
    }

    return gcode;
}

uint Layer::getLayerNumber() const { return m_layer_nr; }

void Layer::compute() {
    for (QSharedPointer<IslandBase> island : m_islands) {
        island->compute(m_layer_nr, m_sync);

        QSharedPointer<PolymerIsland> polyIsland = island.dynamicCast<PolymerIsland>();
        if (polyIsland != nullptr) {
            polyIsland->reorderRegions();
        }
    }
}

void Layer::connectPaths(Point& start, int& start_index, QVector<QSharedPointer<RegionBase>>& previousRegions) {
    m_island_order.clear();

    // Optimize the layer.
    IslandOrderOptimization islandOrderOptimization =
        static_cast<IslandOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kIslandOrder));

    // start index = index of last visited island
    IslandBaseOrderOptimizer ioo(start, m_islands.values(), start_index, islandOrderOptimization); // mode 1

    // seam adjustment
    if (islandOrderOptimization == IslandOrderOptimization::kCustomPoint) {
        Point startOverride(getSb()->setting<double>(PS::Optimizations::kCustomIslandXLocation),
                            getSb()->setting<double>(PS::Optimizations::kCustomIslandYLocation));

        ioo.setStartPoint(startOverride);
    }

    QList<QSharedPointer<IslandBase>> currentIslands;
    currentIslands = m_islands.values(static_cast<int>(IslandType::kSkirt));

    // can only be 1 skirt
    if (currentIslands.size() > 0) {
        m_island_order.push_back(currentIslands[0]);
        m_island_order.last()->optimize(m_layer_nr, start, previousRegions);
    }

    QList<QList<QSharedPointer<IslandBase>>> islandsToProcess;
    QList<QSharedPointer<IslandBase>> allBrims;
    if (m_islands.values(static_cast<int>(IslandType::kBrim)).size() > 0) {
        allBrims = m_islands.values(static_cast<int>(IslandType::kBrim));
    }

    // mutually exclusive, either a layer has rafts or build/support
    if (m_islands.values(static_cast<int>(IslandType::kRaft)).size() > 0) {
        islandsToProcess.push_back(m_islands.values(static_cast<int>(IslandType::kRaft)));
    }
    else {
        // check setting, if first, get supports then actual build islands, else the opposite order
        if (this->getSb()->setting<bool>(PS::Support::kPrintFirst)) {
            if (m_islands.values(static_cast<int>(IslandType::kSupport)).size() > 0) {
                islandsToProcess.push_back(m_islands.values(static_cast<int>(IslandType::kSupport)));
            }
            if (m_islands.values(static_cast<int>(IslandType::kPolymer)).size() > 0) {
                islandsToProcess.push_back(m_islands.values(static_cast<int>(IslandType::kPolymer)));
            }
        }
        else {
            if (m_islands.values(static_cast<int>(IslandType::kPolymer)).size() > 0) {
                islandsToProcess.push_back(m_islands.values(static_cast<int>(IslandType::kPolymer)));
            }
            if (m_islands.values(static_cast<int>(IslandType::kSupport)).size() > 0) {
                islandsToProcess.push_back(m_islands.values(static_cast<int>(IslandType::kSupport)));
            }
        }
    }

    // create small tree-like structure of seqence between brims and contained islands (raft, support, polymer)
    QList<QHash<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>> impliedOrder =
        createSequence(allBrims, islandsToProcess);

    // if brims exist, travel to those first, then contained islands
    // otherwise, the order will simply be determined by all the islands in a particular precendence level and the set
    // optimization strategy ie. shortest distance to next polymer island
    QList<QSharedPointer<IslandBase>> alreadyVisited;
    for (QHash<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>> level : impliedOrder) {
        QList<QSharedPointer<IslandBase>> islandSet = level.keys();
        ioo.setIslands(islandSet);

        while (islandSet.size() > 0) {
            int index = ioo.computeNextIndex();
            QSharedPointer<IslandBase> currentIsland = islandSet[index];

            if (!alreadyVisited.contains(currentIsland)) {
                currentIsland->optimize(m_layer_nr, start, previousRegions);
                m_island_order.push_back(currentIsland);
                alreadyVisited.push_back(currentIsland);
            }
            if (level[currentIsland].size() > 0) {
                QList<QSharedPointer<IslandBase>> childrenSet = level[currentIsland];
                ioo.setIslands(childrenSet);

                while (childrenSet.size() > 0) {
                    int index = ioo.computeNextIndex();
                    QSharedPointer<IslandBase> currentIsland = childrenSet[index];
                    currentIsland->optimize(m_layer_nr, start, previousRegions);
                    m_island_order.push_back(currentIsland);
                    childrenSet.removeAt(index);
                }
            }
            islandSet.removeAt(index);
        }
    }

    currentIslands = m_islands.values(static_cast<int>(IslandType::kWireFeed));

    if (currentIslands.size() > 0) {
        ioo.setIslands(currentIslands);

        while (currentIslands.size() > 0) {
            int index = ioo.computeNextIndex();
            QSharedPointer<IslandBase> isl = currentIslands[index];
            currentIslands.removeAt(index);
            isl->optimize(m_layer_nr, start, previousRegions);
            m_island_order.push_back(isl);
        }
    }

    currentIslands = m_islands.values(static_cast<int>(IslandType::kThermalScan));

    if (currentIslands.size() > 0) {
        ioo.setIslands(currentIslands);

        while (currentIslands.size() > 0) {
            int index = ioo.computeNextIndex();
            QSharedPointer<IslandBase> isl = currentIslands[index];
            currentIslands.removeAt(index);
            isl->optimize(m_layer_nr, start, previousRegions);
            m_island_order.push_back(isl);
        }
    }

    if (islandOrderOptimization == IslandOrderOptimization::kLeastRecentlyVisited) {
        start_index = ioo.getFirstIndexSelected();
    }

    for (QSharedPointer<IslandBase> island : m_islands) {
        island->markRegionStartSegment();
    }
}

QList<QHash<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>>
Layer::createSequence(QList<QSharedPointer<IslandBase>> parent, QList<QList<QSharedPointer<IslandBase>>> children) {
    QList<QHash<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>> result;
    int children_size = children.size();
    result.reserve(children_size);
    for (int i = 0; i < children_size; ++i) {
        result.append(QHash<QSharedPointer<IslandBase>, QList<QSharedPointer<IslandBase>>>());
    }

    if (parent.size() == 0) {
        for (int i = 0; i < children_size; ++i) {
            QList<QSharedPointer<IslandBase>> islandSet = children[i];

            for (QSharedPointer<IslandBase> isl : islandSet) {
                result[i].insert(isl, QList<QSharedPointer<IslandBase>>());
            }
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

void Layer::adjustMultiNozzle() {
    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->adjustMultiNozzle();
    }

    // multi-extruder, multi-build
    if (getSb()->setting<bool>(ES::MultiNozzle::kEnableDuplicatePathRemoval)) {
        removeDuplicateIslands();
    }
}

void Layer::removeDuplicateIslands() {
    // get the nozzle materials and offsets from settings
    int num_nozzles = getSb()->setting<int>(ES::MultiNozzle::kNozzleCount);
    QVector<Point> nozzle_offsets;
    nozzle_offsets.reserve(num_nozzles);

    QVector<int> nozzle_materials;
    nozzle_materials.reserve(num_nozzles);

    for (int nozzle = 0; nozzle < num_nozzles; ++nozzle) {
        Distance x = getSb()->setting<Distance>(ES::MultiNozzle::kNozzleOffsetX, nozzle);
        Distance y = getSb()->setting<Distance>(ES::MultiNozzle::kNozzleOffsetY, nozzle);
        Distance z = getSb()->setting<Distance>(ES::MultiNozzle::kNozzleOffsetZ, nozzle);
        int material = getSb()->setting<int>(ES::MultiNozzle::kNozzleMaterial, nozzle);

        nozzle_offsets.push_back(Point(x(), y(), z()));
        nozzle_materials.push_back(material);
    }

    double sameness = getSb()->setting<double>(ES::MultiNozzle::kDuplicatePathSimilarity) / 100.0;
    QVector<int> islands_to_remove = QVector<int>();
    QVector<int> islands_to_keep = QVector<int>();

    // loop through all pairs of islands, check for identical geometry separated by nozzle offset distance
    // if duplicate geometry is found, remove it and turn on multiple nozzles for first island
    auto islands = getIslands();
    int num_islands = islands.size();
    for (int i = 0; i < num_islands; ++i) {
        for (int j = 0; j < num_islands; ++j) {
            if (i == j || islands_to_remove.contains(i) || islands_to_keep.contains(j))
                continue;

            // if i-th island + offset == j-th island, remove j-th island and keep i-th island
            PolygonList geometry_i = islands[i]->getGeometry();
            PolygonList geometry_j = islands[j]->getGeometry();

            // make comparison for all extruder offsets
            for (int nozzle = 1; nozzle < num_nozzles;
                 ++nozzle) { // start at one bc all paths are already assigned nozzle 0
                // island geometry can have multiple polygons
                // islands must have same number of polygons to be the same
                if (geometry_i.size() == geometry_j.size()) {
                    // assume all the polygons match until you find one that doesn't
                    bool all_polygons_match = true;

                    // loop through all the polygons to see if they match
                    for (int p = 0; p < geometry_i.size() && all_polygons_match; ++p) {
                        // two polygons match if they're separated by a fixed offset
                        // and that offset is some nozzles offset
                        Polygon polygon_i = geometry_i[p];
                        Polygon polygon_j = geometry_j[p];

                        // shift first polygon so that it should overlap the second
                        Polygon shifted_poly_i = polygon_i.translate(nozzle_offsets[nozzle].toQVector3D());

                        // get the area of polygons that don't overlap
                        PolygonList xor_result = shifted_poly_i ^ polygon_j;
                        Area no_overlap_area = 0.0;
                        for (Polygon poly : xor_result) {
                            no_overlap_area += poly.area();
                        }

                        // if non-overlapping area is too big, the polygons are not the same
                        if (no_overlap_area() > std::abs((1.0 - sameness) * polygon_i.area()())) {
                            all_polygons_match = false;
                        }
                    }

                    if (all_polygons_match) {
                        // tell first island to print with mulitple extruders
                        islands_to_keep.append(i);
                        islands[i]->addNozzle(nozzle);

                        // delete the pathing of the second island
                        islands_to_remove.append(j);

                        break; // don't need to check any more nozzles
                    }
                }
            }
        }
    }

    // remove the duplicate islands
    for (int i : islands_to_remove) {
        m_islands.remove((int)islands[i]->getType(), islands[i]);
    }
}

void Layer::calculateModifiers(Point& currentLocation) {
    // check for spiral lift for end of layer
    if (this->getSb()->setting<bool>(MS::SpiralLift::kLayerEnable)) {
        QSharedPointer<IslandBase> lastIsland = m_islands.value(static_cast<int>(IslandType::kPolymer)); // back();
        QList<QSharedPointer<RegionBase>> regions = lastIsland->getRegions();
        QSharedPointer<RegionBase> lastRegion = regions.back();
        Path finalPath = lastRegion->getPaths().back();

        if (finalPath.back()->getSb()->setting<PathModifiers>(SS::kPathModifiers) != PathModifiers::kSpiralLift) {
            PathModifierGenerator::GenerateSpiralLift(finalPath,
                                                      this->getSb()->setting<Distance>(MS::SpiralLift::kLiftRadius),
                                                      this->getSb()->setting<Distance>(MS::SpiralLift::kLiftHeight),
                                                      this->getSb()->setting<int>(MS::SpiralLift::kLiftPoints),
                                                      this->getSb()->setting<Velocity>(MS::SpiralLift::kLiftSpeed),
                                                      this->getSb()->setting<bool>(PRS::MachineSetup::kSupportG3));

            // move current location to the end of the spiral lift
            currentLocation = getIslands().last()->getRegions().last()->getPaths().last().back()->end();
        }
    }
}

void Layer::setSb(const QSharedPointer<SettingsBase>& sb) {
    this->Step::setSb(sb);

    // For every island, set the the settings base.
    for (auto isl : m_islands) {
        isl->setSb(this->getSb());
    }
}

void Layer::flagIfDirtySettingsPolygons(const QVector<SettingsPolygon>& new_settings_polys) {
    bool any_non_matching = false;

    for (auto current_poly : m_settings_polygons) {
        bool found_match = false;

        for (auto new_poly : new_settings_polys) {
            if (current_poly.getSettings()->json() == new_poly.getSettings()->json()) {
                found_match = true;
                break;
            }
        }

        if (!found_match) {
            any_non_matching = true;
            break;
        }
    }

    if (any_non_matching) {
        this->setDirtyBit(true);
    }
}

Point Layer::getEndLocation() {
    for (int island_index = m_island_order.size() - 1; island_index >= 0; --island_index) {
        auto regions = m_island_order[island_index]->getRegions();

        for (int region_index = regions.size() - 1; region_index >= 0; --region_index) {
            auto paths = regions[region_index]->getPaths();

            for (int path_index = paths.size() - 1; path_index >= 0; --path_index) {
                auto path = paths[path_index];

                if (path.size() > 0) {
                    return path.back()->end();
                }
            }
        }
    }
    return Point(0, 0, 0);
}

void Layer::unorient() {
    if (!this->isDirty()) {
        // raise the layer by half the layer height, because cross-sections are taken at the center of a layer
        // but the path for the extruder should be at a full layer height
        // don't add half the height if spiralize is enabled because spiralize should start printing
        // with the nozzle sitting on the build surface, z=0
        Point half_shift = m_shift_amount;
        Distance layer_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);

        if (m_sb->setting<bool>(PS::SpecialModes::kEnableSpiralize)) {
            half_shift.z(m_shift_amount.z() - (0.5 * layer_height));
        }
        else {
            half_shift.z(m_shift_amount.z() + (0.5 * layer_height));
        }

        half_shift.x(half_shift.x() - m_sb->setting<double>(PRS::Dimensions::kXOffset));
        half_shift.y(half_shift.y() - m_sb->setting<double>(PRS::Dimensions::kYOffset));

        // rotate and then shift every island in the layer
        QQuaternion rotation = MathUtils::CreateQuaternion(QVector3D(0, 0, 1), m_slicing_plane.normal());

        for (QSharedPointer<IslandBase> island : getIslands()) {
            island->transform(rotation.inverted(), half_shift * -1);
        }

        // unapply current origin shift
        Point origin_shift = Point(.0, .0, .0) - m_shift_amount;
        origin_shift.z(.0);

        for (QSharedPointer<IslandBase> island : getIslands()) {
            island->transform(QQuaternion(), origin_shift * -1);
        }
    }
}

void Layer::reorient() {
    // Unapply current origin shift
    Point origin_shift = Point(.0, .0, .0) - m_shift_amount;
    origin_shift.z(.0);

    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(QQuaternion(), origin_shift);
    }

    // Raise the layer by half the layer height, because cross-sections are taken at the center of a layer but the
    // path for the extruder should be at a full layer height.
    // Don't add half the height if spiralize is enabled because spiralize should start printing with the nozzle
    // sitting on the build surface, z = 0.
    Point half_shift = m_shift_amount;
    const Distance& layer_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    const Distance& half_layer_height = layer_height / 2.0;

    if (m_sb->setting<bool>(PS::SpecialModes::kEnableSpiralize)) {
        half_shift.z(m_shift_amount.z() - half_layer_height);
    }
    else {
        half_shift.z(m_shift_amount.z() + half_layer_height);
    }

    half_shift.x(half_shift.x() - m_sb->setting<double>(PRS::Dimensions::kXOffset));
    half_shift.y(half_shift.y() - m_sb->setting<double>(PRS::Dimensions::kYOffset));

    // Rotate and then shift every island in the layer
    QQuaternion rotation = MathUtils::CreateQuaternion(QVector3D(0, 0, 1), m_slicing_plane.normal());

    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(rotation, half_shift);
    }
}

void Layer::compensateForRafts() {
    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(QQuaternion(), m_raft_shift);
    }
}

float Layer::getMinZ() {
    float min_z = std::numeric_limits<float>::max();

    for (QSharedPointer<IslandBase> island : m_islands) {
        float island_min = island->getMinZ();
        if (island_min < min_z) {
            min_z = island_min;
        }
    }

    return min_z;
}

Point Layer::getFinalLayerLocation() {
    return getIslands().last()->getRegions().last()->getPaths().last().back()->end();
}

void Layer::setSettingsPolygons(QVector<SettingsPolygon>& settings_polygons) {
    m_settings_polygons = settings_polygons;
}

QVector<SettingsPolygon> Layer::getSettingsPolygons() { return m_settings_polygons; }
} // namespace ORNL
