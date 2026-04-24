
#include "optimizers/island_order_optimizer.h"

#include <algorithm>
#include <cassert>
#include <limits>

#include <QRandomGenerator>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qmap.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <quuid.h>

#include "algorithms/tsp_brute.h"
#include "geometry/path.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "step/layer/island/island_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {

IslandBaseOrderOptimizer::IslandBaseOrderOptimizer(Point& current_positon,
                                                   QList<QSharedPointer<IslandBase>> island_list,
                                                   int last_island_visited, IslandOrderOptimization order_optimization)
    : m_last_island_visited(last_island_visited), m_start(current_positon), m_order_optimization(order_optimization),
      m_first_index(-1) {
    m_island_list = island_list;
    m_was_first_selected = false;
}

IslandBaseOrderOptimizer::IslandBaseOrderOptimizer(Point current_positon,
                                                   QHash<QSharedPointer<IslandBase>, QUuid> part_island_list,
                                                   IslandOrderOptimization order_optimization)
    : m_start(current_positon), m_order_optimization(order_optimization) {
    m_part_island_list = part_island_list;
    m_island_list = m_part_island_list.keys();
}

void IslandBaseOrderOptimizer::setStartPoint(Point start_point) { m_start = start_point; }

void IslandBaseOrderOptimizer::setIslands(QList<QSharedPointer<IslandBase>> island_list) {
    m_island_list = island_list;
}

void IslandBaseOrderOptimizer::setOrderOptimization(IslandOrderOptimization order_optimization) {
    m_order_optimization = order_optimization;
}

int IslandBaseOrderOptimizer::getLastIslandBaseVisited() { return m_last_island_visited; }

int IslandBaseOrderOptimizer::getFirstIndexSelected() { return m_first_index; }

int IslandBaseOrderOptimizer::computeNextIndex() {
    //! \note No need to optimize if we only have a single island
    if (m_island_list.size() < 2)
        return 0;

    int index;
    switch (m_order_optimization) {
        //! Brute force approach for Traveling Salesman Problem
        case IslandOrderOptimization::kShortestDistanceBrute:
            index = this->computeExtremumDistance(true);
            break;

        //! Keep finding the closest island to current one
        case IslandOrderOptimization::kNextClosest:
            index = this->computeNextClosest();
            break;

        //! Brute force approach for Traveling Salesman Problem (but finding the largest distance)
        case IslandOrderOptimization::kNextFarthest:
            index = this->computeExtremumDistance(false);
            break;

        //! An approximate algorithm (Christofides) for Traveling Salesman Problem, much faster than brute force & DP
        case IslandOrderOptimization::kShortestDistanceApprox:
            index = this->computeApproximate();
            break;

        //! Randomizes the order on each layer
        case IslandOrderOptimization::kRandom:
            index = this->computeRandom();
            break;

        //! Visits the least most recently visited island
        case IslandOrderOptimization::kLeastRecentlyVisited:
            index = this->computeLeastRecentlyVisited();
            break;

        //! Use next closest if a method is not supported
        default:
            index = this->computeNextClosest();
            break;
    }

    m_island_list.removeAt(index);

    return index;
}

QList<QUuid> IslandBaseOrderOptimizer::computePartOrder() {
    QList<QUuid> result;

    while (m_part_island_list.size() > 0) {
        // make a copy of the island list to get reference to island from result of computeNext()
        QList<QSharedPointer<IslandBase>> islandSave = m_island_list;
        int index = computeNextIndex();
        QSharedPointer<IslandBase> next_island = islandSave[index];

        // The next part is the one who owns the next island. Save the next part to the results list
        QUuid next_islands_part_id = m_part_island_list[next_island];
        result.append(next_islands_part_id);

        // if there are still islands remaining, remove the islands who belong to the part that was just added to
        // results list
        if (m_part_island_list.size() > 0) {
            for (auto it = m_part_island_list.begin(), end = m_part_island_list.end(); it != end;) {
                if (it.value() == next_islands_part_id)
                    it = m_part_island_list.erase(it);
                else
                    ++it;
            }
            m_island_list = m_part_island_list.keys();
        }

        // if there is only one part left, add it to the results list and stop looking for next_islands
        QList<QUuid> vals = m_part_island_list.values();
        std::sort(vals.begin(), vals.end());
        std::unique(vals.begin(), vals.end());
        if (vals.size() == 1) {
            result.append(vals[0]);
            break;
        }
    }
    return result;
}

