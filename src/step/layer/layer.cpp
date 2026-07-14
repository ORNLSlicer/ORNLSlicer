#include "step/layer/layer.h"

#include <algorithm>
#include <cmath>
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
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/settings_polygon.h"
#include "optimizers/island_order_optimizer.h"
#include "optimizers/optimization_anchor.h"
#include "step/layer/island/polymer_island.h"
#include "step/layer/regions/region_base.h"
#include "step/step.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
namespace {
bool isClearanceModifier(PathModifiers modifiers) {
    constexpr PathModifiers clearance_modifiers = PathModifiers::kForwardTipWipe | PathModifiers::kReverseTipWipe |
                                                  PathModifiers::kAngledTipWipe | PathModifiers::kPerimeterTipWipe |
                                                  PathModifiers::kSpiralLift;

    return (modifiers & clearance_modifiers) != PathModifiers::kNone;
}

bool isBelowBuildPlate(const Point& point, const MeshTypes::Plane_3& build_plate) {
    return build_plate.oriented_side(point.toCartesian3D()) == CGAL::ON_NEGATIVE_SIDE;
}

bool lineIntersectsBuildPlate(const Point& start, const Point& end, const MeshTypes::Plane_3& build_plate) {
    if (isBelowBuildPlate(start, build_plate) || isBelowBuildPlate(end, build_plate)) {
        return true;
    }

    return false;
}

bool arcIntersectsBuildPlate(const ArcSegment& arc, const Point& start, const Point& end,
                             const MeshTypes::Plane_3& build_plate) {
    const Point center = arc.center();
    const double start_radius = std::hypot(start.x() - center.x(), start.y() - center.y());
    const double end_radius = std::hypot(end.x() - center.x(), end.y() - center.y());
    constexpr double kRadiusTolerance = 1.0e-4;
    if (start_radius <= kRadiusTolerance || std::abs(start_radius - end_radius) > kRadiusTolerance) {
        return lineIntersectsBuildPlate(start, end, build_plate);
    }

    constexpr double kMaxSampleAngle = M_PI / 36.0;
    const double arc_angle = std::abs(arc.angle()());
    const int sample_count = std::max(1, static_cast<int>(std::ceil(arc_angle / kMaxSampleAngle)));
    const double start_angle = std::atan2(start.y() - center.y(), start.x() - center.x());
    const double signed_angle = arc.counterclockwise() ? arc_angle : -arc_angle;

    Point previous = start;
    for (int sample = 1; sample <= sample_count; ++sample) {
        const double fraction = static_cast<double>(sample) / sample_count;
        const double angle = start_angle + (signed_angle * fraction);
        const Point current(center.x() + (start_radius * std::cos(angle)),
                            center.y() + (start_radius * std::sin(angle)),
                            start.z() + ((end.z() - start.z()) * fraction));

        if (lineIntersectsBuildPlate(previous, current, build_plate)) {
            return true;
        }
        previous = current;
    }

    return false;
}

bool clearanceMoveIntersectsBuildPlate(const QSharedPointer<SegmentBase>& segment, const Point& start, const Point& end,
                                       const MeshTypes::Plane_3& build_plate) {
    const QSharedPointer<ArcSegment> arc = segment.dynamicCast<ArcSegment>();
    if (!arc.isNull()) {
        return arcIntersectsBuildPlate(*arc, start, end, build_plate);
    }

    return lineIntersectsBuildPlate(start, end, build_plate);
}

Point redirectTipWipeEnd(const Point& start, const QVector3D& original_move, const QVector3D& slicing_normal) {
    // Mirror the move's in-plane component while preserving its lift along the slicing normal.
    const QVector3D normal_component = slicing_normal * QVector3D::dotProduct(original_move, slicing_normal);
    const QVector3D redirected_move = (2.0f * normal_component) - original_move;

    return start + Point::fromQVector3D(redirected_move);
}

QSharedPointer<SegmentBase> rewriteRedirectedSegment(const QSharedPointer<SegmentBase>& segment, const Point& start,
                                                     const Point& end) {
    // Arc spiral lifts carry center-point metadata; once redirected, replace them with a linear clearance move so the
    // stored arc geometry stays consistent with the emitted path.
    if (!segment.dynamicCast<ArcSegment>().isNull()) {
        QSharedPointer<LineSegment> redirected_segment = QSharedPointer<LineSegment>::create(start, end);
        redirected_segment->setSb(segment->getSb());
        return redirected_segment;
    }

    QSharedPointer<SegmentBase> redirected_segment = segment->clone();
    redirected_segment->setStart(start);
    redirected_segment->setEnd(end);
    return redirected_segment;
}
} // namespace

