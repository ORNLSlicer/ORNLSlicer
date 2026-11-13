#include <QSharedPointer>
#include <QVector3D>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include <CGAL/squared_distance_2.h>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "geometry/segments/travel.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "step/layer/island/island_base.h"
#include "step/layer/layer.h"
#include "step/layer/regions/region_base.h"
#include "test_utils.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
class TestRegion : public ORNL::RegionBase {
   public:
    TestRegion(const QSharedPointer<ORNL::SettingsBase>& settings)
        : RegionBase(settings, 0, {}, {}, ORNL::RegionType::kPerimeter) {}

    QString writeGCode(QSharedPointer<ORNL::WriterBase>) override {
        return {};
    }
    void compute(uint) override {}
    void optimize(int, ORNL::Point&, bool&) override {}

   protected:
    ORNL::Path createPath(ORNL::Polyline) override {
        return {};
    }
    void calculateModifiers(ORNL::Path&, bool) override {}
};

class TestIsland : public ORNL::IslandBase {
   public:
    TestIsland(const ORNL::PolygonList& geometry, const QSharedPointer<ORNL::SettingsBase>& settings,
               QVector<const TestIsland*>* optimized_order)
        : IslandBase(geometry, settings, QVector<ORNL::SettingsPolygon>()), m_optimized_order(optimized_order) {
        m_island_type = ORNL::IslandType::kPolymer;
    }

    void optimize(int, ORNL::Point&, QVector<QSharedPointer<ORNL::RegionBase>>&) override {
        if (m_optimized_order != nullptr) m_optimized_order->push_back(this);
    }

   private:
    QVector<const TestIsland*>* m_optimized_order;
};

QSharedPointer<ORNL::SettingsBase> settingsWithIslandOrder(ORNL::IslandOrderOptimization order) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Optimizations::kIslandOrder, static_cast<int>(order));
    settings->setSetting(ORNL::PS::Optimizations::kCustomIslandXLocation, 0.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomIslandYLocation, 0.0);
    settings->setSetting(ORNL::PS::Optimizations::kCustomIslandZLocation, 0.0);
    settings->setSetting(ORNL::PS::Optimizations::kSeamAttractorVectorX, 0.0f);
    settings->setSetting(ORNL::PS::Optimizations::kSeamAttractorVectorY, 0.0f);
    settings->setSetting(ORNL::PS::Optimizations::kSeamAttractorVectorZ, 1.0f);
    settings->setSetting(ORNL::PS::Support::kPrintFirst, false);
    return settings;
}

ORNL::PolygonList squareAt(float x) {
    ORNL::Polygon polygon;
    polygon.append(ORNL::Point(x - 5.0f, -5.0f, 0.0f));
    polygon.append(ORNL::Point(x + 5.0f, -5.0f, 0.0f));
    polygon.append(ORNL::Point(x + 5.0f, 5.0f, 0.0f));
    polygon.append(ORNL::Point(x - 5.0f, 5.0f, 0.0f));

    ORNL::PolygonList polygons;
    polygons += polygon;
    return polygons;
}

QSharedPointer<ORNL::Layer> layerWithIsland(const QSharedPointer<ORNL::SettingsBase>& settings,
                                            const QSharedPointer<TestIsland>& island,
                                            const ORNL::Point& shift = ORNL::Point(0.0f, 0.0f, 0.0f)) {
    QSharedPointer<ORNL::Layer> layer = QSharedPointer<ORNL::Layer>::create(0, settings);
    layer->setOrientation(ORNL::Plane(ORNL::Point(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f)), shift);
    layer->addIsland(ORNL::IslandType::kPolymer, island);
    return layer;
}

void addLayer(ORNL::GlobalLayer& global_layer, const QString& uuid, const QSharedPointer<ORNL::Layer>& layer) {
    ORNL::Part::StepPair step_pair;
    step_pair.printing_layer = layer;
    global_layer.addStepPair(QUuid(uuid), step_pair);
}