int IslandBaseOrderOptimizer::computeRandom() { return QRandomGenerator::global()->bounded(m_island_list.size()); }

int IslandBaseOrderOptimizer::computeLeastRecentlyVisited() {
    if (m_last_island_visited == -1)
        m_last_island_visited = this->computeNextClosest();
    else
        m_last_island_visited = m_last_island_visited % m_island_list.size();

    if (!m_was_first_selected) {
        m_first_index = m_last_island_visited;
        m_was_first_selected = true;
    }

    return m_last_island_visited;
}

int IslandBaseOrderOptimizer::extremumIslandBase(Point start_point, bool closest) {
    Distance start_point_distance = closest ? Distance(std::numeric_limits<float>::max()) : Distance(0);
    int extremum_island_index = 0;
    for (int i = 0, end = m_island_list.size(); i < end; ++i) {
        QSharedPointer<IslandBase> isl = m_island_list[i];

        Polygon polygon = isl->getGeometry()[0];
        for (Point point : polygon) {
            Distance dis = point.distance(start_point);
            if (closest) {
                if (dis < start_point_distance) {
                    start_point_distance = dis;
                    extremum_island_index = i;
                    m_start = point;
                }
            }
            else {
                if (dis > start_point_distance) {
                    start_point_distance = dis;
                    extremum_island_index = i;
                    m_start = point;
                }
            }
        }
    }
    return extremum_island_index;
}

int IslandBaseOrderOptimizer::computeExtremumDistance(bool shortest) {
    int first_island_index;
    if (shortest) {
        first_island_index = this->extremumIslandBase(m_start, true);
    }
    else {
        first_island_index = this->extremumIslandBase(m_start, false);
    }

    // Only compute once. Use stored result otherwise
    if (m_tsp_result.empty()) {
        TspBrute tsp(m_island_list.toVector(), first_island_index, shortest);
        tsp.execute();
        m_tsp_result = tsp.getOptimizedIslandBases();
    }

    if (m_tsp_result.size() <= 0)
        return 0;

    for (int i = 0, end = m_island_list.size(); i < end; ++i) {

        if (m_island_list[i] == m_tsp_result[0]) {
            m_tsp_result.removeFirst();
            return i;
        }
    }
    return 0;
}

void IslandBaseOrderOptimizer::removeValue(QVector<int>& index_list, int value) {
    for (int i = 0, end = index_list.size(); i < end; ++i) {
        if (index_list[i] == value) {
            index_list.remove(i);
        }
    }
}

int IslandBaseOrderOptimizer::computeNextClosest() {
    //! \note Start with the polygon that is closest to starting point
    int first_island_index = this->extremumIslandBase(m_start, true);
    return first_island_index;
}

int IslandBaseOrderOptimizer::computeApproximate() {
    QVector<Point> center_list;
    for (QSharedPointer<IslandBase>& island : m_island_list) {
        center_list += island.data()->getGeometry()[0].boundingRectCenter();
    }

    //! \note Find the minimal spanning tree, represented with an adjacency matrix
    QVector<QVector<int>> minimal_spanning_tree = minimalSpanningTree(center_list);

    //! \note Find all the odd degree vertice
    QVector<int> odd_degree_points_indice;
    for (int i = 0, end = m_island_list.size(); i < end; ++i) {
        int degree = 0;
        for (int& value : minimal_spanning_tree[i]) {
            if (value) {
                ++degree;
            }
        }
        if (degree % 2 == 1) {
            odd_degree_points_indice += i;
        }
    }

    //! \note Find the minimal weight perfect matching for odd degree vertice
    QVector<QPair<int, int>> minimal_matching = minimalMatching(odd_degree_points_indice, center_list);

    //! \note Unite the minimal spanning tree with the minimal matching of odd degree vertice
    QVector<QVector<int>> united_graph = minimal_spanning_tree;
    for (QPair<int, int>& pair : minimal_matching) {
        ++united_graph[pair.first][pair.second];
        ++united_graph[pair.second][pair.first];
    }

    //! \note Find the shortcut Eulerian tour on the united graph, which is an (approximate) optimal Hamilton cycle
    QVector<int> optimal_island_order_indice = shortcutEulerianTour(united_graph);
    assert(optimal_island_order_indice.size() == united_graph.size());

    //! \note Find the first island and adjust the order of the optimal island order indice
    int first_island_index = this->extremumIslandBase(m_start, true);
    while (optimal_island_order_indice[0] != first_island_index) {
        optimal_island_order_indice += optimal_island_order_indice[0];
        optimal_island_order_indice.pop_front();
    }

    return optimal_island_order_indice[0];
}

