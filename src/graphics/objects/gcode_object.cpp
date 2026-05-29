#include "graphics/objects/gcode_object.h"

#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmatrix4x4.h>
#include <qnamespace.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/segment_base.h"
#include "geometry/segments/line.h"
#include "graphics/base_view.h"
#include "graphics/graphics_object.h"
#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "widgets/gcode_info_control.h"

namespace ORNL {
namespace {
//! @brief Segment count where full-build bead rendering should fall back to overview lines.
constexpr qsizetype kLightweightLineThreshold = 2000000;

//! @brief Segment count where instanced bead rendering starts avoiding large CPU mesh expansion.
constexpr qsizetype kInstancedBeadThreshold = 100000;

constexpr uint kInstanceFloatCount = 10;
constexpr uint kInstanceStartOffset = 0;
constexpr uint kInstanceDeltaOffset = 3;
constexpr uint kInstanceColorOffset = 6;
constexpr uint kInstanceStrideBytes = kInstanceFloatCount * sizeof(float);

//! @brief Counts display segments before GL buffer construction so oversized gcode can use a lighter path.
qsizetype countSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    qsizetype count = 0;
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        count += layer.size();
    }
    return count;
}

bool allSegmentsAreLines(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (dynamic_cast<LineSegment*>(segment.data()) == nullptr)
                return false;
        }
    }

    return true;
}

QVector3D displayEnd(const QSharedPointer<SegmentBase>& segment) {
    QVector3D end = segment->end().toQVector3D();
    if (dynamic_cast<LineSegment*>(segment.data()) != nullptr) {
        end += segment->start().toQVector3D();
    }

    return end;
}

void appendInstancedTemplateTriangle(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2,
                                     std::vector<float>& vertices, std::vector<float>& normals) {
    vertices.insert(vertices.end(), {v0.x(), v0.y(), v0.z(), v1.x(), v1.y(), v1.z(), v2.x(), v2.y(), v2.z()});

    QVector3D normal = QVector3D::crossProduct(v1 - v0, v2 - v0).normalized();
    for (int i = 0; i < 3; ++i) {
        normals.insert(normals.end(), {normal.x(), normal.y(), normal.z()});
    }
}

void createInstancedBeadTemplate(float width, float height, std::vector<float>& vertices,
                                 std::vector<float>& normals) {
    width = std::max(width, std::numeric_limits<float>::epsilon());
    height = std::max(height, std::numeric_limits<float>::epsilon());

    float radius;
    unsigned int quads_per_side;
    if (height > width) {
        radius = std::sqrt((width / 2.0f) * (width / 2.0f) + (height / 2.0f) * (height / 2.0f));
        quads_per_side = 1;
    }
    else {
        radius = width / 2.0f;
        quads_per_side = 6;
    }

    const unsigned int vertices_per_arc = quads_per_side + 1;
    const unsigned int vertices_per_side = 2 * vertices_per_arc;
    const float theta_start = -std::asin((height / 2.0f) / radius);
    const float theta_end = -theta_start;
    const float theta_increment = (theta_end - theta_start) / quads_per_side;

    std::vector<QVector3D> top_vertices(2 * vertices_per_arc);
    std::vector<QVector3D> bottom_vertices(2 * vertices_per_arc);

    for (unsigned int i = 0; i < vertices_per_arc; ++i) {
        const float theta = theta_start + i * theta_increment;
        const float x = radius * std::cos(theta);
        const float y = radius * std::sin(theta);

        top_vertices[i] = QVector3D(x, y, 1.0f);
        bottom_vertices[i] = QVector3D(x, y, 0.0f);
        top_vertices[i + vertices_per_arc] = QVector3D(-x, -y, 1.0f);
        bottom_vertices[i + vertices_per_arc] = QVector3D(-x, -y, 0.0f);
    }

    const QVector3D top_center(0.0f, 0.0f, 1.0f);
    const QVector3D bottom_center(0.0f, 0.0f, 0.0f);

    for (unsigned int i = 0; i < vertices_per_side; ++i) {
        const unsigned int j = (i + 1) % vertices_per_side;
        appendInstancedTemplateTriangle(top_center, top_vertices[j], top_vertices[i], vertices, normals);
        appendInstancedTemplateTriangle(top_vertices[i], bottom_vertices[j], bottom_vertices[i], vertices, normals);
        appendInstancedTemplateTriangle(top_vertices[i], top_vertices[j], bottom_vertices[j], vertices, normals);
        appendInstancedTemplateTriangle(bottom_center, bottom_vertices[i], bottom_vertices[j], vertices, normals);
    }
}

