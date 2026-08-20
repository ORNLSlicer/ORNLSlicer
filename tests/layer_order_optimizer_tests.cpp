#include <cstdlib>
#include <iostream>
#include <string>

#include <QList>
#include <QSharedPointer>
#include <QVector>
#include <QVector3D>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "optimizers/layer_order_optimizer.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "step/layer/layer.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace {
bool expect(bool condition, const std::string& message) {
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

QSharedPointer<ORNL::SettingsBase> globalSettings() {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Optimizations::kLayerOrdering, static_cast<int>(ORNL::LayerOrdering::kByHeight));
    settings->setSetting(ORNL::PS::Optimizations::kLayerGroupingTolerance, ORNL::Distance(0.001));
    return settings;
}

QSharedPointer<ORNL::SettingsBase> layerSettings(ORNL::Distance layer_height = ORNL::Distance(0.1)) {
    QSharedPointer<ORNL::SettingsBase> settings = QSharedPointer<ORNL::SettingsBase>::create();
    settings->setSetting(ORNL::PS::Layer::kLayerHeight, layer_height);
    return settings;
}

void appendLayer(QSharedPointer<ORNL::Part> part, const ORNL::Point& plane_point, const QVector3D& plane_normal,
                 ORNL::Distance layer_height = ORNL::Distance(0.1)) {
    const uint layer_number = part->countStepPairs();
    QSharedPointer<ORNL::Layer> layer = QSharedPointer<ORNL::Layer>::create(layer_number, layerSettings(layer_height));
    layer->setOrientation(ORNL::Plane(plane_point, plane_normal), ORNL::Point());
    part->appendStep(layer);
}

QSharedPointer<ORNL::Part> partWithLayer(const ORNL::Point& plane_point, const QVector3D& plane_normal,
                                         ORNL::Distance layer_height = ORNL::Distance(0.1)) {
    QSharedPointer<ORNL::Part> part = QSharedPointer<ORNL::Part>::create();
    appendLayer(part, plane_point, plane_normal, layer_height);
    return part;
}

bool sameOffsetAndNormalGroupTogether() {
    QSharedPointer<ORNL::Part> first_part = partWithLayer(ORNL::Point(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 0.0f, 1.0f));
    QSharedPointer<ORNL::Part> second_part = partWithLayer(ORNL::Point(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 0.0f, 1.0f));

    QList<QSharedPointer<ORNL::GlobalLayer>> layers =
        ORNL::LayerOrderOptimizer::populateSteps(globalSettings(), {first_part, second_part});

    return expect(layers.size() == 1, "Expected matching planes to produce one global layer.") &&
           expect(layers.front()->getStepPairCount() == 2, "Expected both part layers in the grouped global layer.") &&
           expect(layers.front()->hasStepPair(first_part->getId()), "Expected grouped layer to contain first part.") &&
           expect(layers.front()->hasStepPair(second_part->getId()), "Expected grouped layer to contain second part.");
}

bool sameLocalHeightGroupsAcrossGlobalOffsets() {
    QSharedPointer<ORNL::Part> lower_part = partWithLayer(ORNL::Point(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 0.0f, 1.0f));
    QSharedPointer<ORNL::Part> upper_part = partWithLayer(ORNL::Point(0.0f, 0.0f, 2.0f), QVector3D(0.0f, 0.0f, 1.0f));

    QList<QSharedPointer<ORNL::GlobalLayer>> layers =
        ORNL::LayerOrderOptimizer::populateSteps(globalSettings(), {upper_part, lower_part});

    return expect(layers.size() == 1, "Expected same-local-height planes to group despite global offset.") &&
           expect(layers.front()->getStepPairCount() == 2,
                  "Expected both globally separated part layers in the grouped global layer.") &&
           expect(layers.front()->hasStepPair(upper_part->getId()), "Expected grouped layer to contain upper part.") &&
           expect(layers.front()->hasStepPair(lower_part->getId()), "Expected grouped layer to contain lower part.");
}

bool sameLocalHeightGroupsAcrossDifferentNormals() {
    QSharedPointer<ORNL::Part> z_part = partWithLayer(ORNL::Point(0.0f, 0.0f, 1.0f), QVector3D(0.0f, 0.0f, 1.0f));
    QSharedPointer<ORNL::Part> x_part = partWithLayer(ORNL::Point(1.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f));

    QList<QSharedPointer<ORNL::GlobalLayer>> layers =
        ORNL::LayerOrderOptimizer::populateSteps(globalSettings(), {z_part, x_part});

    return expect(layers.size() == 1, "Expected same-local-height planes with different normals to group.") &&
           expect(layers.front()->getStepPairCount() == 2,
                  "Expected both differently oriented part layers in the grouped global layer.") &&
           expect(layers.front()->hasStepPair(z_part->getId()), "Expected grouped layer to contain Z-normal part.") &&
           expect(layers.front()->hasStepPair(x_part->getId()), "Expected grouped layer to contain X-normal part.");
}

bool localLayerHeightOrdersBeforeGlobalOffset() {
    QSharedPointer<ORNL::Part> far_part =
        partWithLayer(ORNL::Point(100.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f), ORNL::Distance(0.1));
    QSharedPointer<ORNL::Part> near_part =
        partWithLayer(ORNL::Point(1.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f), ORNL::Distance(0.2));

    QList<QSharedPointer<ORNL::GlobalLayer>> layers =
        ORNL::LayerOrderOptimizer::populateSteps(globalSettings(), {near_part, far_part});

    return expect(layers.size() == 2, "Expected non-Z parallel planes to produce two global layers.") &&
           expect(layers[0]->hasStepPair(far_part->getId()),
                  "Expected smaller local layer height to order before lower absolute plane offset.") &&
           expect(layers[1]->hasStepPair(near_part->getId()),
                  "Expected larger local layer height to order second.");
}

bool localLayerHeightOrdersAcrossParts() {
    QSharedPointer<ORNL::Part> thick_layer_part =
        partWithLayer(ORNL::Point(100.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f), ORNL::Distance(0.3));
    QSharedPointer<ORNL::Part> thin_layer_part = QSharedPointer<ORNL::Part>::create();
    appendLayer(thin_layer_part, ORNL::Point(1.0f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f), ORNL::Distance(0.1));
    appendLayer(thin_layer_part, ORNL::Point(1.1f, 0.0f, 0.0f), QVector3D(1.0f, 0.0f, 0.0f), ORNL::Distance(0.1));

    QList<QSharedPointer<ORNL::GlobalLayer>> layers =
        ORNL::LayerOrderOptimizer::populateSteps(globalSettings(), {thick_layer_part, thin_layer_part});

    return expect(layers.size() == 3, "Expected one thick layer and two thin layers to stay separate.") &&
           expect(layers[0]->hasStepPair(thin_layer_part->getId()), "Expected first thin local layer to be first.") &&
           expect(layers[1]->hasStepPair(thin_layer_part->getId()), "Expected second thin local layer to be second.") &&
           expect(layers[2]->hasStepPair(thick_layer_part->getId()), "Expected thicker local layer to be third.");
}
} // namespace

int main() {
    bool passed = true;
    passed &= sameOffsetAndNormalGroupTogether();
    passed &= sameLocalHeightGroupsAcrossGlobalOffsets();
    passed &= sameLocalHeightGroupsAcrossDifferentNormals();
    passed &= localLayerHeightOrdersBeforeGlobalOffset();
    passed &= localLayerHeightOrdersAcrossParts();

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
