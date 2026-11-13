#include "geometry/segments/travel.h"

#include <algorithm>

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

QSharedPointer<SegmentBase> TravelSegment::clone() const {
    return QSharedPointer<TravelSegment>::create(*this);
}

QString TravelSegment::writeGCode(QSharedPointer<WriterBase> writer) {
    const QString gcode = writer->writeLayerTravel(m_start, m_end, m_lift_type, this->getSb(), m_waypoints);
    if (!gcode.isEmpty()) writer->setCurrentPosition(m_end);
    return gcode;
}

void TravelSegment::setLiftType(TravelLiftType newLiftType) {
    m_lift_type = newLiftType;
}

void TravelSegment::setWaypoints(const QVector<Point>& waypoints) {
    m_waypoints = waypoints;
}

const QVector<Point>& TravelSegment::waypoints() const {
    return m_waypoints;
}

float TravelSegment::getMinZ() {
    float minimum = std::min(m_start.z(), m_end.z());
    for (const Point& point : m_waypoints) minimum = std::min(minimum, point.z());
    return minimum;
}

Distance TravelSegment::length() {
    Distance result;
    Point previous = m_start;
    for (const Point& point : m_waypoints) {
        result += previous.distance(point);
        previous = point;
    }
    return result + previous.distance(m_end);
}

void TravelSegment::reverse() {
    SegmentBase::reverse();
    std::reverse(m_waypoints.begin(), m_waypoints.end());
}

void TravelSegment::rotate(QQuaternion rotation) {
    SegmentBase::rotate(rotation);
    for (Point& point : m_waypoints) point = Point(rotation.rotatedVector(point.toQVector3D()));
}

void TravelSegment::shift(Point shift) {
    SegmentBase::shift(shift);
    for (Point& point : m_waypoints) point = point + shift;
}
}  // namespace ORNL