void appendBoundingVertices(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode, std::vector<float>& vertices,
                            std::vector<float>& normals, std::vector<float>& colors) {
    QVector3D min(Constants::Limits::Maximums::kMaxFloat, Constants::Limits::Maximums::kMaxFloat,
                  Constants::Limits::Maximums::kMaxFloat);
    QVector3D max(Constants::Limits::Minimums::kMinFloat, Constants::Limits::Minimums::kMinFloat,
                  Constants::Limits::Minimums::kMinFloat);

    float max_half_bead = 0.0f;
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            const QVector3D start = segment->start().toQVector3D();
            const QVector3D end = displayEnd(segment);

            min.setX(std::min({min.x(), start.x(), end.x()}));
            min.setY(std::min({min.y(), start.y(), end.y()}));
            min.setZ(std::min({min.z(), start.z(), end.z()}));
            max.setX(std::max({max.x(), start.x(), end.x()}));
            max.setY(std::max({max.y(), start.y(), end.y()}));
            max.setZ(std::max({max.z(), start.z(), end.z()}));

            max_half_bead = std::max(max_half_bead, std::max(segment->displayWidth(), segment->displayHeight()) / 2.0f);
        }
    }

    min -= QVector3D(max_half_bead, max_half_bead, max_half_bead);
    max += QVector3D(max_half_bead, max_half_bead, max_half_bead);

    const std::array<QVector3D, 8> corners = {QVector3D(min.x(), min.y(), min.z()),
                                             QVector3D(min.x(), max.y(), min.z()),
                                             QVector3D(max.x(), max.y(), min.z()),
                                             QVector3D(max.x(), min.y(), min.z()),
                                             QVector3D(min.x(), min.y(), max.z()),
                                             QVector3D(min.x(), max.y(), max.z()),
                                             QVector3D(max.x(), max.y(), max.z()),
                                             QVector3D(max.x(), min.y(), max.z())};

    vertices.reserve(corners.size() * 3);
    normals.reserve(corners.size() * 3);
    colors.reserve(corners.size() * 4);
    for (const QVector3D& corner : corners) {
        vertices.insert(vertices.end(), {corner.x(), corner.y(), corner.z()});
        normals.insert(normals.end(), {0.0f, 0.0f, 0.0f});
        colors.insert(colors.end(), {0.0f, 0.0f, 0.0f, 0.0f});
    }
}

//! @brief Appends a single GL_LINES segment using the parser's display-space line representation.
void appendLightweightLine(const QSharedPointer<SegmentBase>& segment, std::vector<float>& vertices,
                           std::vector<float>& normals, std::vector<float>& colors) {
    QVector3D start = segment->start().toQVector3D();
    QVector3D end = displayEnd(segment);

    vertices.push_back(start.x());
    vertices.push_back(start.y());
    vertices.push_back(start.z());
    vertices.push_back(end.x());
    vertices.push_back(end.y());
    vertices.push_back(end.z());

    for (int i = 0; i < 2; ++i) {
        // Lightweight gcode uses GL_LINES, so these vertices do not have a
        // meaningful surface normal. Keep them consistent with other line-only
        // objects to avoid lighting/specular highlights washing pale segment
        // colors, such as travel, toward white.
        normals.push_back(0.0f);
        normals.push_back(0.0f);
        normals.push_back(0.0f);

        colors.push_back(segment->color().redF());
        colors.push_back(segment->color().greenF());
        colors.push_back(segment->color().blueF());
        colors.push_back(segment->color().alphaF());
    }
}
} // namespace