bool optimizeGlobalLayer(ORNL::GlobalLayer& global_layer, const QSharedPointer<ORNL::SettingsBase>& global_settings,
                         QVector<const TestIsland*>& optimized_order) {
    ORNL::Point start(0.0f, 0.0f, 0.0f);
    int start_index = -1;
    QVector<QSharedPointer<ORNL::RegionBase>> previous_regions;

    optimized_order.clear();
    global_layer.connectPaths(global_settings, start, start_index, previous_regions);
    return !optimized_order.isEmpty();
}

bool commonLayerIslandOrderSettingsAreUsed() {
    QVector<const TestIsland*> optimized_order;
    const QSharedPointer<ORNL::SettingsBase> global_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    const QSharedPointer<ORNL::SettingsBase> layer_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextFarthest);

    QSharedPointer<TestIsland> near_island =
        QSharedPointer<TestIsland>::create(squareAt(100.0f), layer_settings, &optimized_order);
    QSharedPointer<TestIsland> far_island =
        QSharedPointer<TestIsland>::create(squareAt(1000.0f), layer_settings, &optimized_order);

    ORNL::GlobalLayer global_layer(0);
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000001}", layerWithIsland(layer_settings, near_island));
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000002}", layerWithIsland(layer_settings, far_island));

    return optimizeGlobalLayer(global_layer, global_settings, optimized_order) &&
           optimized_order.first() == far_island.data();
}

bool commonCustomIslandOrderSettingsIgnoreHorizontalShift() {
    QVector<const TestIsland*> optimized_order;
    const QSharedPointer<ORNL::SettingsBase> global_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    const QSharedPointer<ORNL::SettingsBase> layer_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kCustomPoint);
    layer_settings->setSetting(ORNL::PS::Optimizations::kCustomIslandXLocation, 2000.0);

    QSharedPointer<TestIsland> near_island =
        QSharedPointer<TestIsland>::create(squareAt(100.0f), layer_settings, &optimized_order);
    QSharedPointer<TestIsland> far_island =
        QSharedPointer<TestIsland>::create(squareAt(1000.0f), layer_settings, &optimized_order);

    ORNL::GlobalLayer global_layer(0);
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000001}",
             layerWithIsland(layer_settings, near_island, ORNL::Point(100.0f, 0.0f, 0.0f)));
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000002}",
             layerWithIsland(layer_settings, far_island, ORNL::Point(1000.0f, 0.0f, 0.0f)));

    return optimizeGlobalLayer(global_layer, global_settings, optimized_order) &&
           optimized_order.first() == far_island.data();
}

bool equivalentCustomIslandOrderSettingsUseLayerAnchor() {
    QVector<const TestIsland*> optimized_order;
    const QSharedPointer<ORNL::SettingsBase> global_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    const QSharedPointer<ORNL::SettingsBase> near_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kCustomPoint);
    const QSharedPointer<ORNL::SettingsBase> far_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kCustomPoint);
    near_settings->setSetting(ORNL::PS::Optimizations::kCustomIslandXLocation, 2000.0);
    far_settings->setSetting(ORNL::PS::Optimizations::kCustomIslandXLocation, 2000.0);
    far_settings->setSetting(ORNL::PS::Optimizations::kCustomIslandZLocation, 100.0);

    QSharedPointer<TestIsland> near_island =
        QSharedPointer<TestIsland>::create(squareAt(100.0f), near_settings, &optimized_order);
    QSharedPointer<TestIsland> far_island =
        QSharedPointer<TestIsland>::create(squareAt(1000.0f), far_settings, &optimized_order);

    ORNL::GlobalLayer global_layer(0);
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000001}", layerWithIsland(near_settings, near_island));
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000002}", layerWithIsland(far_settings, far_island));

    return optimizeGlobalLayer(global_layer, global_settings, optimized_order) &&
           optimized_order.first() == far_island.data();
}

