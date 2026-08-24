#pragma once

#include "terminus.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \class PossibleStitch
 * \brief Represents a possible stitch between two polylines.
 *
 * This represents the possibility of creating a new merged
 * polyline from appending terminus_1.getPolylineIdx() onto
 * terminus_0.getPolylineIdx() using the Terminus points as the
 * join point.  Consider polylines A -> B and C -> D.  If
 * terminus_0 is B and terminus_1 is C, then this stitch
 * represents A -> B -> C -> D.  If terminus_0 is C and terminus_1
 * is A, then this stitch represents D -> C -> A -> B.  In
 * general, this stitch represents the polyline:
 *   the other terminus of polyline 0 -> terminus_0 -> terminus_1
 *     -> the other terminus of polyline 1.
 *
 * This class also stores the squared distance involved in making
 * the stitch.
 */
class PossibleStitch {
   public:
    PossibleStitch();

    /*!
     * \brief True if this stitch doesn't require any polyline reversals.
     *
     * If this is true, then the polylines can be appended using
     * their natural order.
     */
    bool inOrder() const;

    /*!
     * \brief Orders Possiblestitch by goodness.
     *
     * Better PossibleStitch are > then worse PossibleStitch.
     * QPriorityQueue will give greatest first so greatest
     * must be most desirable stitch
     */
    bool operator<(const PossibleStitch& other) const;

    Area distance_2;      //!< The squared distance from terminus_0 to terminus_1
    Terminus terminus_0;  //!< The Terminus representing the end of polyline_0
                          //! where the join would happen
    Terminus terminus_1;  //!< The Terminus representing the end of polyline_1
                          //! where the join would happen
};
}  // namespace ORNL
