#pragma once

#include <QLinkedList>
#include <qcontainerfwd.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "step/layer/layer.h"

namespace ORNL {
class IslandBase;

class PowderLayer : public Layer {
  public:
    //! \brief Constructor
    PowderLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb);

    //! \brief Writes the code for the layer.
    QString writeGCode(QSharedPointer<WriterBase> writer) override;

    //! \brief Computes the layer.
    void compute() override;

    //! \brief Set the layer's island order to default island order.
    //! Necessary for RPBF slicer as island order isn't set as part of its post-processing.  There are
    //! no travels so no order is derived.
    void setIslandOrder(QVector<QVector<QSharedPointer<IslandBase>>> island_order);

  private:
    //! \brief island order separated into sectors
    QVector<QVector<QSharedPointer<IslandBase>>> m_island_order;
};
} // namespace ORNL