bool conflictingLayerIslandOrderSettingsUseGlobalSettings() {
    QVector<const TestIsland*> optimized_order;
    const QSharedPointer<ORNL::SettingsBase> global_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    const QSharedPointer<ORNL::SettingsBase> farthest_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextFarthest);
    const QSharedPointer<ORNL::SettingsBase> closest_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);

    QSharedPointer<TestIsland> near_island =
        QSharedPointer<TestIsland>::create(squareAt(100.0f), closest_settings, &optimized_order);
    QSharedPointer<TestIsland> far_island =
        QSharedPointer<TestIsland>::create(squareAt(1000.0f), farthest_settings, &optimized_order);

    ORNL::GlobalLayer global_layer(0);
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000001}", layerWithIsland(farthest_settings, far_island));
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000002}", layerWithIsland(closest_settings, near_island));

    return optimizeGlobalLayer(global_layer, global_settings, optimized_order) &&
           optimized_order.first() == near_island.data();
}

bool globalCustomIslandOrderUsesAnchorWhenLayerFramesConflict() {
    QVector<const TestIsland*> optimized_order;
    const QSharedPointer<ORNL::SettingsBase> global_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kCustomPoint);
    global_settings->setSetting(ORNL::PS::Optimizations::kCustomIslandXLocation, 2000.0);
    const QSharedPointer<ORNL::SettingsBase> closest_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    const QSharedPointer<ORNL::SettingsBase> farthest_settings =
        settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextFarthest);

    QSharedPointer<TestIsland> near_island =
        QSharedPointer<TestIsland>::create(squareAt(100.0f), closest_settings, &optimized_order);
    QSharedPointer<TestIsland> far_island =
        QSharedPointer<TestIsland>::create(squareAt(1000.0f), farthest_settings, &optimized_order);

    ORNL::GlobalLayer global_layer(0);
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000001}", layerWithIsland(closest_settings, near_island));
    addLayer(global_layer, "{00000000-0000-0000-0000-000000000002}",
             layerWithIsland(farthest_settings, far_island, ORNL::Point(10.0f, 0.0f, 0.0f)));

    return optimizeGlobalLayer(global_layer, global_settings, optimized_order) &&
           optimized_order.first() == far_island.data();
}

QSharedPointer<ORNL::TravelSegment> appendTravel(ORNL::Path& path) {
    auto travel = QSharedPointer<ORNL::TravelSegment>::create(ORNL::Point(-2000, 0, 0), ORNL::Point(2000, 0, 0));
    travel->getSb()->setSetting(ORNL::SS::kSpeed, ORNL::Velocity(3000));
    path.append(travel);
    return travel;
}

void appendWall(ORNL::Path& path, ORNL::RegionType type, bool deposition = true) {
    const QVector<ORNL::Point> corners = {{-500, -500, 0}, {500, -500, 0}, {500, 500, 0}, {-500, 500, 0}};
    for (int i = 0; i < corners.size(); ++i) {
        auto line = QSharedPointer<ORNL::LineSegment>::create(corners[i], corners[(i + 1) % corners.size()]);
        line->getSb()->setSetting(ORNL::SS::kRegionType, type);
        line->getSb()->setSetting(ORNL::SS::kWidth, ORNL::Distance(100));
        line->setDepositionActive(deposition);
        path.append(line);
    }
}

void processTravels(const QSharedPointer<ORNL::SettingsBase>& settings, const QSharedPointer<TestRegion>& region) {
    auto island = QSharedPointer<TestIsland>::create(squareAt(0), settings, nullptr);
    island->addRegion(region);
    ORNL::GlobalLayer layer(0);
    addLayer(layer, "{00000000-0000-0000-0000-000000000001}", layerWithIsland(settings, island));
    QVector<const TestIsland*> order;
    optimizeGlobalLayer(layer, settings, order);
    ORNL::Point location;
    layer.calculateModifiers(settings, location, 0);
}

