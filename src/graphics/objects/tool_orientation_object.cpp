#include "graphics/objects/tool_orientation_object.h"

#include "graphics/base_view.h"

namespace ORNL {
namespace {
constexpr int kVerticesPerTriad = 18;
}

ToolOrientationObject::ToolOrientationObject(BaseView* view, const QVector<ToolOrientationPose>& poses,
                                             float axis_length) {
    std::vector<float> vertices, colors;
    vertices.reserve(poses.size() * kVerticesPerTriad * 3);
    colors.reserve(poses.size() * kVerticesPerTriad * 4);
    const QVector3D axes[]     = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const QColor axis_colors[] = {QColor(255, 0, 0), QColor(0, 200, 0), QColor(0, 100, 255)};
    for (const auto& pose : poses) {
        m_lines.append(pose.line);
        for (int axis = 0; axis < 3; ++axis) {
            const QVector3D tip      = axes[axis] * axis_length;
            const QVector3D shoulder = tip * 0.75f;
            const QVector3D wing     = axes[(axis + 1) % 3] * axis_length * 0.12f;
            for (const auto& point : {QVector3D(), tip, tip, shoulder + wing, tip, shoulder - wing}) {
                const QVector3D position = pose.position + pose.orientation.rotatedVector(point);
                vertices.insert(vertices.end(), {position.x(), position.y(), position.z()});
                const QColor color = axis_colors[axis];
                colors.insert(colors.end(), {float(color.redF()), float(color.greenF()), float(color.blueF()), 1.0f});
            }
        }
    }
    populateGL(view, vertices, {}, colors, GL_LINES);
    setOnTop(true);
}

void ToolOrientationObject::excludeLine(uint line) {
    m_excluded_index = m_lines.indexOf(line);
}

void ToolOrientationObject::draw() {
    // The optional true-width bead overlay also uses the on-top depth range.
    // Reserve its front boundary for triads, regardless of child render order.
    m_view->glDepthRange(0.0, 0.0);
    if (m_excluded_index < 0) {
        GraphicsObject::draw();
        return;
    }
    const int first_count  = m_excluded_index * kVerticesPerTriad;
    const int second_start = first_count + kVerticesPerTriad;
    m_view->glDrawArrays(GL_LINES, 0, first_count);
    m_view->glDrawArrays(GL_LINES, second_start, m_vertices.size() / 3 - second_start);
}
}  // namespace ORNL
