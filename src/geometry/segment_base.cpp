#include "geometry/segment_base.h"

#include <utility>
#include <vector>

#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/point.h"
#include "geometry/segments/travel.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"

namespace ORNL {
SegmentBase::SegmentBase(Point start, Point end)
    : m_start(start), m_end(end), m_sb(new SettingsBase()), m_deposition_active(true) {
    m_sb->setSetting(SS::kIsRegionStartSegment, false);

    m_sb->setSetting(SS::kRegionType, RegionType::kUnknown);
    m_sb->setSetting(SS::kPathModifiers, PathModifiers::kNone);

    m_non_build_modifiers = PathModifiers::kCoasting | PathModifiers::kForwardTipWipe |
                            PathModifiers::kPerimeterTipWipe | PathModifiers::kReverseTipWipe |
                            PathModifiers::kAngledTipWipe | PathModifiers::kSpiralLift;
}

Point SegmentBase::start() const { return m_start; }

Point SegmentBase::midpoint() const { return (m_start + m_end) / 2.; }

Point SegmentBase::end() const { return m_end; }

uint SegmentBase::layerNumber() { return m_layer_num; }

uint SegmentBase::lineNumber() { return m_line_num; }

float SegmentBase::displayWidth() { return m_display_width; }

float SegmentBase::displayHeight() { return m_display_height; }

SegmentDisplayType SegmentBase::displayType() { return m_display_type; }

void SegmentBase::setDisplayType(SegmentDisplayType type) { m_display_type = type; }

QColor SegmentBase::color() { return m_color; }

bool SegmentBase::depositionActive() const { return m_deposition_active; }

void SegmentBase::setDepositionActive(bool deposition_active) { m_deposition_active = deposition_active; }

void SegmentBase::setCylindricalBeadCenter(const Point& center) {
    m_cylindrical_bead_center = center;
    m_has_cylindrical_bead_center = true;
}

void SegmentBase::clearCylindricalBeadCenter() { m_has_cylindrical_bead_center = false; }

bool SegmentBase::hasCylindricalBeadCenter() const { return m_has_cylindrical_bead_center; }

Point SegmentBase::cylindricalBeadCenter() const { return m_cylindrical_bead_center; }

void SegmentBase::setDisplayInfo(float display_width, float display_length, float display_height,
                                 SegmentDisplayType type, QColor color, uint line_num, uint layer_num) {
    m_display_width = display_width;
    m_display_length = display_length;
    m_display_height = display_height;
    m_display_type = type;
    m_color = color;
    m_line_num = line_num;
    m_layer_num = layer_num;
}

void SegmentBase::setDisplayWidth(float display_width) { m_display_width = display_width; }

void SegmentBase::setDisplayHeight(float display_height) { m_display_height = display_height; }

void SegmentBase::createGraphic(std::vector<float>& vertices, std::vector<float>& normals, std::vector<float>& colors) {
    // NOP
}

void SegmentBase::setStart(Point start) { m_start = start; }

void SegmentBase::setEnd(Point end) { m_end = end; }

void SegmentBase::reverse() { std::swap(m_start, m_end); }

QSharedPointer<SettingsBase> SegmentBase::getSb() const { return m_sb; }

void SegmentBase::setSb(const QSharedPointer<SettingsBase>& sb) { m_sb = sb; }

void SegmentBase::rotate(QQuaternion rotation) {
    // rotate each point
    QVector3D start_vec = m_start.toQVector3D();
    QVector3D result_start = rotation.rotatedVector(start_vec);
    m_start = Point(result_start);

    QVector3D end_vec = m_end.toQVector3D();
    QVector3D result_end = rotation.rotatedVector(end_vec);
    m_end = Point(result_end);

    if (m_has_cylindrical_bead_center) {
        QVector3D center_vec = m_cylindrical_bead_center.toQVector3D();
        QVector3D result_center = rotation.rotatedVector(center_vec);
        m_cylindrical_bead_center = Point(result_center);
    }
}

void SegmentBase::shift(Point shift) {
    m_start = m_start + shift;
    m_end = m_end + shift;
    if (m_has_cylindrical_bead_center) {
        m_cylindrical_bead_center = m_cylindrical_bead_center + shift;
    }
}

bool SegmentBase::isPrintingSegment() {
    if (!m_deposition_active || dynamic_cast<TravelSegment*>(this) != nullptr ||
        (int)(m_sb->setting<uint>(SS::kPathModifiers) & (uint)m_non_build_modifiers) != 0)
        return false;

    return true;
}

Distance SegmentBase::length() { return m_start.distance(m_end); }
} // namespace ORNL
