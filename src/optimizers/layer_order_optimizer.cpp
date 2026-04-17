#include "optimizers/layer_order_optimizer.h"

#include <qassert.h>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmap.h>
#include <qminmax.h>
#include <qsharedpointer.h>
#include <quuid.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "step/step.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
QSharedPointer<GlobalLayer> LayerOrderOptimizer::populateStep(QVector<QSharedPointer<Part>> build_parts) {
    //! \note called by real-time slicers only

    QSharedPointer<GlobalLayer> new_g_layer =
        QSharedPointer<GlobalLayer>::create(0); // only one layer is kept at a time, so we can always number it 0

    // add all the dirty step groups to the global layer
    for (auto& part : build_parts) {
        QList<Part::StepPair> dirty_step_groups = part->getDirtyStepPairs();
        for (auto& step_group : dirty_step_groups)
            new_g_layer->addStepPair(part->getId(), step_group);
    }

    return new_g_layer;
}

QList<QSharedPointer<GlobalLayer>> LayerOrderOptimizer::populateSteps(QSharedPointer<SettingsBase> global_sb,
                                                                      QVector<QSharedPointer<Part>> build_parts) {
    // list to return at end of function
    QList<QSharedPointer<GlobalLayer>> global_layers = QList<QSharedPointer<GlobalLayer>>();

    // get the layer ordering method, and then populate the global layers accordingly
    // after global layers have been assigned, assign nozzles/tools (if necessary)
    LayerOrdering order_method = global_sb->setting<LayerOrdering>(ES::PrinterConfig::kLayerOrdering);

    if (order_method == LayerOrdering::kByHeight) {
        // Retrieve the slicing plane normal
        QVector3D slicing_vector = {global_sb->setting<float>(PS::SlicingVector::kSlicingVectorX),
                                    global_sb->setting<float>(PS::SlicingVector::kSlicingVectorY),
                                    global_sb->setting<float>(PS::SlicingVector::kSlicingVectorZ)};
        slicing_vector.normalize();

        bool steps_left = true;
        int num_global_steps = 0;

        // Make a map to track the step/layer number each part is currently on
        // Start each part at zero
        QMap<QUuid, int> current_layer;
        for (auto& part : build_parts)
            current_layer.insert(part->getId(), 0);

        while (steps_left) {

            // Check the current step of all the parts, find the minimum plane
            QUuid part_with_min_plane = QUuid(); // null quuid initially
            Plane min_plane;
            Distance min_dist;

            // Check all the parts for the next "lowest" layer to print
            for (auto& part : build_parts) {
                QUuid part_id = part->getId();

                // Skip the part if all its layers have been assigned to a global layer
                if (current_layer[part_id] >= part->countStepPairs())
                    continue;

                // Get the current layer for this part
                QSharedPointer<Step> current_step = part->getStepPair(current_layer[part_id]).printing_layer;

                // calculate the distance from
                Plane layer_plane = current_step->getSlicingPlane();
                Distance layer_height = current_step->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
                layer_plane.shiftAlongNormal(layer_height() / 2.0);

                Distance layer_dist =
                    MathUtils::linePlaneIntersection(Point(0, 0, 0), slicing_vector, layer_plane).distance();

                // If this is the first plane in this loop, or its lower than the current min, set this layer as the min
                if (part_with_min_plane.isNull() || layer_dist < min_dist) {
                    part_with_min_plane = part_id;
                    min_plane = layer_plane;
                    min_dist = layer_dist;
                }
            }

            // check all the parts (again) for layers with the same plane as the lowest layer. Layers with same plane
            // can be printed at the same time, and go on the same global layer
            QSharedPointer<GlobalLayer> new_global_layer = QSharedPointer<GlobalLayer>::create(num_global_steps);
            for (auto& part : build_parts) {
                QUuid part_id = part->getId();

                // skip the part if all its layers have been assigned to a global layer
                if (current_layer[part_id] >= part->countStepPairs())
                    continue;

                // get the current layer for this part
                QSharedPointer<Step> current_step = part->getStepPair(current_layer[part_id]).printing_layer;

                Distance layer_height = current_step->getSb()->setting<Distance>(PS::Layer::kLayerHeight);
                Distance layer_grouping_tolerance =
                    global_sb->setting<Distance>(ES::PrinterConfig::kLayerGroupingTolerance);
                Plane layer_plane = current_step->getSlicingPlane();
                layer_plane.shiftAlongNormal(layer_height() / 2.0);

                // if this part's current layer is equal to the min plane, add it to the global layer
                if (layer_plane.isEqual(min_plane, layer_grouping_tolerance())) {
                    new_global_layer->addStepPair(part_id, part->getStepPair(current_layer[part_id]));
                    ++current_layer[part_id];
                }
            }

            // add the new global layer to the list
            global_layers.push_back(new_global_layer);
            ++num_global_steps;

            // update steps_left
            steps_left = false;
            for (auto& part : build_parts)
                steps_left = steps_left || (current_layer[part->getId()] < part->countStepPairs());

        } // end while(steps left)
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
