#pragma once

#include <QVector>
#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "algorithm_base.h"
#include "step/layer/island/island_base.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class TspBrute
 *
 * \brief Provides access to both CPU and GPU implementation of a brute-force traveling salesman problem solver. Takes
 * in islands and uses their center of mass as coordinates to find the shortest circuit. Since we pick our starting
 * island and do not have to return to it, this reduces to number of permutations form N! to (N-1)!. The CPU
 * implementation uses recursion to find the shortest path. The GPU implementation uses factoradics and parallel
 * reduction to compute the shortest path. When benchmarked the CPU implementation faster for N up to 5 before it starts
 * to become much slower. At 11 islands, the GPU implementation runs around 72 times faster than the CPU. This class is
 * built off the Algorithm base class the handles GPU and CPU selection. An Algorithm constructor must be included when
 * TspBrute is created to specify the number of GPUs this algorithm uses(currently 1).
 */
class TspBrute : public AlgorithmBase {
  public:
    //! \brief Computes the optimal path(shortest or longest) to visit all islands in a list.
    //! \note IslandBases are represented by their center of mass
    //! \note Can compute either shortest of longest path
    //! \note This is only computed in 2D
    TspBrute(QVector<QSharedPointer<IslandBase>> islands, int startIndex, bool shortest);

    //! Destructor
    ~TspBrute();

    //! \brief Returns an order list of optimized islands
    QVector<QSharedPointer<IslandBase>> getOptimizedIslandBases();

  protected:
    //! \brief Pointer to an array of optimized island indexs
    int* m_optimized_path;

    //! \brief Pointer to an array x coordinates
    float* m_x;

    //! \brief Pointer to an array y coordinates
    float* m_y;

    //! \brief The number of islands to order
    int m_number_of_islands;

    //! \brief Our start index
    int m_start_index;

    //! \brief If we are computing the largest or shortest distance
    bool m_shortest;

    //! \brief If any of our islands have the same center point
    bool m_same_center = false;

    //! \brief A vector of optimized islands
    QVector<QSharedPointer<IslandBase>> m_optimized_islands;

    //! \brief A vector of un-optimized islands
    QVector<QSharedPointer<IslandBase>> m_islands;

    //! \brief Holds the CPU implementation.
    void executeCPU();

    /*! \brief Called by computeExtremunDistance, recursively tries all possibilities of visiting orders,
     *         and gives the shortest/longest one
     *
     *  \note The distances are the distances between polygons' centers
     *
     *  \param island_index_list: the list of indice of islands to be traversed
     *  \param overall_extremum_distance: the optimal total traveling distance
     *  \param tmp_accumulate_distance: current (temporary) total traveling distance during recursion process
     *  \param optimized_island_path: the optimal island path order, i.e. the result
     *  \param tmp_island_path: current (temporary) island path during recursion process
     *  \param shortest: if true, computes for the shortest distance, else longest.
     */
    void tsp_brute_force_traverse(QVector<int> island_index_list, Distance& overall_extremum_distance,
                                  Distance tmp_accumulate_distance,
                                  QVector<QSharedPointer<IslandBase>>& optimized_island_path,
                                  QVector<QSharedPointer<IslandBase>> tmp_island_path, bool shortest);

    /*! \brief Compute by using each vertex instead of center of mass
     *  \param lastVertexVisited: the last vertex we visited on the last island
     */
    void tsp_brute_force_traverse(QVector<int> island_index_list, Distance& overall_extremum_distance,
                                  Distance tmp_accumulate_distance,
                                  QVector<QSharedPointer<IslandBase>>& optimized_island_path,
                                  QVector<QSharedPointer<IslandBase>> tmp_island_path, int temp_lastVertexVisited,
                                  int lastVertexVisited, bool shortest);

    //! \brief Remove the element with specified value from a vector
    void removeValue(QVector<int>& index_list, int value);
};
} // namespace ORNL