Layer::Layer(uint layer_nr, const QSharedPointer<SettingsBase>& sb)
    : Step(sb), m_layer_nr(layer_nr), m_minimum_z_shift(0) {
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
        island->compute(m_layer_nr);

        QSharedPointer<PolymerIsland> polyIsland = island.dynamicCast<PolymerIsland>();
        if (polyIsland != nullptr) {
            polyIsland->reorderRegions();
        }
    }
}

void Layer::connectPaths(Point& start, int& start_index, QVector<QSharedPointer<RegionBase>>& previousRegions) {
    m_island_order.clear();

    for (QSharedPointer<IslandBase> island : m_islands) {
        island->setOptimizationFrame(m_slicing_plane, m_shift_amount);
    }

    // Optimize the layer.
    IslandOrderOptimization islandOrderOptimization =
        static_cast<IslandOrderOptimization>(this->getSb()->setting<int>(PS::Optimizations::kIslandOrder));

    // start index = index of last visited island
    IslandBaseOrderOptimizer ioo(start, m_islands.values(), start_index, islandOrderOptimization); // mode 1

    // seam adjustment
    if (islandOrderOptimization == IslandOrderOptimization::kCustomPoint) {
        Point startOverride = OptimizationAnchor::customIslandOrderPoint(getSb(), m_slicing_plane, m_shift_amount);

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

    for (QSharedPointer<IslandBase> island : getIslands())
        island->fitCircularArcs(this->getSb());
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

Point Layer::getOrientationShift() const {
    Point orientation_shift = m_shift_amount;

    if (m_sb->setting<bool>(PS::SpecialModes::kEnableSpiralize)) {
        // Spiralized paths start on the build surface instead of at a full layer height.
        const Distance half_layer_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight) / 2.0;
        orientation_shift.z(m_shift_amount.z() - half_layer_height);
    }

    orientation_shift.x(orientation_shift.x() - m_sb->setting<double>(PRS::Dimensions::kXOffset));
    orientation_shift.y(orientation_shift.y() - m_sb->setting<double>(PRS::Dimensions::kYOffset));

    return orientation_shift;
}

void Layer::applyMinimumZShift() {
    m_minimum_z_shift = 0;

    if (m_sb->setting<bool>(PS::SpecialModes::kEnableSpiralize))
        return;

    const QVector3D normal = m_slicing_plane.normal().normalized();
    constexpr float kMinimumZComponent = 1.0e-6f;
    const float z_component = std::abs(normal.z());
    if (z_component < kMinimumZComponent)
        return;

    const float current_min_z_value = getMinimumPrintZ();
    if (current_min_z_value == std::numeric_limits<float>::max())
        return;

    const Distance layer_height = m_sb->setting<Distance>(PS::Layer::kLayerHeight);
    const Distance current_min_z = current_min_z_value;
    const Distance minimum_print_z = current_min_z + (layer_height / (2.0 * z_component));

    m_minimum_z_shift = minimum_print_z - current_min_z;
    const Point shift(0.0f, 0.0f, m_minimum_z_shift());
    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(QQuaternion(), shift);
    }
}

