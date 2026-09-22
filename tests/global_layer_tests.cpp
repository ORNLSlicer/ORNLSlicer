#include <QSharedPointer>
#include <QVector3D>
#include <QVector>
#include <cstdlib>
#include <iostream>
#include <string>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polygon_list.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "step/layer/island/island_base.h"
#include "step/layer/layer.h"
#include "step/layer/regions/region_base.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
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

bool expect(bool condition, const std::string& message) {
    if (condition) return true;

    std::cerr << message << '\n';
    return false;
}

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
                                            const QSharedPointer<TestIsland>& island) {
    QSharedPointer<ORNL::Layer> layer = QSharedPointer<ORNL::Layer>::create(0, settings);
    layer->setOrientation(ORNL::Plane(ORNL::Point(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f)),
                          ORNL::Point(0.0f, 0.0f, 0.0f));
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
}  // namespace

int main() {
    bool passed = true;

    passed &= expect(commonLayerIslandOrderSettingsAreUsed(),
                     "Expected global layers with common island-order settings to use layer settings.");
    passed &= expect(conflictingLayerIslandOrderSettingsUseGlobalSettings(),
                     "Expected conflicting global-layer island-order settings to fall back to global settings.");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
