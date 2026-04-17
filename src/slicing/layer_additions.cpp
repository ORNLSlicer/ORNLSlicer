#include "slicing/layer_additions.h"

#include <algorithm>
#include <limits>

#include <qcontainerfwd.h>
#include <qdir.h>
#include <qlist.h>
#include <qmath.h>
#include <qminmax.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/settings_polygon.h"
#include "part/part.h"
#include "slicing/buffered_slicer.h"
#include "step/layer/island/brim_island.h"
#include "step/layer/island/laser_scan_island.h"
#include "step/layer/island/polymer_island.h"
#include "step/layer/island/raft_island.h"
#include "step/layer/island/skirt_island.h"
#include "step/layer/island/thermal_scan_island.h"
#include "step/layer/island/wire_feed_island.h"
#include "step/layer/layer.h"
#include "step/layer/scan_layer.h"
#include "step/step.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

#if HAVE_WIRE_FEED
    #include <wire_feed/wire_feed.h>
#endif

namespace ORNL {
QSharedPointer<Layer> LayerAdditions::createRaft(QSharedPointer<Layer> layer) {
    Distance raft_offset = layer->getSb()->setting<Distance>(MS::PlatformAdhesion::kRaftOffset);

    // Extract island geometry from existing layer
    QVector<PolygonList> island_outlines = layer->getGeometry().splitIntoParts();

    // Offset by raft offset
    PolygonList new_outlines;
    for (PolygonList poly : island_outlines)
        new_outlines |= poly.offset(raft_offset);

    // Extract new islands based on the offsetting
    QVector<PolygonList> new_islands = new_outlines.splitIntoParts();

    // Make a new copy of settings for raft layer
    QSharedPointer<SettingsBase> currents_settings = QSharedPointer<SettingsBase>::create(*layer->getSb());

    // Create a new layer for the raft
    QSharedPointer<Layer> raft_layer = QSharedPointer<Layer>::create(layer->getLayerNumber(), currents_settings);
    raft_layer->setType(StepType::kRaft);
    raft_layer->setGeometry(new_outlines, layer->getNormal());
    raft_layer->setOrientation(layer->getSlicingPlane(), layer->getShift());

    // Build islands for the raft
    QVector<QSharedPointer<IslandBase>> new_layer_islands;
    for (const PolygonList& island_geometry : new_islands) {
        QSharedPointer<RaftIsland> raft_isl =
            QSharedPointer<RaftIsland>::create(island_geometry, currents_settings, QVector<SettingsPolygon>());
        new_layer_islands.append(raft_isl);
    }
    raft_layer->updateIslands(IslandType::kRaft, new_layer_islands);

    return raft_layer;
}

void LayerAdditions::addBrim(QSharedPointer<Layer> layer) {
    QList<QSharedPointer<IslandBase>> raftIslands = layer->getIslands(IslandType::kRaft);
    QList<QSharedPointer<IslandBase>> polymerIslands = layer->getIslands(IslandType::kPolymer);
    QSharedPointer<SettingsBase> currentLocalSettings;
    if (raftIslands.size() > 0)
        currentLocalSettings = QSharedPointer<SettingsBase>::create(*raftIslands[0]->getSb());
    else
        currentLocalSettings = QSharedPointer<SettingsBase>::create(*polymerIslands[0]->getSb());

    PolygonList geometry = layer->getGeometry();

    Distance brimWidth = currentLocalSettings->setting<Distance>(MS::PlatformAdhesion::kBrimWidth);
    Distance beadWidth = currentLocalSettings->setting<Distance>(MS::PlatformAdhesion::kBrimBeadWidth);
    int m_rings = qCeil(brimWidth() / beadWidth());

    // set the offset as the location of the outer most loop, which is where the brim printing starts
    Distance brim_offset = (m_rings - 0.5) * beadWidth;
    QVector<PolygonList> islandOutlines = geometry.splitIntoParts();
    PolygonList newOutlines;
    for (PolygonList poly : islandOutlines) {
        // get the subset of the polygon list that only describes the outer boundary
        //  and set the Brim with an offset from that polygon list
        PolygonList outerPoly = poly.getOutsidePolygons();
        newOutlines |= outerPoly.offset(brim_offset);
    }
    QVector<PolygonList> newIslands = newOutlines.splitIntoParts();

    for (const PolygonList& island_geometry : newIslands) {
        // Polymer builds use polymer islands.
        QSharedPointer<BrimIsland> brim_isl =
            QSharedPointer<BrimIsland>::create(island_geometry, currentLocalSettings, QVector<SettingsPolygon>());
        layer->addIsland(IslandType::kBrim, brim_isl);
    }
}

void LayerAdditions::addSkirt(QSharedPointer<Layer> layer) {
    float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest(),
          minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::lowest();

    QList<QSharedPointer<IslandBase>> raftIslands = layer->getIslands(IslandType::kRaft);
    QList<QSharedPointer<IslandBase>> polymerIslands = layer->getIslands(IslandType::kPolymer);
    QSharedPointer<SettingsBase> currentLocalSettings;
    if (raftIslands.size() > 0)
        currentLocalSettings = QSharedPointer<SettingsBase>::create(*raftIslands[0]->getSb());
    else
        currentLocalSettings = QSharedPointer<SettingsBase>::create(*polymerIslands[0]->getSb());

    QList<QSharedPointer<IslandBase>> islands = layer->getIslands();
    for (QSharedPointer<IslandBase>& isl : islands) {
        PolygonList poly = isl->getGeometry();
        minX = qMin(minX, poly.min().x());
        maxX = qMax(maxX, poly.max().x());
        minY = qMin(minY, poly.min().y());
        maxY = qMax(maxY, poly.max().y());
    }

    QVector<Point> points;
    points.append(Point(minX, minY));
    points.append(Point(maxX, minY));
    points.append(Point(maxX, maxY));
    points.append(Point(minX, maxY));
    QVector<Polygon> poly;
    poly.append(points);
    PolygonList AABB;
    AABB.addAll(poly);
    QSharedPointer<SkirtIsland> skirt_isl =
        QSharedPointer<SkirtIsland>::create(AABB, currentLocalSettings, QVector<SettingsPolygon>());
    layer->addIsland(IslandType::kSkirt, skirt_isl);
}

void LayerAdditions::addThermalScan(QSharedPointer<Layer> layer) {
    PolygonList current_layer_islands;

    // Gather current layer island geometries
    for (QSharedPointer<IslandBase> island : layer->getIslands())
        current_layer_islands += island->getGeometry();

    // Determine thermal_scan_island geometry
    QRect boundary = current_layer_islands.boundingRect();
    Polygon poly = Polygon({boundary.bottomLeft(), boundary.topLeft(), boundary.topRight(), boundary.bottomRight()});
    PolygonList island;
    island += poly;

    QList<QSharedPointer<IslandBase>> raftIslands = layer->getIslands(IslandType::kRaft);
    QList<QSharedPointer<IslandBase>> polymerIslands = layer->getIslands(IslandType::kPolymer);
    QSharedPointer<SettingsBase> currentLocalSettings;
    if (raftIslands.size() > 0)
        currentLocalSettings = QSharedPointer<SettingsBase>::create(*raftIslands[0]->getSb());
    else
        currentLocalSettings = QSharedPointer<SettingsBase>::create(*polymerIslands[0]->getSb());
    // Create thermal_scan_island and add it to the current layer
    QSharedPointer<ThermalScanIsland> thermal_scan_island =
        QSharedPointer<ThermalScanIsland>::create(island, currentLocalSettings, QVector<SettingsPolygon>());
    layer->addIsland(IslandType::kThermalScan, thermal_scan_island);
}

void LayerAdditions::addLaserScan(QSharedPointer<Part> part, int layer_index, double running_total,
                                  QSharedPointer<Step> build_layer, QDir output_path) {
    QSharedPointer<Step> scan_layer = part->step(layer_index, StepType::kScan);
    QSharedPointer<SettingsBase> sb = QSharedPointer<SettingsBase>::create(*build_layer->getSb());

    if (scan_layer == nullptr) {
        scan_layer = QSharedPointer<ScanLayer>::create(layer_index, sb);
        part->addScanLayerToStep(layer_index, qSharedPointerCast<ScanLayer>(scan_layer));
    }

    scan_layer->flagIfDirtySettings(sb);
    if (scan_layer->isDirty()) {
        scan_layer->setSb(sb);

        // Determine laser_scan_island geometry
        QRect boundary = build_layer->getGeometry().boundingRect();
        Polygon poly =
            Polygon({boundary.bottomLeft(), boundary.topLeft(), boundary.topRight(), boundary.bottomRight()});
        PolygonList island;
        island += poly;

        if (layer_index == 0)
            sb->setSetting(PS::Layer::kLayerHeight, 0.0);

        QVector<QSharedPointer<IslandBase>> newIslands;
        newIslands.push_back(QSharedPointer<LaserScanIsland>::create(island, sb, QVector<SettingsPolygon>()));

        Point shift = build_layer->getShift();
        shift.z(running_total);
        if (layer_index == 0)
            shift.z(0.0);

        scan_layer->setOrientation(build_layer->getSlicingPlane(), shift);
        scan_layer->updateIslands(IslandType::kLaserScan, newIslands);
        scan_layer->setGeometry(build_layer->getGeometry(), QVector3D());
        scan_layer->setCompanionFileLocation(output_path);
    }
}

void LayerAdditions::createWireFeedIslands(QSharedPointer<Layer> layer,
                                           QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta,
                                           bool new_islands) {
    QVector<PolygonList> split_geometry = next_layer_meta->modified_geometry.splitIntoParts();

    auto surface = *std::min_element(split_geometry.begin(), split_geometry.end(),
                                     [](const PolygonList& a, const PolygonList& b) { return a.min() < b.min(); });

    auto base = *std::max_element(split_geometry.begin(), split_geometry.end(),
                                  [](const PolygonList& a, const PolygonList& b) { return a.max() < b.max(); });

    QSharedPointer<SettingsBase> base_sb = QSharedPointer<SettingsBase>::create(*next_layer_meta->settings);
    base_sb->setSetting(PS::Inset::kEnable, false);
    base_sb->setSetting(PS::Skin::kEnable, false);
    base_sb->setSetting(PS::Infill::kEnable, false);
    base_sb->setSetting(PS::Skeleton::kEnable, false);

    QSharedPointer<PolymerIsland> base_isl = QSharedPointer<PolymerIsland>::create(
        base, base_sb, next_layer_meta->settings_polygons, next_layer_meta->geometry);

    QSharedPointer<PolymerIsland> surface_isl =
        QSharedPointer<PolymerIsland>::create(surface, next_layer_meta->settings, next_layer_meta->settings_polygons);

    QSharedPointer<WireFeedIsland> wire_feed_isl = QSharedPointer<WireFeedIsland>::create(
        next_layer_meta->setting_bounded_geometry, next_layer_meta->settings, next_layer_meta->settings_polygons);
    if (new_islands) {
        layer->addIsland(IslandType::kPolymer, base_isl);
        layer->addIsland(IslandType::kPolymer, surface_isl);
        layer->addIsland(IslandType::kWireFeed, wire_feed_isl);
    }
    else {
        layer->updateIslands(IslandType::kPolymer, QVector<QSharedPointer<IslandBase>> {base_isl, surface_isl});
        layer->updateIslands(IslandType::kWireFeed, QVector<QSharedPointer<IslandBase>> {wire_feed_isl});
    }
}

void LayerAdditions::addAnchors(QSharedPointer<Layer> layer) {
#ifdef HAVE_WIRE_FEED
    QSharedPointer<SettingsBase> anchor_sb = QSharedPointer<SettingsBase>::create(*layer->getSb());
    anchor_sb->setSetting(PS::Perimeter::kCount, 1);
    QSharedPointer<IslandBase> isl = layer->getIslands(IslandType::kWireFeed).first();

    Point starting_point(INT_MAX, INT_MAX), ending_point(INT_MIN, INT_MIN);
    for (Polygon poly : isl->getGeometry()) {
        for (Point pt : poly) {
            if (pt.x() < starting_point.x())
                starting_point = pt;

            if (pt.x() > ending_point.x())
                ending_point = pt;
        }
    }
    WireFeed::AnchorInfo ai = WireFeed::WireFeed::generateAnchors(
        anchor_sb->setting<double>(ES::WireFeed::kAnchorObjectDistanceLeft),
        anchor_sb->setting<double>(ES::WireFeed::kAnchorObjectDistanceRight),
        anchor_sb->setting<double>(PS::Perimeter::kBeadWidth) / 2.0,
        anchor_sb->setting<double>(ES::WireFeed::kAnchorWidth), anchor_sb->setting<double>(ES::WireFeed::kAnchorHeight),
        starting_point.x(), starting_point.y(), ending_point.x(), ending_point.y());

    for (QVector<QPair<double, double>> anchor : ai.anchors) {
        Polygon poly(anchor);
        PolygonList polyList;
        polyList.addAll(QVector<Polygon> {poly});
        QSharedPointer<AnchorIsland> anchor_island =
            QSharedPointer<AnchorIsland>::create(polyList, anchor_sb, QVector<SettingsPolygon>());
        layer->addIsland(IslandType::kPolymer, anchor_island);
    }

    QVector<Polyline> anchor_wire_feed;
    for (QVector<QPair<double, double>> anchor_wire : ai.wire_feed_for_anchors)
        anchor_wire_feed.push_back(Polyline(anchor_wire));

    QSharedPointer<WireFeedIsland> wire_isl =
        layer->getIslands(IslandType::kWireFeed).first().dynamicCast<WireFeedIsland>();
    wire_isl->setAnchorWireFeed(anchor_wire_feed);
#endif
}
} // namespace ORNL
