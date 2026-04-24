#include "algorithms/knn.h"

#include <algorithm>
#include <cstdlib>

#include <qcontainerfwd.h>
#include <qmath.h>

#include "algorithms/algorithm_base.h"
#include "geometry/point.h"
#include "units/unit.h"

namespace ORNL {

kNN::kNN(QVector<Point> referencePoints, QVector<Point> queryPoints, int kNeighbors, GPU_VARIANT variant)
    : AlgorithmBase() {
    m_point_dimension = 3;
    m_kNeighbors = kNeighbors;
    m_referencePointsSize = referencePoints.size();
    m_queryPointsSize = queryPoints.size();
    m_variant = variant;

    m_referencePoints = static_cast<float*>(malloc(referencePoints.size() * m_point_dimension * sizeof(float)));
    m_queryPoints = static_cast<float*>(malloc(queryPoints.size() * m_point_dimension * sizeof(float)));

    m_knn_dist = static_cast<float*>(malloc((queryPoints.size()) * m_kNeighbors * sizeof(float)));
    m_knn_index = static_cast<int*>(malloc((queryPoints.size()) * m_kNeighbors * sizeof(int)));

    int refIndex = 0;
    for (Point point : referencePoints) {
        m_referencePoints[refIndex] = point.x();
        refIndex++;
    }
    for (Point point : referencePoints) {
        m_referencePoints[refIndex] = point.y();
        refIndex++;
    }
    for (Point point : referencePoints) {
        m_referencePoints[refIndex] = point.z();
        refIndex++;
    }

    int queryIndex = 0;
    for (Point point : queryPoints) {
        m_queryPoints[queryIndex] = point.x();
        queryIndex++;
    }
    for (Point point : queryPoints) {
        m_queryPoints[queryIndex] = point.y();
        queryIndex++;
    }
    for (Point point : queryPoints) {
        m_queryPoints[queryIndex] = point.z();
        queryIndex++;
    }
}

kNN::~kNN() {
    free(m_referencePoints);
    free(m_queryPoints);
    free(m_knn_dist);
    free(m_knn_index);
}

void kNN::executeCPU() {
    knn_c(m_referencePoints, m_referencePointsSize, m_queryPoints, m_queryPointsSize, m_point_dimension, m_kNeighbors,
          m_knn_dist, m_knn_index);
}

QVector<Distance> kNN::getNearestDistances() {
    QVector<Distance> nearestDistances;
    for (int i = 0; i < (m_queryPointsSize * m_kNeighbors); i++) {
        nearestDistances.push_back(m_knn_dist[i]);
    }
    return nearestDistances;
}

QVector<int> kNN::getNearestIndices() {
    QVector<int> nearestIndices;
    for (int i = 0; i < (m_queryPointsSize * m_kNeighbors); i++) {
        nearestIndices.push_back(m_knn_index[i]);
    }
    return nearestIndices;
}

void kNN::modified_insertion_sort(float* dist, int* index, int length, int k) {
    // Initialise the first index
    index[0] = 0;

    // Go through all points
    for (int i = 1; i < length; ++i) {

        // Store current distance and associated index
        float curr_dist = dist[i];
        int curr_index = i;

        // Skip the current value if its index is >= k and if it's higher the k-th slready sorted mallest value
        if (i >= k && curr_dist >= dist[k - 1]) {
            continue;
        }

        // Shift values (and indexes) higher that the current distance to the right
        int j = std::min(i, k - 1);
        while (j > 0 && dist[j - 1] > curr_dist) {
            dist[j] = dist[j - 1];
            index[j] = index[j - 1];
            --j;
        }

        // Write the current distance and index at their position
        dist[j] = curr_dist;
        index[j] = curr_index;
    }
}

float kNN::compute_distance(const float* ref, int ref_nb, const float* query, int query_nb, int dim, int ref_index,
                            int query_index) {
    float sum = 0.f;
    for (int d = 0; d < dim; ++d) {
        const float diff = ref[d * ref_nb + ref_index] - query[d * query_nb + query_index];
        sum += diff * diff;
    }
    return qSqrt(sum);
}

bool kNN::knn_c(const float* ref, int ref_nb, const float* query, int query_nb, int dim, int k, float* knn_dist,
                int* knn_index) {

    // Allocate local array to store all the distances / indexes for a given query point
    float* dist = static_cast<float*>(malloc(ref_nb * sizeof(float)));
    int* index = static_cast<int*>(malloc(ref_nb * sizeof(int)));

    // Process one query point at the time
    for (int i = 0; i < query_nb; ++i) {

        // Compute all distances / indexes
        for (int j = 0; j < ref_nb; ++j) {
            dist[j] = compute_distance(ref, ref_nb, query, query_nb, dim, j, i);
            index[j] = j;
        }

        // Sort distances / indexes
        modified_insertion_sort(dist, index, ref_nb, k);

        // Copy k smallest distances and their associated index
        for (int j = 0; j < k; ++j) {
            knn_dist[j * query_nb + i] = dist[j];
            knn_index[j * query_nb + i] = index[j];
        }
    }

    free(dist);
    free(index);

    return true;
}

} // namespace ORNL