GCodeObject::GCodeObject(BaseView* view, QVector<QVector<QSharedPointer<SegmentBase>>> gcode,
                         QSharedPointer<GCodeInfoControl> segmentInfoControl) {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;

    m_segment_info_control = segmentInfoControl;
    m_segment_info_control->setGCode(gcode);

    const qsizetype segment_count = countSegments(gcode);
    const bool can_instance_beads = allSegmentsAreLines(gcode);
    m_instanced_beads =
        can_instance_beads && segment_count > kInstancedBeadThreshold && segment_count <= kLightweightLineThreshold;
    m_lightweight_lines =
        segment_count > kLightweightLineThreshold || (!can_instance_beads && segment_count > kInstancedBeadThreshold);
    if (m_lightweight_lines) {
        vertices.reserve(segment_count * 2 * 3);
        normals.reserve(segment_count * 2 * 3);
        colors.reserve(segment_count * 2 * 4);
    }

    m_segments.reserve(gcode.size());

    for (auto& layer : gcode) {
        QVector<QSharedPointer<SegmentDisplayMeta>> layer_meta;
        layer_meta.reserve(layer.size());

        for (auto& segment : layer) {
            QSharedPointer<SegmentDisplayMeta> seg_meta = QSharedPointer<SegmentDisplayMeta>::create();
            seg_meta->layer = segment->layerNumber();
            seg_meta->line = segment->lineNumber();
            seg_meta->type = segment->displayType();
            seg_meta->original_color = segment->color();
            seg_meta->current_color = segment->color();

            if (m_lightweight_lines) {
                seg_meta->offset = vertices.size() / 3;
                appendLightweightLine(segment, vertices, normals, colors);
                seg_meta->length = (vertices.size() / 3) - seg_meta->offset;
            }
            else if (m_instanced_beads) {
                const auto [group_index, instance_index] = appendInstancedBead(segment);
                seg_meta->instance_group = group_index;
                seg_meta->instance_offset = instance_index;
                seg_meta->offset = instance_index;
                seg_meta->length = 1;
            }
            else {
                seg_meta->offset = vertices.size() / 3;
                segment->createGraphic(vertices, normals, colors);
                seg_meta->length = (vertices.size() / 3) - seg_meta->offset;
            }

            if (static_cast<bool>(seg_meta->type & m_hidden_type))
                seg_meta->hidden = true;

            layer_meta.push_back(seg_meta);
        }

        m_segments.push_back(layer_meta);
    }

    m_low_layer = 0;
    m_high_layer = gcode.size() - 1;

    m_low_segment = 0;
    m_high_segment = visibleSegmentCount();

    if (m_instanced_beads) {
        appendBoundingVertices(gcode, vertices, normals, colors);
        this->populateGL(view, vertices, normals, colors, GL_POINTS);
        this->populateInstancedBeadsGL();
    }
    else {
        this->populateGL(view, vertices, normals, colors, m_lightweight_lines ? GL_LINES : GL_TRIANGLES);
    }
}

void GCodeObject::configureUniforms() {
    GraphicsObject::configureUniforms();

    if (m_instanced_beads) {
        QVector3D stacking_axis = {GSM->getGlobal()->setting<float>(PS::Slicing::kSlicingVectorX),
                                   GSM->getGlobal()->setting<float>(PS::Slicing::kSlicingVectorY),
                                   GSM->getGlobal()->setting<float>(PS::Slicing::kSlicingVectorZ)};
        if (stacking_axis.lengthSquared() < std::numeric_limits<float>::epsilon()) {
            stacking_axis = QVector3D(0.0f, 0.0f, 1.0f);
        }

        this->view()->shaderProgram()->setUniformValue(m_shader_locs.usingInstancedGcode, true);
        this->view()->shaderProgram()->setUniformValue(m_shader_locs.stackingAxis, stacking_axis.normalized());
    }
}

std::pair<uint, uint> GCodeObject::appendInstancedBead(const QSharedPointer<SegmentBase>& segment) {
    const float width = segment->displayWidth();
    const float height = segment->displayHeight();

    uint group_index = 0;
    for (; group_index < m_instanced_bead_groups.size(); ++group_index) {
        const QSharedPointer<InstancedBeadGroup>& group = m_instanced_bead_groups[group_index];
        if (std::abs(group->width - width) < 0.000001f && std::abs(group->height - height) < 0.000001f) {
            break;
        }
    }

    if (group_index == m_instanced_bead_groups.size()) {
        QSharedPointer<InstancedBeadGroup> group = QSharedPointer<InstancedBeadGroup>::create();
        group->width = width;
        group->height = height;
        createInstancedBeadTemplate(width, height, group->template_vertices, group->template_normals);
        group->template_vertex_count = group->template_vertices.size() / 3;
        m_instanced_bead_groups.push_back(group);
    }

    QSharedPointer<InstancedBeadGroup>& group = m_instanced_bead_groups[group_index];
    const uint instance_index = group->instances.size() / kInstanceFloatCount;
    const QVector3D start = segment->start().toQVector3D();
    const QVector3D delta = displayEnd(segment) - start;
    const QColor color = segment->color();

    group->instances.insert(group->instances.end(), {start.x(), start.y(), start.z(), delta.x(), delta.y(), delta.z(),
                                                     color.redF(), color.greenF(), color.blueF(), color.alphaF()});

    return std::make_pair(group_index, instance_index);
}

