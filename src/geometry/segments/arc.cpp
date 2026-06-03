#include "geometry/segments/arc.h"

#include <math.h>

#include <cmath>
#include <limits>
#include <vector>

#include <qhashfunctions.h>
#include <qmath.h>
#include <qnumeric.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "exceptions/exceptions.h"
#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "graphics/support/shape_factory.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
ArcSegment::ArcSegment(Point start, Point end, Point center, Angle angle, bool ccw)
    : SegmentBase(start, end), m_center(center), m_angle(angle), m_ccw(ccw) {
    // NOP
}

ArcSegment::ArcSegment(Point start, Point middle, Point end) : SegmentBase(start, end) {
    switch (MathUtils::orientation(start, middle, end)) {
        case 0:
            throw IllegalArgumentException(
                "Cannot construct an ArcSegment from collinear start, middle, and end points");
        case 1:            // Clockwise
            m_ccw = false;
            break;
        case -1: // Counter-clockwise
            m_ccw = true;
            break;
    }

    m_start = start;
    m_center = CalculateCenter(start, middle, end);
    m_end = end;

    updateAngle();
}

ArcSegment::ArcSegment(Point start, Point end, Point center, bool ccw)
    : SegmentBase(start, end), m_center(center), m_ccw(ccw) {
    updateAngle();
}

void ArcSegment::createGraphic(std::vector<float>& vertices, std::vector<float>& normals, std::vector<float>& colors) {
    ShapeFactory::appendArcBead(m_display_width, m_start, m_center, m_end, m_ccw, m_color, vertices, colors, normals);
}

QSharedPointer<SegmentBase> ArcSegment::clone() const { return QSharedPointer<ArcSegment>::create(*this); }

Point ArcSegment::center() const { return m_center; }

Angle ArcSegment::angle() const { return m_angle; }

void ArcSegment::setAngle(const Angle& angle) { m_angle = angle; }

bool ArcSegment::counterclockwise() const { return m_ccw; }

QString ArcSegment::writeGCode(QSharedPointer<WriterBase> writer) {
    Velocity speed = this->getSb()->setting<Velocity>(SS::kSpeed);
    int extruderSpeed = this->getSb()->setting<int>(SS::kExtruderSpeed);
    RegionType regionType = this->getSb()->setting<RegionType>(SS::kRegionType);
    PathModifiers modifiers = this->getSb()->setting<PathModifiers>(SS::kPathModifiers);
    return writer->writeArc(m_start, m_end, m_center, m_angle, m_ccw, this->getSb());
}

float ArcSegment::getMinZ() {
    // might not actually be correct
    if (m_start.z() < m_end.z())
        return m_start.z();
    else
        return m_end.z();
}

Distance ArcSegment::length() {
    const double planar_radius = std::hypot(m_start.x() - m_center.x(), m_start.y() - m_center.y());
    const double planar_length = m_angle() * planar_radius;
    const double z_delta = m_end.z() - m_start.z();
    return Distance(std::hypot(planar_length, z_delta));
}

Point ArcSegment::CalculateCenter(const Point& start, const Point& middle, const Point& end) {
    const double ax = start.x() - end.x();
    const double ay = start.y() - end.y();
    const double bx = middle.x() - end.x();
    const double by = middle.y() - end.y();
    const double determinant = 2.0 * ((ax * by) - (ay * bx));
    const double scale = qMax(1.0, qMax(qMax(qAbs(ax), qAbs(ay)), qMax(qAbs(bx), qAbs(by))));
    const double determinant_tolerance = std::numeric_limits<double>::epsilon() * scale * scale * 16.0;

    if (qFuzzyIsNull(determinant) || qAbs(determinant) <= determinant_tolerance) {
        throw IllegalArgumentException("Cannot calculate an arc center from collinear or near-collinear points");
    }

    const double start_offset_squared = (ax * ax) + (ay * ay);
    const double middle_offset_squared = (bx * bx) + (by * by);
    const double x = end.x() + ((by * start_offset_squared) - (ay * middle_offset_squared)) / determinant;
    const double y = end.y() + ((ax * middle_offset_squared) - (bx * start_offset_squared)) / determinant;

    return Point(x, y, ((end.z() - start.z()) / 2) + start.z());
}

Distance ArcSegment::Radius(const Point& a, const Point& b, const Point& c) {
    return a.distance(CalculateCenter(a, b, c));
}

double ArcSegment::SignedCurvature(const Point& a, const Point& b, const Point& c) {
    double li = a.distance(b)();
    double li1 = b.distance(c)();
    double qi = a.distance(c)();

    QVector3D li_v = b.toQVector3D() - a.toQVector3D();
    QVector3D li1_v = c.toQVector3D() - b.toQVector3D();

    double det = QVector3D::crossProduct(li_v, li1_v).z();

    double numerator = 2 * det;
    double denominator = li * li1 * qi;

    return numerator / denominator;
}

double ArcSegment::SignedCurvature(QSharedPointer<SegmentBase> first, QSharedPointer<SegmentBase> second) {
    return SignedCurvature(first->start(), first->end(), second->end());
}

void ArcSegment::updateAngle() {
    double a = qAtan2(m_center.x() - m_start.x(), m_center.y() - m_start.y());
    double b = qAtan2(m_center.x() - m_end.x(), m_center.y() - m_end.y());

    if (m_ccw)
        m_angle = Angle(a - b);
    else
        m_angle = Angle(b - a);

    if (m_angle <= 0)
        m_angle = (2.0f * M_PI) + m_angle;
}

void ArcSegment::rotate(QQuaternion rotation) {
    // rotate each point
    QVector3D center_vec = m_center.toQVector3D();
    QVector3D result_center = rotation.rotatedVector(center_vec);
    m_center = Point(result_center);

    SegmentBase::rotate(rotation);
}

void ArcSegment::shift(Point shift) {
    m_center = m_center + shift;

    SegmentBase::shift(shift);
}
} // namespace ORNL