QVector<QVector<int>> IslandBaseOrderOptimizer::minimalSpanningTree(QVector<Point> vertice) {
    QVector<QVector<int>> minimal_spanning_tree(vertice.size());
    for (QVector<int>& vertex : minimal_spanning_tree) {
        vertex.fill(0, vertice.size());
    }

    //! \note Vector of all the edges, each edge is in format {length, {vertex_1, vertex_2}}
    QVector<QPair<Distance, QPair<int, int>>> edges;
    for (int i = 0, end = vertice.size(); i < end; ++i) {
        for (int j = 0; j < i; ++j) {
            edges += {vertice[i].distance(vertice[j]), {i, j}};
        }
    }
    std::sort(edges.begin(), edges.end());

    //! \note Keep picking current shortest edge, if it forms a circle, drop it (Kruskal algorithm)
    QVector<QSet<int>> chosen_vertice;
    while (!(chosen_vertice.size() == 1 && chosen_vertice.first().size() == vertice.size())) {
        int first_vertex_index = edges.first().second.first;
        int second_vertex_index = edges.first().second.second;
        int set_num_first = findSet(chosen_vertice, first_vertex_index);
        int set_num_second = findSet(chosen_vertice, second_vertex_index);
        bool not_cycle = false;
        if (set_num_first != set_num_second) {
            not_cycle = true;
            if (set_num_first == -1 && set_num_second != -1) {
                chosen_vertice[set_num_second] += first_vertex_index;
            }
            else if (set_num_second == -1 && set_num_first != -1) {
                chosen_vertice[set_num_first] += second_vertex_index;
            }
            else {
                chosen_vertice[set_num_first].unite(chosen_vertice[set_num_second]);
                chosen_vertice.remove(set_num_second);
            }
        }
        if (set_num_first == -1 && set_num_second == -1) {
            chosen_vertice += {first_vertex_index, second_vertex_index};
            not_cycle = true;
        }
        if (not_cycle) {
            minimal_spanning_tree[first_vertex_index][second_vertex_index] = 1;
            minimal_spanning_tree[second_vertex_index][first_vertex_index] = 1;
        }

        edges.pop_front();
    }
    return minimal_spanning_tree;
}

int IslandBaseOrderOptimizer::findSet(QVector<QSet<int>> chosen_vertice, int vertex_index) {
    int set_num = -1;
    bool found = false;
    if (!chosen_vertice.empty()) {
        for (int i = 0, end = chosen_vertice.size(); i < end && !found; ++i) {
            for (int vertex : chosen_vertice[i]) {
                if (vertex == vertex_index) {
                    set_num = i;
                    found = true;
                    break;
                }
            }
        }
    }
    return set_num;
}

QVector<QPair<int, int>> IslandBaseOrderOptimizer::minimalMatching(QVector<int> odd_degree_points_indice,
                                                                   QVector<Point> center_list) {
    //! \note Vector of all ordered edges, along with the indice of two vertice
    QVector<QPair<Distance, QPair<int, int>>> distance_ordered_edges;
    for (int i = 0, end = odd_degree_points_indice.size() - 1; i < end; ++i) {
        for (int j = i + 1, end = odd_degree_points_indice.size(); j < end; ++j) {
            int index_0 = odd_degree_points_indice[i];
            int index_1 = odd_degree_points_indice[j];
            Distance distance = center_list[index_0].distance(center_list[index_1]);
            distance_ordered_edges += {distance, {index_0, index_1}};
        }
    }
    std::sort(distance_ordered_edges.begin(), distance_ordered_edges.end());

    //! \note Keep picking the shortest edge
    QVector<QPair<int, int>> minimal_matching;
    while (!distance_ordered_edges.empty()) {
        QPair<int, int> match_pair = distance_ordered_edges.first().second;
        minimal_matching += match_pair;
        //! \note Size must be evaluated every iteration
        for (int i = 0; i < distance_ordered_edges.size();) {
            if (distance_ordered_edges[i].second.first == match_pair.first ||
                distance_ordered_edges[i].second.first == match_pair.second ||
                distance_ordered_edges[i].second.second == match_pair.first ||
                distance_ordered_edges[i].second.second == match_pair.second) {
                distance_ordered_edges.remove(i);
            }
            else {
                ++i;
            }
        }
    }
    return minimal_matching;
}