void Layer::removeMinimumZShift() {
    if (m_minimum_z_shift == 0)
        return;

    const Point shift(0.0f, 0.0f, -m_minimum_z_shift());
    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(QQuaternion(), shift);
    }
    m_minimum_z_shift = 0;
}

float Layer::getMinimumPrintZ() {
    float minimum_print_z = std::numeric_limits<float>::max();

    for (const QSharedPointer<IslandBase>& island : getIslands()) {
        for (const QSharedPointer<RegionBase>& region : island->getRegions()) {
            for (Path& path : region->getPaths()) {
                for (const QSharedPointer<SegmentBase>& segment : path.getSegments()) {
                    if (segment->isPrintingSegment()) {
                        const float segment_min_z = segment->getMinZ();
                        if (segment_min_z < minimum_print_z)
                            minimum_print_z = segment_min_z;
                    }
                }
            }
        }
    }

    return minimum_print_z;
}

void Layer::unorient() {
    if (!this->isDirty()) {
        removeMinimumZShift();
        const Point orientation_shift = getOrientationShift();

        // rotate and then shift every island in the layer
        QQuaternion rotation = MathUtils::CreateQuaternion(QVector3D(0, 0, 1), m_slicing_plane.normal());

        for (QSharedPointer<IslandBase> island : getIslands()) {
            island->transform(rotation.inverted(), orientation_shift * -1);
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

    const Point orientation_shift = getOrientationShift();

    // Rotate and then shift every island in the layer
    QQuaternion rotation = MathUtils::CreateQuaternion(QVector3D(0, 0, 1), m_slicing_plane.normal());

    for (QSharedPointer<IslandBase> island : getIslands()) {
        island->transform(rotation, orientation_shift);
    }

    applyMinimumZShift();
    redirectClearanceMoves();
}

void Layer::redirectClearanceMoves() {
    QVector3D slicing_normal = m_slicing_plane.normal().normalized();
    if (slicing_normal.isNull()) {
        return;
    }

    const MeshTypes::Plane_3 build_plate(MeshTypes::Point_3(0.0, 0.0, 0.0), MeshTypes::Vector_3(0.0, 0.0, 1.0));

    for (const QSharedPointer<IslandBase>& island : getIslands()) {
        for (const QSharedPointer<RegionBase>& region : island->getRegions()) {
            for (Path& path : region->getPaths()) {
                QList<QSharedPointer<SegmentBase>>& segments = path.getSegments();

                for (int index = 0; index < segments.size();) {
                    if (!isClearanceModifier(segments[index]->getSb()->setting<PathModifiers>(SS::kPathModifiers))) {
                        ++index;
                        continue;
                    }

                    Point original_position = index > 0 ? segments[index - 1]->end() : segments[index]->start();
                    Point redirected_position = original_position;
                    bool has_redirected = false;

                    // Reconstruct the emitted moves from successive endpoints. Multi-segment wipes and spiral lifts
                    // can store starts on the source contour, but the machine moves from the preceding endpoint.
                    while (index < segments.size() &&
                           isClearanceModifier(segments[index]->getSb()->setting<PathModifiers>(SS::kPathModifiers))) {
                        const QSharedPointer<SegmentBase>& segment = segments[index];
                        const Point original_end = segment->end();
                        const QVector3D original_move = (original_end - original_position).toQVector3D();
                        Point candidate_end =
                            has_redirected ? redirected_position + Point::fromQVector3D(original_move) : original_end;
                        bool should_rewrite_segment = has_redirected;

                        if (clearanceMoveIntersectsBuildPlate(segment, redirected_position, candidate_end,
                                                              build_plate)) {
                            candidate_end = redirectTipWipeEnd(redirected_position, original_move, slicing_normal);
                            has_redirected = true;
                            should_rewrite_segment = true;
                        }

                        if (should_rewrite_segment) {
                            segments[index] = rewriteRedirectedSegment(segment, redirected_position, candidate_end);
                        }

                        original_position = original_end;
                        redirected_position = candidate_end;
                        ++index;
                    }
                }
            }
        }
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
