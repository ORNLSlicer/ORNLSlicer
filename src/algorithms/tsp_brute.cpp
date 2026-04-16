#include "algorithms/tsp_brute.h"

#include <cstdlib>
#include <limits>

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "algorithms/algorithm_base.h"
#include "algorithms/knn.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "step/layer/island/island_base.h"
#include "units/unit.h"

namespace ORNL {

TspBrute::TspBrute(QVector<QSharedPointer<IslandBase>> islands, int startIndex, bool shortest) : AlgorithmBase() {
    m_islands = islands;
    m_start_index = startIndex;
    m_shortest = shortest;
    m_number_of_islands = islands.size();

    m_x = static_cast<float*>(malloc(m_islands.size() * sizeof(float)));
    m_y = static_cast<float*>(malloc(m_islands.size() * sizeof(float)));
    m_optimized_path = static_cast<int*>(malloc(m_islands.size() * sizeof(int)));

    QVector<Point> centers;
    for (int i = 0; i < m_islands.size(); i++) {
        Point center = m_islands[i]->getGeometry().boundingRectCenter();
        centers.push_back(center);
        m_x[i] = center.x();
        m_y[i] = center.y();
    }

    kNN knn(centers, centers, 2);
    knn.execute();

    QVector<Distance> distances = knn.getNearestDistances();
    for (int i = m_islands.size(); i < distances.size(); i++) {
        if (distances[i] == 0.0) {
            m_same_center = true;
        }
    }
}

TspBrute::~TspBrute() {}

void TspBrute::executeCPU() {
    QVector<QSharedPointer<IslandBase>> optimized_island_path;
    QVector<QSharedPointer<IslandBase>> tmp_island_path;
    QVector<int> island_index_list;

    for (int i = 0; i < m_number_of_islands; ++i) {
        island_index_list += i;
    }

    Distance overall_extremum_distance;
    Distance tmp_accumulate_distance = Distance(0);
    if (m_shortest) {
        overall_extremum_distance = Distance(std::numeric_limits<float>::max());
    }
    else {
        overall_extremum_distance = Distance(0);
    }

    this->removeValue(island_index_list, m_start_index);

    tmp_island_path += m_islands[m_start_index];

    if (m_same_center) {
        tsp_brute_force_traverse(island_index_list, overall_extremum_distance, tmp_accumulate_distance,
                                 optimized_island_path, tmp_island_path, 0, 0, m_shortest);
    }
    else {
        tsp_brute_force_traverse(island_index_list, overall_extremum_distance, tmp_accumulate_distance,
                                 optimized_island_path, tmp_island_path, m_shortest);
    }
    m_optimized_islands = optimized_island_path;
}

//! \note Calculates to center of mass. Much faster than to calculate every vertex but does not work on concentric
//! islands
void TspBrute::tsp_brute_force_traverse(QVector<int> island_index_list, Distance& overall_extremum_distance,
                                        Distance tmp_accumulate_distance,
                                        QVector<QSharedPointer<IslandBase>>& optimized_island_path,
                                        QVector<QSharedPointer<IslandBase>> tmp_island_path, bool shortest) {

    if (island_index_list.empty()) {
        if (shortest) {
            if (tmp_accumulate_distance <= overall_extremum_distance) {
                overall_extremum_distance = tmp_accumulate_distance;
                optimized_island_path = tmp_island_path;
            }
        }
        else {
            if (tmp_accumulate_distance > overall_extremum_distance) {
                overall_extremum_distance = tmp_accumulate_distance;
                optimized_island_path = tmp_island_path;
            }
        }
    }
    else {
        for (int i = 0; i < island_index_list.size(); ++i) {
            Point previous_center = tmp_island_path.last()->getRegions().first()->getGeometry().boundingRectCenter();
            Point next_center =
                m_islands[island_index_list[i]]->getRegions().first()->getGeometry().boundingRectCenter();
            Distance next_tmp_accumulate_distance = tmp_accumulate_distance + previous_center.distance(next_center);

            QVector<int> next_island_list = island_index_list;
            next_island_list.remove(i);

            QVector<QSharedPointer<IslandBase>> next_tmp_island_path = tmp_island_path;
            next_tmp_island_path.push_back(m_islands[island_index_list[i]]);

            tsp_brute_force_traverse(next_island_list, overall_extremum_distance, next_tmp_accumulate_distance,
                                     optimized_island_path, next_tmp_island_path, shortest);
        }
    }
}

//! \note Calculates to every vertex. Much slower than center of mass but finds true shortest
void TspBrute::tsp_brute_force_traverse(QVector<int> island_index_list, Distance& overall_extremum_distance,
                                        Distance tmp_accumulate_distance,
                                        QVector<QSharedPointer<IslandBase>>& optimized_island_path,
                                        QVector<QSharedPointer<IslandBase>> tmp_island_path, int temp_lastVertexVisited,
                                        int lastVertexVisited, bool shortest) {
    if (island_index_list.empty()) {
        if (shortest) {
            if (tmp_accumulate_distance < overall_extremum_distance) {
                overall_extremum_distance = tmp_accumulate_distance;
                optimized_island_path = tmp_island_path;
                lastVertexVisited = temp_lastVertexVisited;
            }
        }
        else {
            if (tmp_accumulate_distance > overall_extremum_distance) {
                overall_extremum_distance = tmp_accumulate_distance;
                optimized_island_path = tmp_island_path;
                lastVertexVisited = temp_lastVertexVisited;
            }
        }
    }
    else {
        for (int i = 0; i < island_index_list.size(); ++i) {
            // IslandBases Stuff
            QVector<int> next_island_list = island_index_list;
            next_island_list.remove(i);
            QVector<QSharedPointer<IslandBase>> next_tmp_island_path = tmp_island_path;
            next_tmp_island_path.push_back(m_islands[island_index_list[i]]);

            // Path Segment Stuff
            Path previous_Innermost_path = tmp_island_path.last()->getRegions().first()->getPaths().last();

            // Find next outermost valid path
            Path next_Outermost_path;
            bool found_valid_path = false;
            auto& next_island_regions = m_islands[island_index_list[i]]->getRegions();
            for (int j = next_island_regions.size() - 1; i > 0; --i) {
                if (next_island_regions[j]->getPaths().size() > 0) {
                    next_Outermost_path = next_island_regions[j]->getPaths().first();
                    found_valid_path = true;
                    break;
                }
            }

            if (!found_valid_path) // There is no path in the next island, so skip it
                return;

            for (int j = 0; j < next_Outermost_path.size(); j++) {
                Distance next_tmp_accumulate_distance =
                    tmp_accumulate_distance +
                    previous_Innermost_path[lastVertexVisited]->end().distance(next_Outermost_path[j]->start());
                int temp_lastVertexVisited = j;
                tsp_brute_force_traverse(next_island_list, overall_extremum_distance, next_tmp_accumulate_distance,
                                         optimized_island_path, next_tmp_island_path, temp_lastVertexVisited,
                                         lastVertexVisited, shortest);
            }
        }
    }
}

void TspBrute::removeValue(QVector<int>& index_list, int value) {
    for (int i = 0; i < index_list.size(); ++i) {
        if (index_list[i] == value) {
            index_list.remove(i);
        }
    }
}

QVector<QSharedPointer<IslandBase>> TspBrute::getOptimizedIslandBases() { return m_optimized_islands; }

} // namespace ORNL
