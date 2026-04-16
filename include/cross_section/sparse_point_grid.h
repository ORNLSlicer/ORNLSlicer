#pragma once

#include <cstddef>

#include <qtypes.h>

#include "geometry/point.h"
#include "sparse_grid.h"

namespace ORNL {
/*!
 * \class SparsePointGrid
 *
 * \brief Sparse grid which can locate spatially nearby elements
 * efficiently.
 *
 * \tparam T - The element type to store
 * \tparam Locator = The functor to get the location from T. Locator must
 *      have: Point operator()(const T &t) const
 *      which returns the location associated with val.
 */
template <class T, class Locator> class SparsePointGrid : public SparseGrid<T> {
  public:
    //! \brief Constructs a sparse grid with the specified cell size.
    SparsePointGrid(qlonglong cell_size, size_t element_reserve = 0U, float max_load_factor = 1.0f);

    //! \brief Inserts t into the sparse grid
    void insert(const T& t);

  protected:
    using GridPoint = typename SparseGrid<T>::GridPoint;

    Locator m_locator; //!< Accessor for getting locations from elements
};

template <class T, class Locator>
SparsePointGrid<T, Locator>::SparsePointGrid(qlonglong cell_size, size_t element_reserve, float max_load_factor)
    : SparseGrid<T>(cell_size, element_reserve, max_load_factor) {}

template <class T, class Locator> void SparsePointGrid<T, Locator>::insert(const T& t) {
    Point loc = m_locator(t);
    GridPoint grid_loc = SparseGrid<T>::toGridPoint(loc);

    SparseGrid<T>::m_grid.emplace(grid_loc, t);
}
} // namespace ORNL
