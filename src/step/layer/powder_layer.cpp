#include "step/layer/powder_layer.h"

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "step/layer/island/powder_sector_island.h"
#include "step/layer/layer.h"
#include "utilities/enums.h"

namespace ORNL {
PowderLayer::PowderLayer(uint layer_nr, const QSharedPointer<SettingsBase>& sb) : Layer(layer_nr, sb) {
    m_type = StepType::kLayer;
}

QString PowderLayer::writeGCode(QSharedPointer<WriterBase> writer) {
    QString gcode;

    for (int i = 0, end = m_island_order.size(); i < end; ++i) {
        QVector<QSharedPointer<IslandBase>> sector = m_island_order[i];
        bool anyPaths = false;
        for (QSharedPointer<IslandBase> island : sector) {
            if (island->getAnyValidPaths())
                anyPaths = true;
        }
        if (anyPaths) {
            for (QSharedPointer<IslandBase> island : sector)
                gcode += island->writeGCode(writer);

            if (i < end - 1)
                gcode += writer->writeAfterRegion(RegionType::kUnknown);
        }
        else {
            gcode += writer->writeEmptyStep();

            if (i < end - 1)
                gcode += writer->writeAfterRegion(RegionType::kUnknown);
        }
    }
    return gcode;
}

void PowderLayer::compute() {
    for (QSharedPointer<IslandBase> island : m_islands) {
        island->compute(m_layer_nr, m_sync);

        island.dynamicCast<PowderSectorIsland>()->reorderRegions();
    }
}

void PowderLayer::setIslandOrder(QVector<QVector<QSharedPointer<IslandBase>>> island_order) {
    m_island_order = island_order;
}
} // namespace ORNL
