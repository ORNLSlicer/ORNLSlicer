#include "geometry/segments/travel.h"

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "utilities/enums.h"

namespace ORNL {
TravelSegment::TravelSegment(Point start, Point end, TravelLiftType liftType)
    : m_lift_type(liftType), SegmentBase(start, end) {
    // NOP
}

QSharedPointer<SegmentBase> TravelSegment::clone() const { return QSharedPointer<TravelSegment>::create(*this); }

QString TravelSegment::writeGCode(QSharedPointer<WriterBase> writer) {
    return writer->writeTravel(m_start, m_end, m_lift_type, this->getSb());
}

void TravelSegment::setLiftType(TravelLiftType newLiftType) { m_lift_type = newLiftType; }

float TravelSegment::getMinZ() {
    if (m_start.z() < m_end.z())
        return m_start.z();
    else
        return m_end.z();
}
} // namespace ORNL