void GCodeObject::populateInstancedBeadsGL() {
    this->view()->makeCurrent();
    this->view()->shaderProgram()->bind();

    for (QSharedPointer<InstancedBeadGroup>& group : m_instanced_bead_groups) {
        group->vao = QSharedPointer<QOpenGLVertexArrayObject>::create();
        group->vao->create();
        group->vao->bind();

        group->template_vbo.create();
        group->template_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        group->template_vbo.bind();
        group->template_vbo.allocate(group->template_vertices.data(), group->template_vertices.size() * sizeof(float));
        this->view()->glEnableVertexAttribArray(0);
        this->view()->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        group->template_nbo.create();
        group->template_nbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        group->template_nbo.bind();
        group->template_nbo.allocate(group->template_normals.data(), group->template_normals.size() * sizeof(float));
        this->view()->glEnableVertexAttribArray(1);
        this->view()->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        group->instance_vbo.create();
        group->instance_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
        group->instance_vbo.bind();
        group->instance_vbo.allocate(group->instances.data(), group->instances.size() * sizeof(float));

        this->view()->glEnableVertexAttribArray(4);
        this->view()->glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, kInstanceStrideBytes,
                                            reinterpret_cast<const void*>(kInstanceStartOffset * sizeof(float)));
        this->view()->glVertexAttribDivisor(4, 1);

        this->view()->glEnableVertexAttribArray(5);
        this->view()->glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, kInstanceStrideBytes,
                                            reinterpret_cast<const void*>(kInstanceDeltaOffset * sizeof(float)));
        this->view()->glVertexAttribDivisor(5, 1);

        this->view()->glEnableVertexAttribArray(6);
        this->view()->glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, kInstanceStrideBytes,
                                            reinterpret_cast<const void*>(kInstanceColorOffset * sizeof(float)));
        this->view()->glVertexAttribDivisor(6, 1);

        group->vao->release();
        group->template_vbo.release();
        group->template_nbo.release();
        group->instance_vbo.release();
    }

    this->view()->shaderProgram()->release();
}

void GCodeObject::hideSegmentType(SegmentDisplayType type, bool hide) {
    m_hidden_type = (hide) ? (m_hidden_type | type) : (m_hidden_type & ~type);

    for (auto& layer : m_segments) {
        for (auto& segment : layer) {
            if (static_cast<bool>(segment->type & m_hidden_type))
                segment->hidden = true;
            else if (segment->hidden)
                segment->hidden = false;
        }
    }
}

void GCodeObject::showSegments(uint low_segment, uint high_segment) {
    uint max = visibleSegmentCount();
    if (low_segment < 0 || high_segment > max)
        return;

    m_high_segment = high_segment;
    m_low_segment = low_segment;

    uint count = 0;
    for (int i = m_low_layer; i <= m_high_layer; i++) {
        for (int j = 0; j < m_segments[i].size(); j++) {
            if (count < low_segment || count > high_segment ||
                static_cast<bool>(m_segments[i][j]->type & m_hidden_type)) {
                m_segments[i][j]->hidden = true;
            }
            else {
                m_segments[i][j]->hidden = false;
            }
            ++count;
        }
    }
}

void GCodeObject::showLowSegment(uint low_segment) { this->showSegments(low_segment, m_high_segment); }

void GCodeObject::showHighSegment(uint high_segment) { this->showSegments(m_low_segment, high_segment); }

void GCodeObject::showLayers(uint low_layer, uint high_layer) {
    if (low_layer < 0 || high_layer > m_segments.size())
        return;

    m_low_layer = low_layer;
    m_high_layer = high_layer;
}

void GCodeObject::showLow(uint low_layer) { this->showLayers(low_layer, m_high_layer); }

void GCodeObject::showHigh(uint high_layer) { this->showLayers(m_low_layer, high_layer); }

