#include "geometry/segments/scan.h"

#include <limits>

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "units/unit.h"
#include "utilities/constants.h"

namespace ORNL {
ScanSegment::ScanSegment(Point start, Point end) : SegmentBase(start, end) {
    // NOP
}

QSharedPointer<SegmentBase> ScanSegment::clone() const { return QSharedPointer<ScanSegment>::create(*this); }

QString ScanSegment::writeGCode(QSharedPointer<WriterBase> writer) {
    Velocity speed = this->getSb()->setting<Velocity>(SS::kSpeed);

    return writer->writeScan(m_end, speed, m_on_off);
}

void ScanSegment::setDataCollection(bool on) { m_on_off = on; }

float ScanSegment::getMinZ() {
    // don't want this min to impact the min for an entire layer, but still need to return something
    // so return the largest possible number
    return std::numeric_limits<float>::max();
}
} // namespace ORNL