bool travelStandoffUsesResolvedDefaultBeadWidth() {
    auto settings = settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, true);
    settings->setSetting(ORNL::PS::Layer::kBeadWidth, ORNL::Distance(300));
    auto local_settings = settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    local_settings->setSetting(ORNL::PS::Perimeter::kBeadWidth, ORNL::Distance(1200));
    auto region = QSharedPointer<TestRegion>::create(local_settings);
    ORNL::Path path;
    appendWall(path, ORNL::RegionType::kPerimeter);
    auto travel             = appendTravel(path);
    const ORNL::Point start = travel->start(), end = travel->end();
    region->getPaths().append(path);

    auto clearance = [&]() {
        using Point2   = ORNL::MeshTypes::Kernel::Point_2;
        using Segment2 = ORNL::MeshTypes::Kernel::Segment_2;
        const QVector<Point2> corners {{-500, -500}, {500, -500}, {500, 500}, {-500, 500}};
        QVector<ORNL::Point> points {travel->start()};
        points.append(travel->waypoints());
        points.append(travel->end());
        double minimum_squared = std::numeric_limits<double>::infinity();
        for (int i = 1; i < points.size(); ++i) {
            const Segment2 leg(Point2(points[i - 1].x(), points[i - 1].y()), Point2(points[i].x(), points[i].y()));
            for (int edge = 0; edge < corners.size(); ++edge) {
                minimum_squared = std::min(
                    minimum_squared, CGAL::squared_distance(leg, Segment2(corners[edge], corners[(edge + 1) % 4])));
            }
        }
        return std::sqrt(minimum_squared);
    };

    processTravels(settings, region);
    const double global_clearance = clearance();
    bool passed = ORNL::Testing::expect(global_clearance >= 350 && global_clearance <= 353,
                                        "Missing local default width must use global width outside the actual bead.");
    local_settings->setSetting(ORNL::PS::Layer::kBeadWidth, ORNL::Distance(450));
    processTravels(settings, region);
    const double local_clearance = clearance();
    passed &= ORNL::Testing::expect(local_clearance >= 500 && local_clearance <= 503 &&
                                        ORNL::Testing::near(local_clearance - global_clearance, 150, 3),
                                    "A local default width must override the global travel standoff.");
    passed &= ORNL::Testing::expect(travel->start() == start && travel->end() == end,
                                    "Default-width standoff must preserve the original travel endpoints.");
    return passed;
}

bool travelsAvoidOnlyCompletedPerimeters() {
    auto settings = settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, true);
    auto region = QSharedPointer<TestRegion>::create(settings);
    ORNL::Path path;
    auto before = appendTravel(path);
    appendWall(path, ORNL::RegionType::kPerimeter);
    auto after = appendTravel(path);
    region->getPaths().append(path);
    processTravels(settings, region);
    bool passed = ORNL::Testing::expect(before->waypoints().isEmpty(), "Future perimeters must not obstruct travel.");
    passed &= ORNL::Testing::expect(!after->waypoints().isEmpty(), "Completed perimeters must trigger a detour.");
    passed &=
        ORNL::Testing::expect(after->start() == ORNL::Point(-2000, 0, 0) && after->end() == ORNL::Point(2000, 0, 0) &&
                                  after->getSb()->setting<ORNL::Velocity>(ORNL::SS::kSpeed) == ORNL::Velocity(3000),
                              "A detour must preserve endpoints and travel settings.");

    auto next_region = QSharedPointer<TestRegion>::create(settings);
    ORNL::Path next_path;
    auto next_layer_travel = appendTravel(next_path);
    next_region->getPaths().append(next_path);
    processTravels(settings, next_region);
    passed &= ORNL::Testing::expect(next_layer_travel->waypoints().isEmpty(),
                                    "Perimeter obstacle history must reset for each layer.");

    settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, false);
    processTravels(settings, region);
    passed &= ORNL::Testing::expect(after->waypoints().isEmpty(), "Disabled avoidance must retain direct travel.");
    return passed;
}

