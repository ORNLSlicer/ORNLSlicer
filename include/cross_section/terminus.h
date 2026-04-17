#pragma once

#include <cstddef>

#include <qtypes.h>

namespace ORNL {
/*!
 * \class Terminus
 * \brief This class represents the location of an end point of a
 *      polyline in a polyline vector.
 *
 * The location records the index in the polyline vector and
 * whether this is the vertex at the start of the polyline or the
 * vertex at the end.
 */
class Terminus {
  public:
    /*! A representation of Terminus that can be used as an array index.
     *
     * See \ref asIndex() for more information.
     */
    using Index = size_t;

    /*! A Terminus value representing an invalid value.
     *
     * This is used to record when Terminus are removed.
     */
    static const Terminus INVALID_TERMINUS;

    //! \brief Constructor
    Terminus();

    /*!
     * \brief Constructor from Index representation.
     *
     * Terminus{t.asIndex()} == t for all Terminus t.
     */
    Terminus(Index idx);

    /*!
     * \brief  Constuctor from the polyline index and which end of the
     * polyline.
     *
     * Terminus{t.getPolylineIdx(), t.isEnd()} == t for all Terminus t.
     */
    Terminus(Index polyline_idx, bool is_end);

    //! Gets the polyline index for this Terminus.
    Index getPolylineIdx() const;

    //! \brief Gets whether this Terminus represents the end point of the
    //! polyline.
    bool isEnd() const;

    /*!
     * \brief Gets the Index representation of this Terminus.
     *
     * The index representation much satisfy the following:
     * 1. for all Terminus t0, t1: t0 == t1 implies t0.asIndex() ==
     * t1.asIndex()
     * 2. for all Terminus t0, t1: t0 != t1 implies t0.asIndex() !=
     * t1.asIndex()
     * 3. t0.asIndex() >= 0
     * 4. if y = \ref endIndexFromPolylineEndIndex(x), then for all Terminus
     * t if t.getPolylineIdx() < x then t.asIndex() < y
     *
     * In addition, the Index representation should be reasonably
     * compact for efficiency.  This means that for polyline index
     * in [0,x) and Terminus t with t.getPolylineIdx() < x, the
     * set of containing all t.asIndex() union {0} should be
     * small.  In other words, t.asIndex() should map to [0,y)
     * where y is as small as possible.
     */
    Index asIndex() const;

    //! \brief Calculates the Terminus end Index from the polyline vector
    //! end index.

    static Index endIndexFromPolylineEndIndex(uint polyline_end_idx);

    /*!
     * \brief Tests for equality
     *
     * Two Terminus are equal if they return the same results for \ref
     * getPolylineIdx()
     * and \ref isEnd().
     */
    bool operator==(const Terminus& other);

    //! \brief Test for inequality
    bool operator!=(const Terminus& other);

  private:
    /*!
     * \brief The index representation of the Terminus
     *
     * The polyline idx and end flas are calculated from this on demand.
     */
    Index m_idx;
};
} // namespace ORNL
