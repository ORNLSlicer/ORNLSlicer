#include "graphics/support/tool_orientation_preview.h"

#include <algorithm>
#include <cmath>

#include "geometry/segments/arc.h"
#include "geometry/segments/bezier.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
constexpr qsizetype kMaximumSamples = 200;
constexpr double kTwoPi             = 6.28318530717958647692;

ToolOrientationPose poseFor(const QSharedPointer<SegmentBase>& segment) {
    return {segment->lineNumber(), segment->end().toQVector3D(), *segment->toolOrientation()};
}
}  // namespace

void ToolOrientationPreview::reset(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    m_visible_segments.clear();
    m_samples.clear();
    clearSelection();

    QVector3D minimum;
    QVector3D maximum;
    bool has_bounds    = false;
    const auto include = [&](const QVector3D& point) {
        if (!has_bounds) {
            minimum = maximum = point;
            has_bounds        = true;
        }
        else {
            for (int axis = 0; axis < 3; ++axis) {
                minimum[axis] = std::min(minimum[axis], point[axis]);
                maximum[axis] = std::max(maximum[axis], point[axis]);
            }
        }
    };
    for (const auto& layer : gcode) {
        for (const auto& segment : layer) {
            if (!segment->toolOrientation()) { continue; }
            include(segment->start().toQVector3D());
            include(segment->end().toQVector3D());
            // Include arc extrema: a full revolution can start and end at the same XY point.
            if (const auto* arc = dynamic_cast<ArcSegment*>(segment.data())) {
                const Point start        = arc->start();
                const Point center       = arc->center();
                const double radius      = std::hypot(start.x() - center.x(), start.y() - center.y());
                const double start_angle = std::atan2(start.y() - center.y(), start.x() - center.x());
                for (int quadrant = 0; quadrant < 4; ++quadrant) {
                    const double angle = quadrant * kTwoPi / 4.0;
                    const double delta = arc->counterclockwise() ? angle - start_angle : start_angle - angle;
                    const double sweep = std::fmod(std::fmod(delta, kTwoPi) + kTwoPi, kTwoPi);
                    if (sweep <= arc->angle()()) {
                        include(QVector3D(center.x() + radius * std::cos(angle), center.y() + radius * std::sin(angle),
                                          start.z()));
                    }
                }
            }
            else if (auto* bezier = dynamic_cast<BezierSegment*>(segment.data())) {
                for (int sample = 1; sample < 48; ++sample) {
                    include(bezier->getPointAlong(sample / 48.0).toQVector3D());
                }
            }
        }
    }
    const float millimeter = (1.0 * mm)() * Constants::OpenGL::kObjectToView;
    m_axis_length          = std::clamp(0.02f * (maximum - minimum).length(), millimeter, 10.0f * millimeter);
}

void ToolOrientationPreview::refresh(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode, uint low_layer,
                                     uint high_layer, uint low_segment, uint high_segment,
                                     SegmentDisplayType hidden_types) {
    m_visible_segments.clear();
    m_samples.clear();
    uint index = 0;
    for (uint layer = low_layer; layer < static_cast<uint>(gcode.size()) && layer <= high_layer; ++layer) {
        for (const auto& segment : gcode[layer]) {
            if (index >= low_segment && index <= high_segment && segment->toolOrientation() &&
                !static_cast<bool>(segment->displayType() & hidden_types)) {
                m_visible_segments.append(segment);
            }
            ++index;
        }
    }
    const qsizetype count = std::min(kMaximumSamples, m_visible_segments.size());
    m_samples.reserve(count);
    for (qsizetype sample = 0; sample < count; ++sample) {
        const qsizetype index = count == 1 ? 0 : sample * (m_visible_segments.size() - 1) / (count - 1);
        m_samples.append(poseFor(m_visible_segments[index]));
    }
}

void ToolOrientationPreview::updateSelection(const QList<int>& added, const QList<int>& removed) {
    for (int line : removed) { m_selected_lines.removeAll(line + 1); }
    for (int line : added) {
        m_selected_lines.removeAll(line + 1);
        m_selected_lines.append(line + 1);
    }
}

void ToolOrientationPreview::clearSelection() {
    m_selected_lines.clear();
}

QList<int> ToolOrientationPreview::selectedLines() const {
    QList<int> lines;
    for (uint line : m_selected_lines) { lines.append(int(line) - 1); }
    return lines;
}

float ToolOrientationPreview::axisLength() const {
    return m_axis_length;
}

const QVector<ToolOrientationPose>& ToolOrientationPreview::sampledPoses() const {
    return m_samples;
}

std::optional<ToolOrientationPose> ToolOrientationPreview::activePose() const {
    for (auto selected = m_selected_lines.crbegin(); selected != m_selected_lines.crend(); ++selected) {
        const auto found =
            std::lower_bound(m_visible_segments.cbegin(), m_visible_segments.cend(), *selected,
                             [](const auto& segment, uint line) { return segment->lineNumber() < line; });
        if (found != m_visible_segments.cend() && (*found)->lineNumber() == *selected) { return poseFor(*found); }
    }
    if (m_visible_segments.isEmpty()) { return std::nullopt; }
    return poseFor(m_visible_segments.last());
}
}  // namespace ORNL