bool insetsAndNonDepositingMovesDoNotObstructTravel() {
    auto settings = settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, true);
    for (bool depositing_inset : {true, false}) {
        auto region = QSharedPointer<TestRegion>::create(settings);
        ORNL::Path path;
        appendWall(path, depositing_inset ? ORNL::RegionType::kInset : ORNL::RegionType::kPerimeter, depositing_inset);
        auto travel = appendTravel(path);
        region->getPaths().append(path);
        processTravels(settings, region);
        if (!ORNL::Testing::expect(travel->waypoints().isEmpty(),
                                   "Inset walls and non-depositing perimeter moves must not cause detours."))
            return false;
    }
    return true;
}

bool perimeterHistorySpansIslandsAndHonorsLocalSettings() {
    auto settings = settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, true);
    auto local_settings = QSharedPointer<ORNL::SettingsBase>::create(*settings);
    local_settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, false);
    auto wall_region = QSharedPointer<TestRegion>::create(settings);
    ORNL::Path wall;
    appendWall(wall, ORNL::RegionType::kPerimeter);
    wall_region->getPaths().append(wall);
    auto travel_region = QSharedPointer<TestRegion>::create(local_settings);
    ORNL::Path path;
    auto travel = appendTravel(path);
    travel_region->getPaths().append(path);

    QVector<const TestIsland*> order;
    auto first  = QSharedPointer<TestIsland>::create(squareAt(0), settings, &order);
    auto second = QSharedPointer<TestIsland>::create(squareAt(5000), settings, &order);
    first->addRegion(wall_region);
    second->addRegion(travel_region);
    ORNL::GlobalLayer layer(0);
    addLayer(layer, "{00000000-0000-0000-0000-000000000001}", layerWithIsland(settings, first));
    addLayer(layer, "{00000000-0000-0000-0000-000000000002}", layerWithIsland(settings, second));
    optimizeGlobalLayer(layer, settings, order);
    ORNL::Point location;
    layer.calculateModifiers(settings, location, 0);
    bool passed = ORNL::Testing::expect(order.size() == 2 && order.front() == first.data(),
                                        "The wall fixture must print before the travel fixture.");
    passed &= ORNL::Testing::expect(travel->waypoints().isEmpty(),
                                    "A local disabled setting must override enabled global avoidance.");
    local_settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, true);
    layer.calculateModifiers(settings, location, 0);
    passed &= ORNL::Testing::expect(!travel->waypoints().isEmpty(),
                                    "Completed perimeters in another part's island must obstruct later travel.");
    return passed;
}

bool avoidsRightSemicircle(const ORNL::TravelSegment& travel) {
    QVector<ORNL::Point> points {travel.start()};
    points.append(travel.waypoints());
    points.append(travel.end());
    for (int index = 1; index < points.size(); ++index) {
        const auto& a   = points[index - 1];
        const auto& b   = points[index];
        const double dx = b.x() - a.x(), dy = b.y() - a.y();
        const double quadratic    = dx * dx + dy * dy;
        const double linear       = 2.0 * (a.x() * dx + a.y() * dy);
        const double constant     = a.x() * a.x() + a.y() * a.y() - 500.0 * 500.0;
        const double discriminant = linear * linear - 4.0 * quadratic * constant;
        if (discriminant < 0) continue;
        for (double sign : {-1.0, 1.0}) {
            const double t = (-linear + sign * std::sqrt(discriminant)) / (2.0 * quadratic);
            if (t < -1.0e-7 || t > 1 + 1.0e-7 || a.x() + t * dx < 0) continue;
            if (index == 1 && std::abs(t) < 1.0e-7) continue;
            if (index + 1 == points.size() && std::abs(t - 1) < 1.0e-7) continue;
            return false;
        }
    }
    return true;
}

