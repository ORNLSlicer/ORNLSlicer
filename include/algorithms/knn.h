#pragma once

#include <cstdint>

#include <QVector>
#include <qcontainerfwd.h>

#include "algorithm_base.h"
#include "geometry/point.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class kNN
 *
 * \brief Provides access to both a CPU and GPU implementation of a K nearest neighbors algorithm. Takes in reference
 * points and and compares each supplied query point to find the K nearest neighbors. It is based off a CUDA kNN library
 * and includes support for using a GPU global memory, texture memory or the CUBLAS library. This class is built off the
 * Algorithm base class the handles GPU and CPU selection. An Algorithm constructor must be included when kNN is created
 * to specify the number of GPUs this algorithm uses.
 */
class kNN : public AlgorithmBase {
  public:
    //! \enum This selectes what mode the GPU will use when it is run;
    //!       - GLOBAL: Uses the GPU global memory to store data. This can be slower sometimes but is the most
    //!       consistent. This is the default method.
    //!       - TEXTURE: Uses the GPU texture memory to store reference points and global memory for everything else.
    //!       Using a texture usually speeds-up computations
    //!                  compared to global but due to the size constraint of the texture structures in CUDA, this
    //!                  implementation might not an option for some high dimensional problems.
    //!       - CUBLAS: Uses the Cuda Basic Linear Algebra Subprograms library. Computation of the distances are split
    //!       into several sub-problems better suited to a GPU acceleration.
    //!                 As a consequence, on some specific problems, this implementation may lead to a much faster
    //!                 processing time but may not give an optimal solution.
    enum class GPU_VARIANT : uint8_t {
        GLOBAL = 0,
        TEXTURE = 1,
        CUBLAS = 2,

    };

    //! \brief Computes the kNN for a set of query points.
    //! \note Points are compared in 3D space.
    //! \note A GPU_VARIANT can be supplied to set the GPU algorithm
    //! \note Defaults to GPU_VARIANT::GLOBAL
    kNN(QVector<Point> referencePoints, QVector<Point> queryPoints, int kNeighbors,
        GPU_VARIANT variant = GPU_VARIANT::GLOBAL);

    //! Destructor
    ~kNN();

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

    //! \brief The GPU_VARIANT to run on.
    GPU_VARIANT m_variant;

    //! \brief Holds the CPU implementation.
    void executeCPU();

    //! \brief Computes an insertion sort. Used by the CPU implementation.
    void modified_insertion_sort(float* dist, int* index, int length, int k);

    //! \brief Computes distances. Used by the CPU implementation.
    float compute_distance(const float* ref, int ref_nb, const float* query, int query_nb, int dim, int ref_index,
                           int query_index);

    //! \brief The CPU implementation.
    bool knn_c(const float* ref, int ref_nb, const float* query, int query_nb, int dim, int k, float* knn_dist,
               int* knn_index);
};
} // namespace ORNL
