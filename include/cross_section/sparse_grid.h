#pragma once

#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>

#include <QVector>
#include <qassert.h>
#include <qcontainerfwd.h>
#include <qtypes.h>

#include "geometry/point.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class SparseGrid
 *
 * \brief Sparse grid which can locate spatially nearby elements
 * efficiently.
 *
 * \note This is an abstract template class which doesn't have any functions
 * to insert elements. \see SparsePointGrid
 */
template <class T> class SparseGrid {
  public:
    //! \brief Constructs a sparse grid with the specified cell size.
    SparseGrid(qlonglong cell_size, size_t element_reserve = 0U, float max_load_factor = 1.0f);

    /*!
     * \brief Returns all data within radius of \p query_pt.
     *
     * Finds all elements with location within radius of \p query_pt.  May
     * return additional elements that are beyond radius.
     *
     * Average running time is a * (1 + 2 * radius / cell_size) ** 2 +
     * b*cnt where a and b are proportionality constance and cnt is
     * the number of returned items.  The search will return items in
     * an area of (2 * radius + cell_size) ** 2 on average.  The max range
     * of an item from the query_point is radius + cell_size.
     */
    QVector<T> getNearby(const Point& query_pt, Distance radius) const {
        QVector<T> ret;
        const std::function<bool(const T&)> process_func = [&ret](const T& t) {
            ret.push_back(t);
            return true;
        };
        processNearby(query_pt, radius, process_func);
        return ret;
    }

    // TODO
    // defined here to fix declaration problems,
    // take a look at this commit to understand why
    static const std::function<bool(const T&)> no_precondition() { return true; };

    //! \brief Find the nearest element to a given \p query_pt within \p
    //! radius.
    bool getNearest(const Point& query_pt, Distance radius, T& element_nearest,
                    const std::function<bool(const T& t)> precondition = no_precondition) const;

    /*!
     * \brief Process elements from cells that might contain sought after
     * points.
     *
     *  Processes elements from cell that might have elements within \p
     * radius of \p query_pt.  Processes all elements that are within
     * radius of query_pt.  May process elements that are up to radius +
     * cell_size from query_pt.
     */
    void processNearby(const Point& query_pt, Distance radius, const std::function<bool(const T&)>& process_func) const;

    /*!
     * \brief Process elements from cells that might contain sought after
     * points along a line.
     *
     * Processes elements from cells that cross the line \p query_line.
     * May process elements that are up to sqrt(2) * cell_size from \p
     * query_line.
     */
    void processLine(const QPair<Point, Point> query_line,
                     const std::function<bool(const T&)>& process_elem_func) const;

    long long getCellSize() const;

  protected:
    using GridPoint = Point;
    using grid_coord_t = qlonglong;
    using GridMap = std::unordered_multimap<GridPoint, T>;

    //! \brief Process elements from the cell indicated by \p grid_pt
    bool processFromCell(const GridPoint& grid_pt, const std::function<bool(const T&)>& process_func) const;

    //! \brief Process cells along a line indicated by \p line.
    void processLineCells(const QPair<Point, Point> line, const std::function<bool(GridPoint)>& process_cell_func);

    //! \brief Compute the grid coordinates of a point.
    GridPoint toGridPoint(const Point& point) const;

    //! \brief Compute the grid coordinate of a print space coordinate
    grid_coord_t toGridCoord(const qlonglong& coord) const;

    /*!
     * \brief Compute the lowest point in a grid cell.
     *
     * The lowest point is the point in the grid cell closest to the origin.
     */
    Point toLowerCorner(const GridPoint& location) const;

    /*!
     * \brief Compute the lowest coord in a grid cell.
     *
     * The lowest point is the point in the grid cell closest to the origin.
     */
    long long toLowerCoord(const grid_coord_t& grid_coord) const;

    GridMap m_grid;        //!< Map from grid locations (GridPoint) to elements (T).
    qlonglong m_cell_size; //!< The cell (square) size.
    grid_coord_t nonzero_sign(const grid_coord_t z) const;
};

#define SGI_TEMPLATE template <class T>
#define SGI_THIS     SparseGrid<T>

SGI_TEMPLATE
SGI_THIS::SparseGrid(qlonglong cell_size, size_t element_reserve, float max_load_factor) {
    Q_ASSERT(cell_size > 0U);

    m_cell_size = cell_size;

    // Must be before the reserve call
    m_grid.max_load_factor(max_load_factor);
    if (element_reserve != 0U) {
        m_grid.reserve(element_reserve);
    }
}

SGI_TEMPLATE
typename SGI_THIS::GridPoint SGI_THIS::toGridPoint(const Point& point) const {
    return Point(toGridCoord(point.x()), toGridCoord(point.y()));
}

SGI_TEMPLATE
typename SGI_THIS::grid_coord_t SGI_THIS::toGridCoord(const qlonglong& coord) const {
    // This mapping via truncation results in the cells with
    // GridPoint.x==0 being twice as large and similarly for
    // GridPoint.y==0.  This doesn't cause any incorrect behavior,
    // just changes the running time slightly.  The change in running
    // time from this is probably not worth doing a proper floor
    // operation.
    return coord / m_cell_size;
}

SGI_TEMPLATE
Point SGI_THIS::toLowerCorner(const GridPoint& location) const {
    return Point(toLowerCoord(location.x()), toLowerCoord(location.y()));
}

SGI_TEMPLATE
qlonglong SGI_THIS::toLowerCoord(const grid_coord_t& grid_coord) const {
    // This mapping via truncation results in the cells with
    // GridPoint.x==0 being twice as large and similarly for
    // GridPoint.y==0.  This doesn't cause any incorrect behavior,
    // just changes the running time slightly.  The change in running
    // time from this is probably not worth doing a proper floor
    // operation.
    return grid_coord * m_cell_size;
}