void GCodeObject::selectSegment(uint line_number) {

    for (auto& layer : m_segments) {
        if (layer.isEmpty() || layer.back()->line < line_number)
            continue;

        for (auto& seg : layer) {
            if (seg->line == line_number) {
                seg->current_color = QColor(Qt::yellow);
                this->paintSegment(seg, QColor(Qt::yellow));
                m_selected_segments.insert(line_number, seg);

                m_segment_info_control->addSegmentInfo(line_number);
                return;
            }
        }
    }
}

void GCodeObject::deselectSegment(uint line_number) {

    if (m_selected_segments.contains(line_number)) {
        QSharedPointer<SegmentDisplayMeta> seg_meta = m_selected_segments[line_number];
        m_selected_segments.remove(line_number);
        seg_meta->current_color = seg_meta->original_color;
        this->paintSegment(seg_meta, seg_meta->original_color);

        m_segment_info_control->removeSegmentInfo(line_number);
    }
}

QList<int> GCodeObject::deselectAll() {
    QList<int> lines_to_remove;
    lines_to_remove.reserve(m_selected_segments.size());
    for (QSharedPointer<SegmentDisplayMeta> seg_meta : m_selected_segments.values()) {
        seg_meta->current_color = seg_meta->original_color;
        this->paintSegment(seg_meta, seg_meta->original_color);
        lines_to_remove.push_back(seg_meta->line - 1);

        m_segment_info_control->removeSegmentInfo(seg_meta->line);
    }
    m_selected_segments.clear();
    return lines_to_remove;
}

void GCodeObject::highlightSegment(uint line_number) {

    if (!m_highlighted_segment.isNull()) {
        if (m_highlighted_segment->line == line_number)
            return;
        else
            this->paintSegment(m_highlighted_segment, m_highlighted_segment->current_color);
    }

    QSharedPointer<SegmentDisplayMeta> seg_meta;
    for (auto& layer : m_segments) {
        if (layer.isEmpty() || layer.back()->line < line_number)
            continue;

        for (auto& seg : layer) {
            if (seg->line == line_number) {
                seg_meta = seg;
                goto search_break;
            }
        }
    }
search_break:

    if (!seg_meta.isNull()) {
        m_highlighted_segment = seg_meta;
        this->paintSegment(m_highlighted_segment, m_highlighted_segment->current_color.lighter());
    }
    else {
        if (!m_highlighted_segment.isNull()) {
            this->paintSegment(m_highlighted_segment, m_highlighted_segment->current_color);
            m_highlighted_segment = seg_meta;
        }
    }
}

uint GCodeObject::visibleSegmentCount() {
    uint sum = 0;

    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        sum += m_segments[i].size();
    }

    return sum;
}

bool GCodeObject::isCurrentlySelected(int line_num) { return m_selected_segments.contains(line_num); }

const QVector<std::pair<uint, std::vector<Triangle>>> GCodeObject::segmentTriangles() {
    QVector<std::pair<uint, std::vector<Triangle>>> ret;

    if (m_lightweight_lines || m_instanced_beads) {
        return ret;
    }

    QMatrix4x4 transform = this->transformation();
    const std::vector<float>& vert = this->vertices();

    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        for (QSharedPointer<SegmentDisplayMeta> seg : m_segments[i]) {
            if (seg->hidden)
                continue;
            // For each segment, get its triangles.
            std::vector<Triangle> seg_tri;
            Triangle current_triangle;

            uint seg_start = seg->offset * 3;
            uint seg_end = (seg->offset + seg->length) * 3;

            for (uint i = seg_start; i < seg_end; i += 9) {
                current_triangle.a = transform * QVector3D(vert[i + 0], vert[i + 1], vert[i + 2]);

                current_triangle.b = transform * QVector3D(vert[i + 3], vert[i + 4], vert[i + 5]);

                current_triangle.c = transform * QVector3D(vert[i + 6], vert[i + 7], vert[i + 8]);

                seg_tri.push_back(current_triangle);
            }

            ret.push_back(std::make_pair(seg->line, seg_tri));
        }
    }

    return ret;
}

void GCodeObject::draw() {
    if (m_instanced_beads) {
        drawInstancedBeads();
        return;
    }

    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        uint run_offset = 0;
        uint run_length = 0;

        auto drawRun = [&]() {
            if (run_length > 0) {
                this->view()->glDrawArrays(this->renderMode(), run_offset, run_length);
                run_length = 0;
            }
        };

        for (const auto& segment : m_segments[i]) {
            if (segment->hidden) {
                drawRun();
                continue;
            }

            if (run_length > 0 && run_offset + run_length == segment->offset) {
                run_length += segment->length;
            }
            else {
                drawRun();
                run_offset = segment->offset;
                run_length = segment->length;
            }
        }

        drawRun();
    }
}

