#include "cross_section/terminus_tracking_map.h"

#include <cstddef>

#include <qcontainerfwd.h>
#include <qtypes.h>

#include "cross_section/terminus.h"

namespace ORNL {
// Second constructor argument was missing, was causing out of bounds range.
TerminusTrackingMap::TerminusTrackingMap(Terminus::Index end_idx)
    : m_terminus_old_to_current_map(end_idx), m_terminus_current_to_old_map(end_idx) {
    // Initialize map to everythin points to itself seince nothing has moved
    // yet
    for (uint i = 0; i < end_idx; i++) {
        m_terminus_current_to_old_map[i] = Terminus {i};
    }
    m_terminus_old_to_current_map = m_terminus_current_to_old_map;
}

Terminus TerminusTrackingMap::getCurrentFromOld(const Terminus& old) const {
    return m_terminus_old_to_current_map[old.asIndex()];
}

Terminus TerminusTrackingMap::getOldFromCurrent(const Terminus& current) const {
    return m_terminus_current_to_old_map[current.asIndex()];
}

void TerminusTrackingMap::markRemoved(const Terminus& current) {
    Terminus old = getOldFromCurrent(current);
    m_terminus_old_to_current_map[old.asIndex()] = Terminus::INVALID_TERMINUS;
    m_terminus_current_to_old_map[current.asIndex()] = Terminus::INVALID_TERMINUS;
}

void TerminusTrackingMap::updateMap(size_t num_terminuses, const Terminus* current_terminuses,
                                    const Terminus* next_terminuses, size_t num_removed_terminuses,
                                    const Terminus* removed_current_terminuses) {
    // save old locations
    QVector<Terminus> old_terminuses(num_terminuses);
    for (uint i = 0; i < num_terminuses; i++) {
        old_terminuses[i] = getOldFromCurrent(current_terminuses[i]);
    }

    // update using maps old <-> current and current <-> next
    for (uint i = 0; i < num_terminuses; i++) {
        m_terminus_old_to_current_map[old_terminuses[i].asIndex()] = next_terminuses[i];
        Terminus next_terminus = next_terminuses[i];
        if (next_terminus != Terminus::INVALID_TERMINUS) {
            m_terminus_current_to_old_map[next_terminus.asIndex()] = old_terminuses[i];
        }
    }

    // remove next locations that no longer exist
    for (uint i = 0; i < num_removed_terminuses; i++) {
        m_terminus_current_to_old_map[removed_current_terminuses[i].asIndex()] = Terminus::INVALID_TERMINUS;
    }
}
} // namespace ORNL
