#include "geometry/segments/line.h"

#include <vector>

#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "gcode/writers/writer_base.h"
#include "geometry/point.h"
#include "geometry/segment_base.h"
#include "graphics/support/shape_factory.h"

namespace ORNL {
LineSegment::LineSegment(Point start, Point end) : SegmentBase(start, end) {
    // NOP
}

void LineSegment::createGraphic(std::vector<float>& vertices, std::vector<float>& normals, std::vector<float>& colors) {
    ShapeFactory::createGcodeCylinder(m_display_width, m_display_length, m_display_height, m_start.toQVector3D(),
                                      m_end.toQVector3D(), m_color, vertices, colors, normals);
}

QSharedPointer<SegmentBase> LineSegment::clone() const { return QSharedPointer<LineSegment>::create(*this); }

QString LineSegment::writeGCode(QSharedPointer<WriterBase> writer) {
    return writer->writeLine(m_start, m_end, this->getSb());
}

float LineSegment::getMinZ() {
    if (m_start.z() < m_end.z()) {
        return m_start.z();
    }
    else {
        return m_end.z();
    }
}
} // namespace ORNL
