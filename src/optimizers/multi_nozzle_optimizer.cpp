#include "optimizers/multi_nozzle_optimizer.h"

#include <algorithm>
#include <cassert>

#include <qcontainerfwd.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "step/layer/island/island_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {

void MultiNozzleOptimizer::assignByArea(QVector<QSharedPointer<IslandBase>>& islands, int nozzle_count) {
    int num_islands = islands.size();
    assert(num_islands > 0);

    std::sort(islands.begin(), islands.end(), islandAreaLessThan);

    QVector<float> extruder_areas;
    for (int i = 0; i < nozzle_count; ++i)
        extruder_areas.push_back(0.0);

    // for each island, from biggest to smallest
    //  assign it to the extruder with the least area so far
    for (int i = islands.size() - 1; i >= 0; --i) {
        int min_ext = 0;
        for (int j = 1; j < nozzle_count; ++j) {
            if (extruder_areas[j] < extruder_areas[min_ext])
                min_ext = j;
        }

        islands[i]->setExtruder(min_ext);
        auto geometry = islands[i]->getGeometry();
        extruder_areas[min_ext] += geometry.totalArea()();
    }
}

void MultiNozzleOptimizer::assignByAxisLocation(QVector<QSharedPointer<IslandBase>>& islands, int nozzle_count,
                                                Axis axis) {
    int num_islands = islands.size();
    assert(num_islands > 0);

    if (axis == Axis::kX)
        std::sort(islands.begin(), islands.end(), islandLocationXLessThan);
    else if (axis == Axis::kY)
        std::sort(islands.begin(), islands.end(), islandLocationYLessThan);
    else
        assert(false);

    int std_sect_size = num_islands / nozzle_count;
    int remain = num_islands % nozzle_count;

    int last = 0;
    for (int i = 0; i < nozzle_count; ++i) {
        int boundry = last + std_sect_size;
        if (i < remain)
            ++boundry;

        for (int j = last; j < boundry; ++j)
            islands[j]->setExtruder(i);

        last = boundry;
    }
}

bool MultiNozzleOptimizer::islandLocationXLessThan(const QSharedPointer<IslandBase>& island_1,
                                                   const QSharedPointer<IslandBase>& island_2) {
    QRect bounding_box_1 = island_1->getGeometry().boundingRect();
    QRect bounding_box_2 = island_2->getGeometry().boundingRect();

    return bounding_box_1.left() < bounding_box_2.left();
}

bool MultiNozzleOptimizer::islandLocationYLessThan(const QSharedPointer<IslandBase>& island_1,
                                                   const QSharedPointer<IslandBase>& island_2) {
    QRect bounding_box_1 = island_1->getGeometry().boundingRect();
    QRect bounding_box_2 = island_2->getGeometry().boundingRect();

    return bounding_box_1.bottom() < bounding_box_2.bottom();
}

bool MultiNozzleOptimizer::islandAreaLessThan(const QSharedPointer<IslandBase>& island_1,
                                              const QSharedPointer<IslandBase>& island_2) {
    auto geometry_1 = island_1->getGeometry();
    auto geometry_2 = island_2->getGeometry();
    Area area_1 = geometry_1.totalArea();
    Area area_2 = geometry_2.totalArea();

    return area_1() < area_2();
}
} // namespace ORNL