bool curvedPerimetersObstructTravel() {
    auto settings = settingsWithIslandOrder(ORNL::IslandOrderOptimization::kNextClosest);
    settings->setSetting(ORNL::PS::Travel::kAvoidCrossingPerimeters, true);
    auto region = QSharedPointer<TestRegion>::create(settings);
    ORNL::Path path;
    auto arc = QSharedPointer<ORNL::ArcSegment>::create(ORNL::Point(0, -500, 0), ORNL::Point(0, 500, 0),
                                                        ORNL::Point(0, 0, 0), ORNL::Angle(M_PI), true);
    arc->getSb()->setSetting(ORNL::SS::kRegionType, ORNL::RegionType::kPerimeter);
    arc->getSb()->setSetting(ORNL::SS::kWidth, ORNL::Distance(100));
    path.append(arc);
    // This crosses the semicircle but not the chord between its endpoints.
    auto travel = QSharedPointer<ORNL::TravelSegment>::create(ORNL::Point(250, -1000, 0), ORNL::Point(250, 1000, 0));
    path.append(travel);
    auto from_arc = QSharedPointer<ORNL::TravelSegment>::create(arc->start(), ORNL::Point(250, 1000, 0));
    path.append(from_arc);
    region->getPaths().append(path);
    processTravels(settings, region);
    return ORNL::Testing::expect(!travel->waypoints().isEmpty() && avoidsRightSemicircle(*travel) &&
                                     !from_arc->waypoints().isEmpty() && avoidsRightSemicircle(*from_arc),
                                 "Routed travel must avoid the actual arc, including when departing its endpoint.");
}

bool travelWaypointsFollowLayerTransforms() {
    ORNL::TravelSegment travel(ORNL::Point(0, 0, 0), ORNL::Point(100, 0, 0));
    travel.setWaypoints({ORNL::Point(0, 100, 0), ORNL::Point(100, 100, 0)});
    bool passed         = ORNL::Testing::expect(ORNL::Testing::near(travel.length()(), 300),
                                                "Travel length must include every routed leg.");
    const auto rotation = QQuaternion::fromAxisAndAngle(QVector3D(1, 0, 0), 90);
    travel.rotate(rotation);
    travel.shift(ORNL::Point(10, 20, 30));
    passed &= ORNL::Testing::expect(ORNL::Testing::near3DPoint(travel.waypoints()[0], 10, 20, 130),
                                    "Travel waypoints must rotate and shift with the layer.");
    auto clone = travel.clone().dynamicCast<ORNL::TravelSegment>();
    clone->reverse();
    passed &= ORNL::Testing::expect(clone->waypoints().first() == travel.waypoints().last() &&
                                        clone->start() == travel.end() && clone->end() == travel.start(),
                                    "Cloning and reversal must preserve the complete routed travel.");
    return passed;
}
}  // namespace

int main() {
    bool passed = true;

    passed &= ORNL::Testing::expect(commonLayerIslandOrderSettingsAreUsed(),
                                    "Expected global layers with common island-order settings to use layer settings.");
    passed &=
        ORNL::Testing::expect(commonCustomIslandOrderSettingsIgnoreHorizontalShift(),
                              "Expected horizontal per-part shifts to preserve common custom island-order settings.");
    passed &=
        ORNL::Testing::expect(equivalentCustomIslandOrderSettingsUseLayerAnchor(),
                              "Expected equivalent projected custom anchors to preserve layer island-order settings.");
    passed &= ORNL::Testing::expect(
        conflictingLayerIslandOrderSettingsUseGlobalSettings(),
        "Expected conflicting global-layer island-order settings to fall back to global settings.");
    passed &= ORNL::Testing::expect(
        globalCustomIslandOrderUsesAnchorWhenLayerFramesConflict(),
        "Expected global custom island-order fallback to use an anchor layer when frames conflict.");
    passed &= travelsAvoidOnlyCompletedPerimeters();
    passed &= travelStandoffUsesResolvedDefaultBeadWidth();
    passed &= insetsAndNonDepositingMovesDoNotObstructTravel();
    passed &= perimeterHistorySpansIslandsAndHonorsLocalSettings();
    passed &= curvedPerimetersObstructTravel();
    passed &= travelWaypointsFollowLayerTransforms();

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
