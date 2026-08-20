#include "optimizers/layer_order_optimizer.h"

#include <algorithm>
#include <cmath>

#include <qassert.h>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qminmax.h>
#include <qsharedpointer.h>
#include <quuid.h>
#include <qvector.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "step/step.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
namespace {
constexpr double kLocalLayerZComparisonTolerance = 1.0e-7;

struct LayerCandidate {
    int part_index;
    int step_index;
    QSharedPointer<Part> part;
    QUuid part_id;
    Distance local_layer_z;
};

bool localLayerZsAreEqual(Distance lhs, Distance rhs, double tolerance = kLocalLayerZComparisonTolerance) {
    return std::abs(lhs() - rhs()) <= tolerance;
}

bool candidatesCanShareGlobalLayer(const LayerCandidate& lhs, const LayerCandidate& rhs, Distance tolerance) {
    return localLayerZsAreEqual(lhs.local_layer_z, rhs.local_layer_z, tolerance());
}

bool layerCandidateLessThan(const LayerCandidate& lhs, const LayerCandidate& rhs) {
    if (localLayerZsAreEqual(lhs.local_layer_z, rhs.local_layer_z))
        return lhs.part_index < rhs.part_index;

    return lhs.local_layer_z < rhs.local_layer_z;
}
} // namespace

QList<QSharedPointer<GlobalLayer>> LayerOrderOptimizer::populateSteps(QSharedPointer<SettingsBase> global_sb,
                                                                      QVector<QSharedPointer<Part>> build_parts) {
    // list to return at end of function
    QList<QSharedPointer<GlobalLayer>> global_layers = QList<QSharedPointer<GlobalLayer>>();

    // get the layer ordering method, and then populate the global layers accordingly
    LayerOrdering order_method = global_sb->setting<LayerOrdering>(PS::Optimizations::kLayerOrdering);

    if (order_method == LayerOrdering::kByHeight) {
        int num_global_steps = 0;
        const Distance layer_grouping_tolerance =
            global_sb->setting<Distance>(PS::Optimizations::kLayerGroupingTolerance);

        // Track the current step by build-parts index so ties keep the input part order.
        QVector<int> current_layer(build_parts.size(), 0);
        QVector<Distance> current_local_z(build_parts.size(), Distance(0.0));

        while (true) {
            QVector<LayerCandidate> candidates;
            candidates.reserve(build_parts.size());

            for (int part_index = 0, part_count = build_parts.size(); part_index < part_count; ++part_index) {
                QSharedPointer<Part> part = build_parts[part_index];
                if (part.isNull())
                    continue;

                if (current_layer[part_index] >= part->countStepPairs())
                    continue;

                QSharedPointer<Step> current_step = part->getStepPair(current_layer[part_index]).printing_layer;
                Q_ASSERT(!current_step.isNull());
                if (current_step.isNull()) {
                    ++current_layer[part_index];
                    continue;
                }

                if (current_step->getSlicingPlane().normal().isNull()) {
                    Q_ASSERT(false);
                    ++current_layer[part_index];
                    continue;
                }

                const Distance layer_height = current_step->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
                const Distance local_layer_z = current_local_z[part_index] + layer_height;
                candidates.push_back({part_index, current_layer[part_index], part, part->getId(), local_layer_z});
            }

            if (candidates.isEmpty())
                break;

            const auto min_candidate =
                std::min_element(candidates.constBegin(), candidates.constEnd(), layerCandidateLessThan);

            // Layers at the same local height can be printed at the same time, even when their slicing planes differ.
            QSharedPointer<GlobalLayer> new_global_layer = QSharedPointer<GlobalLayer>::create(num_global_steps);
            bool added_step_pair = false;
            for (const LayerCandidate& candidate : candidates) {
                if (candidatesCanShareGlobalLayer(candidate, *min_candidate, layer_grouping_tolerance)) {
                    new_global_layer->addStepPair(candidate.part_id, candidate.part->getStepPair(candidate.step_index));
                    current_local_z[candidate.part_index] = candidate.local_layer_z;
                    ++current_layer[candidate.part_index];
                    added_step_pair = true;
                }
            }

            Q_ASSERT(added_step_pair);
            if (!added_step_pair)
                break;

            global_layers.push_back(new_global_layer);
            ++num_global_steps;
        }
    }
    else if (order_method == LayerOrdering::kByLayerNumber) {
        // look at all the parts to find the maximum number of steps
        // this will be the number of global layers
        int max_steps = 0;
        for (auto part : build_parts)
            max_steps = qMax(max_steps, part->countStepPairs());

        global_layers.reserve(max_steps);

        // for each global layer
        //     make a new layer
        //     add the steps/layers/scan layers from all the parts
        for (int step = 0; step < max_steps; ++step) {
            QSharedPointer<GlobalLayer> new_global_layer = QSharedPointer<GlobalLayer>::create(step);

            for (auto part : build_parts) {
                if (step < part->countStepPairs())
                    new_global_layer->addStepPair(part->getId(), part->getStepPair(step));
            }

            global_layers.push_back(new_global_layer);
        }
    }
    else if (order_method == LayerOrdering::kByPart) {
        // printing parts sequentially, so every part layer gets its own global layer
        int max_steps = 0;
        for (auto part : build_parts)
            max_steps += part->countStepPairs();

        global_layers.reserve(max_steps);

        int num_g_steps = 0;
        for (auto part : build_parts) {
            for (int s = 0, max_steps = part->countStepPairs(); s < max_steps; ++s) {
                QSharedPointer<GlobalLayer> new_global_layer = QSharedPointer<GlobalLayer>::create(num_g_steps);
                new_global_layer->addStepPair(part->getId(), part->getStepPair(s));
                global_layers.push_back(new_global_layer);

                ++num_g_steps;
            }
        }
    }
    else {
        Q_ASSERT(false); // invalid order method
    }

    return global_layers;
}
} // namespace ORNL
