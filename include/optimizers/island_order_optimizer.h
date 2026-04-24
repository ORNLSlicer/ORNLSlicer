#pragma once

#include <QUuid>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qmap.h>
#include <qset.h>
#include <qsharedpointer.h>

#include "geometry/point.h"
#include "step/layer/island/island_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
class IslandBaseOrderOptimizer {
  public:
    //! \brief (Mode 1) Used to order islands in an order according to an optimization method
    //! \param current_position Point that represents current position, altered interally
    //! \param island_list List of islands to consider
    //! \param last_island_visited Index of last island visited (used for specific optimization strategies)
    //! \param order_optimization Selected optimization strategy
    IslandBaseOrderOptimizer(Point& current_positon, QList<QSharedPointer<IslandBase>> island_list,
                             int last_island_visited,
                             IslandOrderOptimization order_optimization = IslandOrderOptimization::kNextClosest);

    //! \brief (Mode 2) Used to order parts based on island list and optimization method
    //! \param current_position Point that represents current position, altered interally
    //! \param part_island_list Hash of islands linked to specific part ids
    //! \param order_optimization Selected optimization strategy
    IslandBaseOrderOptimizer(Point current_positon, QHash<QSharedPointer<IslandBase>, QUuid> part_island_list,
                             IslandOrderOptimization order_optimization);

    //! \brief Compute next island index based on perviously set optimization parameters
    //! \return Next index
    int computeNextIndex();

    //! \brief (Mode 2) Compute order for entire part list based on islands and perviously set optimization parameters
    //! \return Order list of part ids
    QList<QUuid> computePartOrder();

    //! \brief Sets start point
    //! \param start_point New start point to set
    void setStartPoint(Point start_point);

    //! \brief Compute next island index based on perviously set optimization parameters
    //! \return island_list New island list to set
    void setIslands(QList<QSharedPointer<IslandBase>> island_list);

    //! \brief Sets default optimization method
    //! \param order_optimization New optimization method to set
    void setOrderOptimization(IslandOrderOptimization order_optimization);

    //! \brief Returns the index for the last island visited
    //! \return Index for last island visited
    int getLastIslandBaseVisited();

    //! \brief Returns the index for the last island visited
    //! \return Index for last island visited
    int getFirstIndexSelected();

  private:
    //! \brief Optimization information about last island visited in previous layers. Used only in the
    //! least recently used optimization mode.
    int m_last_island_visited;

    //! \brief Used to track first selected island for least recently used optimization mode
    bool m_was_first_selected;
    int m_first_index;

    //! \brief Starting point
    Point m_start;

    //! \brief All the islands based on paths
    QList<QSharedPointer<IslandBase>> m_island_list;

    //! \brief All the islands mapped to specific parts.
    QHash<QSharedPointer<IslandBase>, QUuid> m_part_island_list;

    //! \brief Optimization method to be used, if not specified
    IslandOrderOptimization m_order_optimization;

    //! \brief the order computed by the TSP solver
    QVector<QSharedPointer<IslandBase>> m_tsp_result;

    //! \brief Remove the element with specified value from a vector
    //! \param index_list Vector of indicies
    //! \param value Value to remove from vector
    void removeValue(QVector<int>& index_list, int value);

    /*! \brief Computes a random island order
     *  \return Index of next island
     *  \note Each layer has a randomized visiting order
     */
    int computeRandom();

    //! \brief Computes a island visiting order based on which one was visted least reacently visited.
    //! \return Index of next island
    int computeLeastRecentlyVisited();

    /*! \brief Computes the island order which gives shortest/longest total distance, using brute force approach
     *  \return Index of next island
     *  \note 1. Represent each island with its center point or if islands are concentric to each other use vertex.
     *        2. If shortest = true, compute the shortest distance; else the longest.
     */
    int computeExtremumDistance(bool shortest);

    /*! \brief Computes the island order with following logic:
     *          Each time move to the nearest polygon to current position
     *  \return Index of next island
     */
    int computeNextClosest();

    /*! \brief The index of the island that has the nearest end point from a start point
     *  \return Index of next island
     *  \param start_point Starting point to consider for calculation
     *  \param closest Boolean to control closest or further calculation
     *  \note If closest = true, compute for the closest island; else furthest.
     */
    int extremumIslandBase(Point start_point, bool closest);

    /*! \brief Returns the TSP solution using an approximate algorithm, much faster than brute force (O(n3) complexity).
     *  \return Index of next island
     *  \note Uses Christofides algorithm, which gives a result with at most 3/2 of the optimal length
     */
    int computeApproximate();

    /*! \brief With a list of vertices, returns a minimal spanning tree
     *  \param vertice Vector of vertices to consider
     *  \note Kruskal algorithm
     *  \return Minimum spanning tree
     */
    QVector<QVector<int>> minimalSpanningTree(QVector<Point> vertice);

    //! \brief For minimalSpanningTree, finds the set number of a vertice; if it doesn't belong to any set, returns -1
    //! \param chosen_vertice Set of vertices to consider
    //! \param vertex index Index of interest
    //! \return set number of vertex_index
    int findSet(QVector<QSet<int>> chosen_vertice, int vertex_index);

    /*! \brief With a list of odd degree vertices, returns a minimal cost matching
     *  \param odd_degree_points_indice Vector of odd degree points
     *  \param center_list Center list
     *  \note Approximate algorithm, greedy. Performance ratio is 2
     *  \return Vector of minimal cost matches among indices
     */
    QVector<QPair<int, int>> minimalMatching(QVector<int> odd_degree_points_indice, QVector<Point> center_list);

    /*! \brief The last process of Christofides algorithm, finds the shortcut Eulerian path
     *  \param graph Graph to consider for tour
     *  \note Finds the Eulerian cycle with Fleury's Algorithm
     *  \param Euler tour of supplied graph
     */
    QVector<int> shortcutEulerianTour(QVector<QVector<int>> graph);

    //! \brief Depth-first search and counts reachable vertice number from start_vertex
    //! \param start_vertex Vertex to being at
    //! \param graph Graph to consider
    //! \param visited Whether or not a particular vertex in the graph has been visited
    //! \return Count of reached vertices from the given start vertex
    int DFSCount(int start_vertex, QVector<QVector<int>> graph, QVector<bool>& visited);

    //! \brief Returns all the subsets of "set" with specific size, and which must consist first_island_index
    //! \param size Size to calculate
    //! \param first_island_index Index that set must contain
    //! \param set Set to consider
    //! \return Set of subsets that adhere to size and index criteria
    QSet<QSet<int>> chooseSet(int size, int first_island_index, QSet<int> set);

    //! \brief Recurse process for chooseSet() function
    //! \param size Size to consider
    //! \param set Set to consider
    //! \param recurse_subset Current subset under evaluation
    //! \param subset Subsets to return
    void recursiveChoose(int size, QSet<int> set, QSet<int> recurse_subset, QSet<QSet<int>>& subsets);

    //! \brief Returns the index of the last point, given current point's index, which makes the total distance shortest
    //! \param DP_Distance_Map Map to consider
    //! \param point_set Current point set
    //! \param ending_point_index Index to consider
    //! \return Index of last point that minimizes total distance of input params
    int minDistanceLastPoint(QHash<QSet<int>, QMap<int, Distance>> DP_Distance_Map, QSet<int> point_set,
                             int ending_point_index);
};
} // namespace ORNL