void GCodeObject::drawInstancedBeads() {
    constexpr uint invalid_group = std::numeric_limits<uint>::max();

    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        uint run_group = invalid_group;
        uint run_first_instance = 0;
        uint run_instance_count = 0;

        auto drawRun = [&]() {
            if (run_instance_count > 0) {
                drawInstancedBeadRun(run_group, run_first_instance, run_instance_count);
                run_instance_count = 0;
                run_group = invalid_group;
            }
        };

        for (const auto& segment : m_segments[i]) {
            if (segment->hidden) {
                drawRun();
                continue;
            }

            if (run_instance_count > 0 && run_group == segment->instance_group &&
                run_first_instance + run_instance_count == segment->instance_offset) {
                ++run_instance_count;
            }
            else {
                drawRun();
                run_group = segment->instance_group;
                run_first_instance = segment->instance_offset;
                run_instance_count = 1;
            }
        }

        drawRun();
    }
}

void GCodeObject::drawInstancedBeadRun(uint group_index, uint first_instance, uint instance_count) {
    if (group_index >= m_instanced_bead_groups.size() || instance_count == 0) {
        return;
    }

    const QSharedPointer<InstancedBeadGroup>& group = m_instanced_bead_groups[group_index];
    const std::uintptr_t first_instance_bytes =
        static_cast<std::uintptr_t>(first_instance) * static_cast<std::uintptr_t>(kInstanceStrideBytes);

    group->vao->bind();
    group->instance_vbo.bind();

    this->view()->glVertexAttribPointer(
        4, 3, GL_FLOAT, GL_FALSE, kInstanceStrideBytes,
        reinterpret_cast<const void*>(first_instance_bytes + (kInstanceStartOffset * sizeof(float))));
    this->view()->glVertexAttribPointer(
        5, 3, GL_FLOAT, GL_FALSE, kInstanceStrideBytes,
        reinterpret_cast<const void*>(first_instance_bytes + (kInstanceDeltaOffset * sizeof(float))));
    this->view()->glVertexAttribPointer(
        6, 4, GL_FLOAT, GL_FALSE, kInstanceStrideBytes,
        reinterpret_cast<const void*>(first_instance_bytes + (kInstanceColorOffset * sizeof(float))));

    this->view()->glDrawArraysInstanced(GL_TRIANGLES, 0, group->template_vertex_count, instance_count);

    group->instance_vbo.release();
    group->vao->release();
}

void GCodeObject::paintSegment(QSharedPointer<GCodeObject::SegmentDisplayMeta> seg_meta, QColor color) {
    if (m_instanced_beads) {
        paintInstancedBead(seg_meta, color);
        return;
    }

    std::vector<float> new_colors;
    new_colors.resize(seg_meta->length * 4, 0.0f);

    for (uint i = 0; i < seg_meta->length; i++) {
        new_colors[(4 * i) + 0] = color.redF();
        new_colors[(4 * i) + 1] = color.greenF();
        new_colors[(4 * i) + 2] = color.blueF();
        new_colors[(4 * i) + 3] = color.alphaF();
    }

    this->updateColors(new_colors, seg_meta->offset * 4);
}

void GCodeObject::paintInstancedBead(QSharedPointer<GCodeObject::SegmentDisplayMeta> seg_meta, QColor color) {
    if (seg_meta->instance_group >= m_instanced_bead_groups.size()) {
        return;
    }

    QSharedPointer<InstancedBeadGroup>& group = m_instanced_bead_groups[seg_meta->instance_group];
    const uint color_offset = (seg_meta->instance_offset * kInstanceFloatCount) + kInstanceColorOffset;
    if (color_offset + 3 >= group->instances.size()) {
        return;
    }

    group->instances[color_offset + 0] = color.redF();
    group->instances[color_offset + 1] = color.greenF();
    group->instances[color_offset + 2] = color.blueF();
    group->instances[color_offset + 3] = color.alphaF();

    group->instance_vbo.bind();
    group->instance_vbo.write(color_offset * sizeof(float), group->instances.data() + color_offset, 4 * sizeof(float));
    group->instance_vbo.release();
}
} // namespace ORNL