QVector<int> IslandBaseOrderOptimizer::shortcutEulerianTour(QVector<QVector<int>> graph) {
    QVector<int> Eulerian_tour;

    int current_vertex_index = 0;
    int graph_size = graph.size();

    QVector<bool> visited;
    visited.fill(false, graph_size);

    bool found_next = true;
    while (found_next) {
        Eulerian_tour += current_vertex_index;
        found_next = false;

        int degree = 0;
        int tmp_next;
        for (int i = 0; i < graph_size; ++i) {
            if (graph[current_vertex_index][i] > 0) {
                ++degree;
                tmp_next = i;
            }
        }

        if (degree == 1) {
            --graph[current_vertex_index][tmp_next];
            --graph[tmp_next][current_vertex_index];
            current_vertex_index = tmp_next;
            found_next = true;
        }
        else {
            for (int i = 0; i < graph_size; ++i) {
                if (graph[current_vertex_index][i] > 0) {
                    visited.fill(false);
                    int reachable_before_remove = DFSCount(current_vertex_index, graph, visited);

                    //! \note Modify the graph and check reachable vetice number after visiting current edge
                    --graph[current_vertex_index][i];
                    --graph[i][current_vertex_index];
                    visited.fill(false);
                    int reachable_after_remove = DFSCount(current_vertex_index, graph, visited);
                    if (reachable_after_remove == reachable_before_remove) {
                        //! \note This is not a bridge, visit this edge
                        current_vertex_index = i;
                        found_next = true;
                        break;
                    }
                    else {
                        //! \note This is a bridge, don't visit it, recover the graph
                        ++graph[current_vertex_index][i];
                        ++graph[i][current_vertex_index];
                    }
                }
            }
        }
    }

    //! \note Shortcut
    visited.fill(false);
    //! \note Size must be evaluated every iteration
    for (int i = 0; i < Eulerian_tour.size();) {
        if (!visited[Eulerian_tour[i]]) {
            visited[Eulerian_tour[i]] = true;
            ++i;
        }
        else {
            Eulerian_tour.remove(i);
        }
    }

    return Eulerian_tour;
}

int IslandBaseOrderOptimizer::DFSCount(int start_vertex, QVector<QVector<int>> graph, QVector<bool>& visited) {
    int count = 1;
    visited[start_vertex] = true;
    for (int i = 0, end = graph.size(); i < end; ++i) {
        if (graph[start_vertex][i] > 0 && !visited[i]) {
            count += DFSCount(i, graph, visited);
        }
    }
    return count;
}

QSet<QSet<int>> IslandBaseOrderOptimizer::chooseSet(int size, int first_island_index, QSet<int> set) {
    QSet<QSet<int>> subsets;
    QSet<int> recurse_subset;

    set -= first_island_index;
    recurse_subset += first_island_index;

    recursiveChoose(size - 1, set, recurse_subset, subsets);

    return subsets;
}

void IslandBaseOrderOptimizer::recursiveChoose(int size, QSet<int> set, QSet<int> recurse_subset,
                                               QSet<QSet<int>>& subsets) {
    if (size <= set.size()) {
        if (size == 0) {
            subsets += recurse_subset;
        }
        else {
            QSet<int> this_set = set;
            for (int value : this_set) {
                set -= value;
                QSet<int> next_recurse_subset = recurse_subset;
                next_recurse_subset += value;
                recursiveChoose(size - 1, set, next_recurse_subset, subsets);
            }
        }
    }
}

int IslandBaseOrderOptimizer::minDistanceLastPoint(QHash<QSet<int>, QMap<int, Distance>> DP_Distance_Map,
                                                   QSet<int> last_set, int ending_point_index) {
    int min_distance_last_point;

    Distance shortestDistance = Distance(std::numeric_limits<float>::max());
    for (int last_point : last_set) {
        if (DP_Distance_Map[last_set][last_point] != Distance(std::numeric_limits<float>::max())) {
            Distance current_distance =
                DP_Distance_Map[last_set][last_point] +
                m_island_list[last_point].data()->getGeometry()[0].boundingRectCenter().distance(
                    m_island_list[ending_point_index].data()->getGeometry()[0].boundingRectCenter());
            if (current_distance < shortestDistance) {
                shortestDistance = current_distance;
                min_distance_last_point = last_point;
            }
        }
    }

    return min_distance_last_point;
}

} // namespace ORNL
