#pragma once

#include <cstddef>

#include <QVector>
#include <qcontainerfwd.h>

#include "terminus.h"

namespace ORNL {
/*!
 * \class TerminusTrackingMap
 *
 * \brief Tracks movements of polyline end point locations (Terminus).
 *
 * Tracks the movement of polyline end point locations within the
 * polyline vector as polylines are joined, reversed, and used to
 * form polygons.
 */
class TerminusTrackingMap {
  public:
    //! \brief Initializes the TerminusTrackingMap with the size indicated.
    TerminusTrackingMap(Terminus::Index end_idx);

    /*!
     * \brief Given the old Terminus location returns the current location.
     *
     * If the old location is no longer the endpoint of a polyline
     * in the polyline vector, then this returns
     * Terminus::INVALID_TERMINUS.  As long as the old location is
     * still an endpoint in the polyline vector, then
     * getCurFromOld(old) will always refer to the same point.
     * Endpoints are removed from the polyline vector as polylines
     * are merged or converted to Polygons.
     */
    Terminus getCurrentFromOld(const Terminus& old) const;

    //! \brief Given the current Terminus location returns the old location.
    Terminus getOldFromCurrent(const Terminus& current) const;

    /*!
     * \brief Mark the current Terminus as being removed.
     *
     * This marks the current Terminus as being removed from the
     * polyline vector.
     */
    void markRemoved(const Terminus& current);

    /*!
     * \brief Update the map for movement of Terminus.
     *
     * This updates the map for the movement / removal of Terminus
     * locations.  next_terms[i] should refer to the same point as
     * current_terms[i] for i < num_terms, unless the Terminus was
     * removed.  If the Terminus was removed, next_terms[i] should
     * be INVALID_TERMINUS.
     *
     * removed_current_terminuses should refer to those Terminus that are
     * no longer present after the update.  removed_current_terminuses
     * should be the set of terminus values that are in cur_terms
     * but not in next_terminuses, i.e. viewing the inputs as sets:
     * removed_current_terminuses = next_terminuses - cur_terminuses.  It is
     * passed
     * separately to avoid calculating the set difference since
     * the caller generally has this information readily
     * available.
     */
    void updateMap(size_t num_terminuses, const Terminus* current_terminus, const Terminus* next_terminus,
                   size_t num_removed_terminuses, const Terminus* removed_current_terminuses);

  private:
    QVector<Terminus> m_terminus_old_to_current_map;
    QVector<Terminus> m_terminus_current_to_old_map;
};
} // namespace ORNL
