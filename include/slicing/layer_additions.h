#pragma once

#include <qdir.h>
#include <qsharedpointer.h>

#include "part/part.h"
#include "step/layer/layer.h"

namespace ORNL {
//! \brief Reusable functions that add things to existing layers or use existing layers to create new ones
namespace LayerAdditions {
//! \brief creates a raft for a given layer
//! \param layer: the layer to create a raft from
//! \return a new raft layer to be place below supplied layer
QSharedPointer<Layer> createRaft(QSharedPointer<Layer> layer);

//! \brief adds a brim to a layer
//! \param layer: the layer to add a brim to
void addBrim(QSharedPointer<Layer> layer);

//! \brief adds a skirt to a layer
//! \param layer: the layer to add a skirt to
void addSkirt(QSharedPointer<Layer> layer);

//! \brief adds a thermal scan to a layer
//! \param layer: the layer to add a thermal scan to
void addThermalScan(QSharedPointer<Layer> layer);

//! \brief adds a laser scan layer on a part for a layer
//! \param part: the part to add to
//! \param layer_index: the index of the layer to add at
//! \param running_total: the total number of layers
//! \param build_layer: the build layer
//! \param output_path: the ouput dir of the scan file
void addLaserScan(QSharedPointer<Part> part, int layer_index, double running_total, QSharedPointer<Step> build_layer,
                  QDir output_path);
}  // namespace LayerAdditions
}  // namespace ORNL
