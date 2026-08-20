#pragma once

#include <QVector>

#include "geometry/point.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class kNN
 *
 * \brief Finds the K nearest reference points for each supplied query point.
 *
 * \note Points are compared in 3D space.
 */
class kNN {
  public:
    //! \brief Computes the kNN for a set of query points.
    kNN(const QVector<Point>& referencePoints, const QVector<Point>& queryPoints, int kNeighbors);

    //! Destructor
    ~kNN();

    //! \brief Executes the kNN calculation.
    void execute();

    //! \brief Returns the K nearest indices for each query point
    QVector<int> getNearestIndices();

    //! \brief Returns the K nearest distances for each query point
    QVector<Distance> getNearestDistances();

  protected:
    //! \brief A pointer to our array of reference points.
    float* m_referencePoints;

    //! \brief The number of reference points.
    int m_referencePointsSize;

    //! \brief A pointer to our array of query points.
    float* m_queryPoints;

    //! \brief The number of query points.
    int m_queryPointsSize;

    //! \brief The dimension of our points.
    int m_point_dimension;

    //! \brief K neighbors to compute.
    int m_kNeighbors;

    //! \brief A pointer to our array of output distances.
    float* m_knn_dist;

    //! \brief A pointer to our array of output indices.
    int* m_knn_index;

    //! \brief Computes an insertion sort.
    void modified_insertion_sort(float* dist, int* index, int length, int k);

    //! \brief Computes distances.
    float compute_distance(const float* ref, int ref_nb, const float* query, int query_nb, int dim, int ref_index,
                           int query_index);

    //! \brief The CPU implementation.
    bool knn_c(const float* ref, int ref_nb, const float* query, int query_nb, int dim, int k, float* knn_dist,
               int* knn_index);
};
} // namespace ORNL