SGI_TEMPLATE
bool SGI_THIS::processFromCell(const GridPoint& grid_pt, const std::function<bool(const T&)>& process_func) const {
    auto grid_range = m_grid.equal_range(grid_pt);
    for (auto iter = grid_range.first; iter != grid_range.second; iter++) {
        if (!process_func(iter->second)) {
            return false;
        }
    }
    return true;
}

SGI_TEMPLATE
void SGI_THIS::processLineCells(const QPair<Point, Point> line,
                                const std::function<bool(GridPoint)>& process_cell_func) {
    Point start = line.first;
    Point end = line.second;

    if (end.x() < start.x()) {
        // make sure X increases between start and end
        std::swap(start, end);
    }

    const GridPoint start_cell = toGridPoint(start);
    const GridPoint end_cell = toGridPoint(end);
    const qlonglong y_diff = end.y() - start.y();
    const grid_coord_t y_dir = nonzero_sign(y_diff);

    grid_coord_t x_cell_start = start_cell.x();

    // for all y from start to end
    for (grid_coord_t cell_y = start_cell.y(); cell_y <= end_cell.y() * y_dir; cell_y += y_dir) {
        // nearest y coordinate of the cells in the next row
        grid_coord_t nearest_next_y =
            toLowerCoord(cell_y + ((nonzero_sign(cell_y) == y_dir || cell_y == 0) ? y_dir : qlonglong(0)));
        grid_coord_t x_cell_end; // The x coord of the last cell to include
                                 // from this row
        if (y_diff == 0) {
            x_cell_end = end_cell.x();
        }
        else {
            qlonglong area = (end.x() - start.x()) * (nearest_next_y - start.y());
            qlonglong corresponding_x = start.x() + area / y_diff; //!< the x coordinate correspinding to
                                                                   //!< nearest_next_y
            x_cell_end = toGridCoord(corresponding_x + ((corresponding_x < 0) && ((area % y_diff) != 0)));
            if (x_cell_end < start_cell.x()) {
                // process at least one cell!
                x_cell_end = x_cell_start;
            }
        }

        for (grid_coord_t cell_x = x_cell_start; cell_x <= x_cell_end; cell_x++) {
            GridPoint grid_loc(cell_x, cell_y);
            bool continue_ = process_cell_func(grid_loc);
            if (!continue_) {
                return;
            }
            if (grid_loc == end_cell) {
                return;
            }

            // TODO: this causes at least a one cell overlap for each row,
            // which include extra cells when crossing precisely on the
            // corners where positive slope where x > 0 and negative slope x
            // < 0
            x_cell_start = x_cell_end;
        }

        Q_ASSERT_X(false, "processLineCells", "We should have returned already before here!");
    }
}

SGI_TEMPLATE
void SGI_THIS::processNearby(const Point& query_pt, Distance radius,
                             const std::function<bool(const T&)>& process_func) const {
    Point min_loc(query_pt.x() - radius(), query_pt.y() - radius());
    Point max_loc(query_pt.x() + radius(), query_pt.y() + radius());

    GridPoint min_grid = toGridPoint(min_loc);
    GridPoint max_grid = toGridPoint(max_loc);

    for (qlonglong grid_y = min_grid.y(); grid_y <= max_grid.y(); grid_y++) {
        for (qlonglong grid_x = min_grid.x(); grid_x <= max_grid.x(); grid_x++) {
            GridPoint grid_pt(grid_x, grid_y);
            bool continue_ = processFromCell(grid_pt, process_func);
            if (!continue_) {
                return;
            }
        }
    }
}

SGI_TEMPLATE
void SGI_THIS::processLine(const QPair<Point, Point> query_line,
                           const std::function<bool(const T&)>& process_elem_func) const {
    const std::function<bool(const GridPoint&)> process_cell_func = [&process_elem_func, this](GridPoint grid_loc) {
        return processFromCell(grid_loc, process_elem_func);
    };
    processLineCells(query_line, process_cell_func);
}

/*
template<class T>
QVector<typename SparseGrid<T>::T> SparseGrid<T>::getNearby(const Point&
query_pt, Distance radius) const
{
    QVector<T> ret;
    const std::function<bool (const T&)> process_func = [&ret](const T& t)
    {
        ret.push_back(t);
        return true;
    };
    processNearby(query_pt, radius, process_func);
    return ret;
}
*/

// // HACK/TODO
// why was this here? take a look at this commit to figure out why
// SGI_TEMPLATE
// const std::function<bool(const typename SGI_THIS::T&)>
// SGI_THIS::no_precondition //= [](const typename SGI_THIS::T&)
// {
//     return true;
// };

SGI_TEMPLATE
bool SGI_THIS::getNearest(const Point& query_pt, Distance radius, T& element_nearest,
                          const std::function<bool(const T& t)> precondition) const {
    bool found = false;
    Area best_dist2 = radius * radius;
    const std::function<bool(const T&)> process_func = [&query_pt, &element_nearest, &found, &best_dist2,
                                                        &precondition](const T& t) {
        if (!precondition(t)) {
            return true;
        }
        Area dist2 = qPow((t.point - query_pt).distance()(), 2);
        if (dist2 < best_dist2) {
            found = true;
            element_nearest = t;
            best_dist2 = dist2;
        }
        return true;
    };
    processNearby(query_pt, radius, process_func);
    return found;
}

SGI_TEMPLATE
qlonglong SGI_THIS::getCellSize() const { return m_cell_size; }

SGI_TEMPLATE
typename SGI_THIS::grid_coord_t SGI_THIS::nonzero_sign(const grid_coord_t z) const { return (z >= 0) - (z - 0); }
} // namespace ORNL
