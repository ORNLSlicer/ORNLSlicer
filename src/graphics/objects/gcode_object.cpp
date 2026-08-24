#include "graphics/objects/gcode_object.h"

#include <GL/gl.h>

#include <QOpenGLShaderProgram>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmatrix4x4.h>
#include <qnamespace.h>
#include <qopenglbuffer.h>
#include <qopenglvertexarrayobject.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/segment_base.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/bezier.h"
#include "graphics/base_view.h"
#include "graphics/graphics_object.h"
#include "utilities/enums.h"
#include "widgets/gcode_info_control.h"

namespace ORNL {
namespace {
//! @brief Vertex counts emitted by ShapeFactory for each full bead mesh segment type.
constexpr qsizetype kLinearBeadVertexCount   = 120;
constexpr qsizetype kCurvedBeadVertexCount   = 1980;
constexpr double kLightweightArcSegmentAngle = (2.0 * 3.14159265358979323846) / 48.0;
constexpr double kLightweightArcEpsilon      = 1.0e-6;

//! @brief Counts display segments before GL buffer construction so oversized gcode can use a lighter path.
qsizetype countSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    qsizetype count = 0;
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) { count += layer.size(); }
    return count;
}

//! @brief Returns true when any segment needs bead mesh geometry in normal rendering.
bool hasMeshSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (!static_cast<bool>(segment->displayType() & SegmentDisplayType::kTravel)) { return true; }
        }
    }

    return false;
}

//! @brief Counts travel segments before GL buffer construction so their secondary line buffer can be preallocated.
qsizetype countTravelSegments(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode) {
    qsizetype count = 0;
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (static_cast<bool>(segment->displayType() & SegmentDisplayType::kTravel)) { ++count; }
        }
    }

    return count;
}

//! @brief Appends one lightweight GL_LINES edge.
void appendLightweightLineEdge(const QVector3D& start, const QVector3D& end, const QColor& color,
                               std::vector<float>& vertices, std::vector<float>& normals, std::vector<float>& colors) {
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

        colors.push_back(color.redF());
        colors.push_back(color.greenF());
        colors.push_back(color.blueF());
        colors.push_back(color.alphaF());
    }
}

int lightweightArcSegmentCount(const ArcSegment& arc) {
    const double sweep = arc.angle()();
    if (!std::isfinite(sweep) || sweep <= kLightweightArcEpsilon) { return 1; }

    return std::max(1, static_cast<int>(std::ceil(sweep / kLightweightArcSegmentAngle)));
}

//! @brief Appends a curved GL_LINES approximation for an arc segment.
void appendLightweightArc(const ArcSegment& arc, const QColor& color, std::vector<float>& vertices,
                          std::vector<float>& normals, std::vector<float>& colors) {
    const Point start   = arc.start();
    const Point center  = arc.center();
    const Point end     = arc.end();
    const double radius = std::hypot(start.x() - center.x(), start.y() - center.y());
    const double sweep  = arc.angle()();

    if (!std::isfinite(radius) || !std::isfinite(sweep) || radius <= kLightweightArcEpsilon ||
        sweep <= kLightweightArcEpsilon) {
        appendLightweightLineEdge(start.toQVector3D(), end.toQVector3D(), color, vertices, normals, colors);
        return;
    }

    const int segment_count   = lightweightArcSegmentCount(arc);
    const double start_angle  = std::atan2(start.y() - center.y(), start.x() - center.x());
    const double signed_sweep = arc.counterclockwise() ? sweep : -sweep;
    const double z_delta      = end.z() - start.z();

    QVector3D previous = start.toQVector3D();
    for (int i = 1; i <= segment_count; ++i) {
        QVector3D current;
        if (i == segment_count) { current = end.toQVector3D(); }
        else {
            const double t     = static_cast<double>(i) / static_cast<double>(segment_count);
            const double angle = start_angle + (signed_sweep * t);
            current = QVector3D(center.x() + (radius * std::cos(angle)), center.y() + (radius * std::sin(angle)),
                                start.z() + (z_delta * t));
        }

        appendLightweightLineEdge(previous, current, color, vertices, normals, colors);
        previous = current;
    }
}

//! @brief Appends a GL_LINES representation using the parser's display-space geometry.
void appendLightweightLine(const QSharedPointer<SegmentBase>& segment, std::vector<float>& vertices,
                           std::vector<float>& normals, std::vector<float>& colors) {
    const QColor color = segment->color();
    if (const auto* arc = dynamic_cast<ArcSegment*>(segment.data())) {
        appendLightweightArc(*arc, color, vertices, normals, colors);
        return;
    }

    appendLightweightLineEdge(segment->start().toQVector3D(), segment->end().toQVector3D(), color, vertices, normals,
                              colors);
}
}  // namespace

qsizetype GCodeObject::estimateTrueWidthVertexCount(const QVector<QVector<QSharedPointer<SegmentBase>>>& gcode,
                                                    qsizetype limit) {
    qsizetype count = 0;
    for (const QVector<QSharedPointer<SegmentBase>>& layer : gcode) {
        for (const QSharedPointer<SegmentBase>& segment : layer) {
            if (static_cast<bool>(segment->displayType() & SegmentDisplayType::kTravel)) { continue; }

            if (dynamic_cast<ArcSegment*>(segment.data()) != nullptr ||
                dynamic_cast<BezierSegment*>(segment.data()) != nullptr) {
                count += kCurvedBeadVertexCount;
            }
            else { count += kLinearBeadVertexCount; }

            if (count > limit) { return count; }
        }
    }

    return count;
}

GCodeObject::GCodeObject(BaseView* view, QVector<QVector<QSharedPointer<SegmentBase>>> gcode,
                         QSharedPointer<GCodeInfoControl> segmentInfoControl, bool use_true_widths,
                         GCodePreviewMode preview_mode, qsizetype true_width_vertex_threshold,
                         bool update_segment_info) {
    std::vector<float> primary_vertices;
    std::vector<float> primary_normals;
    std::vector<float> primary_colors;
    std::vector<float> travel_line_vertices;
    std::vector<float> travel_line_normals;
    std::vector<float> travel_line_colors;

    m_segment_info_control = segmentInfoControl;
    m_updates_segment_info = update_segment_info;
    if (m_updates_segment_info && !m_segment_info_control.isNull()) { m_segment_info_control->setGCode(gcode); }

    const qsizetype segment_count     = countSegments(gcode);
    const qsizetype vertex_threshold  = true_width_vertex_threshold < 0 ? 0 : true_width_vertex_threshold;
    const bool true_widths_requested  = use_true_widths && preview_mode != GCodePreviewMode::kThinLines;
    const bool auto_threshold_limited = preview_mode != GCodePreviewMode::kTrueWidths;
    const qsizetype estimate_limit = auto_threshold_limited ? vertex_threshold : std::numeric_limits<qsizetype>::max();
    const qsizetype mesh_vertex_count = true_widths_requested ? estimateTrueWidthVertexCount(gcode, estimate_limit) : 0;
    m_lightweight_lines   = !true_widths_requested || (auto_threshold_limited && mesh_vertex_count > vertex_threshold);
    m_primary_render_mode = (m_lightweight_lines || !hasMeshSegments(gcode)) ? GL_LINES : GL_TRIANGLES;

    if (m_lightweight_lines) {
        primary_vertices.reserve(segment_count * 2 * 3);
        primary_normals.reserve(segment_count * 2 * 3);
        primary_colors.reserve(segment_count * 2 * 4);
    }
    else if (m_primary_render_mode == GL_TRIANGLES) {
        primary_vertices.reserve(mesh_vertex_count * 3);
        primary_normals.reserve(mesh_vertex_count * 3);
        primary_colors.reserve(mesh_vertex_count * 4);

        const qsizetype travel_segment_count = countTravelSegments(gcode);
        travel_line_vertices.reserve(travel_segment_count * 2 * 3);
        travel_line_normals.reserve(travel_segment_count * 2 * 3);
        travel_line_colors.reserve(travel_segment_count * 2 * 4);
    }
    else {
        primary_vertices.reserve(segment_count * 2 * 3);
        primary_normals.reserve(segment_count * 2 * 3);
        primary_colors.reserve(segment_count * 2 * 4);
    }

    m_segments.reserve(gcode.size());

    for (auto& layer : gcode) {
        QVector<QSharedPointer<SegmentDisplayMeta>> layer_meta;
        layer_meta.reserve(layer.size());

        for (auto& segment : layer) {
            QSharedPointer<SegmentDisplayMeta> seg_meta = QSharedPointer<SegmentDisplayMeta>::create();
            seg_meta->layer                             = segment->layerNumber();
            seg_meta->line                              = segment->lineNumber();
            seg_meta->type                              = segment->displayType();
            seg_meta->original_color                    = segment->color();
            seg_meta->current_color                     = segment->color();

            const bool render_as_line =
                m_lightweight_lines || static_cast<bool>(segment->displayType() & SegmentDisplayType::kTravel);
            const bool use_primary_buffer = m_lightweight_lines || !render_as_line || m_primary_render_mode == GL_LINES;
            seg_meta->buffer = use_primary_buffer ? SegmentRenderBuffer::kPrimary : SegmentRenderBuffer::kTravelLine;

            if (use_primary_buffer) {
                seg_meta->offset = primary_vertices.size() / 3;
                if (render_as_line) {
                    appendLightweightLine(segment, primary_vertices, primary_normals, primary_colors);
                }
                else { segment->createGraphic(primary_vertices, primary_normals, primary_colors); }
                seg_meta->length = (primary_vertices.size() / 3) - seg_meta->offset;
            }
            else {
                seg_meta->offset = travel_line_vertices.size() / 3;
                appendLightweightLine(segment, travel_line_vertices, travel_line_normals, travel_line_colors);
                seg_meta->length = (travel_line_vertices.size() / 3) - seg_meta->offset;
            }

            if (static_cast<bool>(seg_meta->type & m_hidden_type)) seg_meta->hidden = true;

            layer_meta.push_back(seg_meta);
        }

        m_segments.push_back(layer_meta);
    }

    m_low_layer  = 0;
    m_high_layer = gcode.size() - 1;

    m_low_segment  = 0;
    m_high_segment = visibleSegmentCount();

    this->populateGL(view, primary_vertices, primary_normals, primary_colors, m_primary_render_mode);
    this->populateTravelLineGL(view, travel_line_vertices, travel_line_normals, travel_line_colors);
}

GCodeObject::~GCodeObject() {
    if (m_view.isNull() || m_view->context() == nullptr) return;

    m_view->makeCurrent();

    if (!m_travel_line_vao.isNull()) { m_travel_line_vao->destroy(); }

    m_travel_line_vbo.destroy();
    m_travel_line_nbo.destroy();
    m_travel_line_cbo.destroy();
    m_travel_line_tbo.destroy();
}

void GCodeObject::populateTravelLineGL(BaseView* view, const std::vector<float>& vertices,
                                       const std::vector<float>& normals, const std::vector<float>& colors) {
    if (vertices.empty()) { return; }

    m_travel_line_vertices = vertices;
    m_travel_line_normals  = normals;
    m_travel_line_colors   = colors;
    m_travel_line_uv.resize((m_travel_line_vertices.size() / 3) * 2, 0.0f);

    view->makeCurrent();
    view->shaderProgram()->bind();

    m_travel_line_vao = QSharedPointer<QOpenGLVertexArrayObject>::create();
    m_travel_line_vao->create();
    m_travel_line_vao->bind();

    m_travel_line_vbo.create();
    m_travel_line_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_travel_line_vbo.bind();
    m_travel_line_vbo.allocate(m_travel_line_vertices.data(), m_travel_line_vertices.size() * sizeof(float));
    view->shaderProgram()->enableAttributeArray(m_shader_locs.vertice);
    view->shaderProgram()->setAttributeBuffer(m_shader_locs.vertice, GL_FLOAT, 0, 3);

    m_travel_line_nbo.create();
    m_travel_line_nbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_travel_line_nbo.bind();
    m_travel_line_nbo.allocate(m_travel_line_normals.data(), m_travel_line_normals.size() * sizeof(float));
    view->shaderProgram()->enableAttributeArray(m_shader_locs.normal);
    view->shaderProgram()->setAttributeBuffer(m_shader_locs.normal, GL_FLOAT, 0, 3);

    m_travel_line_cbo.create();
    m_travel_line_cbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_travel_line_cbo.bind();
    m_travel_line_cbo.allocate(m_travel_line_colors.data(), m_travel_line_colors.size() * sizeof(float));
    view->shaderProgram()->enableAttributeArray(m_shader_locs.color);
    view->shaderProgram()->setAttributeBuffer(m_shader_locs.color, GL_FLOAT, 0, 4);

    m_travel_line_tbo.create();
    m_travel_line_tbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_travel_line_tbo.bind();
    m_travel_line_tbo.allocate(m_travel_line_uv.data(), m_travel_line_uv.size() * sizeof(float));
    view->shaderProgram()->enableAttributeArray(m_shader_locs.uv);
    view->shaderProgram()->setAttributeBuffer(m_shader_locs.uv, GL_FLOAT, 0, 2);

    m_travel_line_vao->release();
    m_travel_line_vbo.release();
    m_travel_line_nbo.release();
    m_travel_line_cbo.release();
    m_travel_line_tbo.release();

    view->shaderProgram()->release();
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
    if (low_segment < 0 || high_segment > max) return;

    m_high_segment = high_segment;
    m_low_segment  = low_segment;

    uint count = 0;
    for (int i = m_low_layer; i <= m_high_layer; i++) {
        for (int j = 0; j < m_segments[i].size(); j++) {
            if (count < low_segment || count > high_segment ||
                static_cast<bool>(m_segments[i][j]->type & m_hidden_type)) {
                m_segments[i][j]->hidden = true;
            }
            else { m_segments[i][j]->hidden = false; }
            ++count;
        }
    }
}

void GCodeObject::showLowSegment(uint low_segment) {
    this->showSegments(low_segment, m_high_segment);
}

void GCodeObject::showHighSegment(uint high_segment) {
    this->showSegments(m_low_segment, high_segment);
}

void GCodeObject::showLayers(uint low_layer, uint high_layer) {
    if (low_layer < 0 || high_layer > m_segments.size()) return;

    m_low_layer  = low_layer;
    m_high_layer = high_layer;
}

void GCodeObject::showLow(uint low_layer) {
    this->showLayers(low_layer, m_high_layer);
}

void GCodeObject::showHigh(uint high_layer) {
    this->showLayers(m_low_layer, high_layer);
}

void GCodeObject::selectSegment(uint line_number) {
    for (auto& layer : m_segments) {
        if (layer.isEmpty() || layer.back()->line < line_number) continue;

        for (auto& seg : layer) {
            if (seg->line == line_number) {
                seg->current_color = QColor(Qt::yellow);
                this->paintSegment(seg, QColor(Qt::yellow));
                m_selected_segments.insert(line_number, seg);

                if (m_updates_segment_info && !m_segment_info_control.isNull()) {
                    m_segment_info_control->addSegmentInfo(line_number);
                }
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

        if (m_updates_segment_info && !m_segment_info_control.isNull()) {
            m_segment_info_control->removeSegmentInfo(line_number);
        }
    }
}

QList<int> GCodeObject::deselectAll() {
    QList<int> lines_to_remove;
    lines_to_remove.reserve(m_selected_segments.size());
    for (QSharedPointer<SegmentDisplayMeta> seg_meta : m_selected_segments.values()) {
        seg_meta->current_color = seg_meta->original_color;
        this->paintSegment(seg_meta, seg_meta->original_color);
        lines_to_remove.push_back(seg_meta->line - 1);

        if (m_updates_segment_info && !m_segment_info_control.isNull()) {
            m_segment_info_control->removeSegmentInfo(seg_meta->line);
        }
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
        if (layer.isEmpty() || layer.back()->line < line_number) continue;

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

    for (uint i = m_low_layer; i <= m_high_layer; i++) { sum += m_segments[i].size(); }

    return sum;
}

bool GCodeObject::isCurrentlySelected(int line_num) {
    return m_selected_segments.contains(line_num);
}

bool GCodeObject::isLightweight() const {
    return m_lightweight_lines;
}

const QVector<std::pair<uint, std::vector<Triangle>>> GCodeObject::segmentTriangles() {
    QVector<std::pair<uint, std::vector<Triangle>>> ret;

    if (m_primary_render_mode != GL_TRIANGLES) { return ret; }

    QMatrix4x4 transform           = this->transformation();
    const std::vector<float>& vert = this->vertices();

    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        for (QSharedPointer<SegmentDisplayMeta> seg : m_segments[i]) {
            if (seg->hidden || seg->buffer != SegmentRenderBuffer::kPrimary) continue;
            // For each segment, get its triangles.
            std::vector<Triangle> seg_tri;
            Triangle current_triangle;

            uint seg_start = seg->offset * 3;
            uint seg_end   = (seg->offset + seg->length) * 3;

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

const QVector<std::pair<uint, std::pair<QVector3D, QVector3D>>> GCodeObject::segmentLines() {
    QVector<std::pair<uint, std::pair<QVector3D, QVector3D>>> ret;
    QMatrix4x4 transform = this->transformation();

    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        for (QSharedPointer<SegmentDisplayMeta> seg : m_segments[i]) {
            if (seg->hidden) continue;
            if (seg->buffer == SegmentRenderBuffer::kPrimary && m_primary_render_mode != GL_LINES) continue;

            const std::vector<float>& vert =
                seg->buffer == SegmentRenderBuffer::kTravelLine ? m_travel_line_vertices : this->vertices();
            uint seg_start = seg->offset * 3;
            uint seg_end   = (seg->offset + seg->length) * 3;

            for (uint i = seg_start; i + 5 < seg_end; i += 6) {
                QVector3D start = transform * QVector3D(vert[i + 0], vert[i + 1], vert[i + 2]);
                QVector3D end   = transform * QVector3D(vert[i + 3], vert[i + 4], vert[i + 5]);
                ret.push_back(std::make_pair(seg->line, std::make_pair(start, end)));
            }
        }
    }

    return ret;
}

void GCodeObject::drawBufferRuns(SegmentRenderBuffer buffer, ushort render_mode) {
    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        uint run_offset = 0;
        uint run_length = 0;

        auto drawRun = [&]() {
            if (run_length > 0) {
                this->view()->glDrawArrays(render_mode, run_offset, run_length);
                run_length = 0;
            }
        };

        for (const auto& segment : m_segments[i]) {
            if (segment->hidden || segment->buffer != buffer) {
                drawRun();
                continue;
            }

            if (run_length > 0 && run_offset + run_length == segment->offset) { run_length += segment->length; }
            else {
                drawRun();
                run_offset = segment->offset;
                run_length = segment->length;
            }
        }

        drawRun();
    }
}

void GCodeObject::draw() {
    drawBufferRuns(SegmentRenderBuffer::kPrimary, m_primary_render_mode);

    if (m_travel_line_vao.isNull()) { return; }

    this->vao()->release();
    m_travel_line_vao->bind();
    drawBufferRuns(SegmentRenderBuffer::kTravelLine, GL_LINES);
    m_travel_line_vao->release();
    this->vao()->bind();
}

void GCodeObject::updateTravelLineColors(std::vector<float>& colors, uint whence) {
    if (m_travel_line_vao.isNull()) { return; }

    m_travel_line_cbo.bind();

    const uint end_count = colors.size() + whence;
    if (end_count > m_travel_line_colors.size()) {
        m_travel_line_colors.resize(end_count);

        m_travel_line_cbo.allocate(end_count);
        m_travel_line_cbo.write(0, m_travel_line_colors.data(), (whence + 1) * sizeof(float));
    }

    memcpy(m_travel_line_colors.data() + whence, colors.data(), colors.size() * sizeof(float));

    m_travel_line_cbo.write(whence * sizeof(float), m_travel_line_colors.data() + whence,
                            colors.size() * sizeof(float));
    m_travel_line_cbo.release();
}

void GCodeObject::paintSegment(QSharedPointer<GCodeObject::SegmentDisplayMeta> seg_meta, QColor color) {
    std::vector<float> new_colors;
    new_colors.resize(seg_meta->length * 4, 0.0f);

    for (uint i = 0; i < seg_meta->length; i++) {
        new_colors[(4 * i) + 0] = color.redF();
        new_colors[(4 * i) + 1] = color.greenF();
        new_colors[(4 * i) + 2] = color.blueF();
        new_colors[(4 * i) + 3] = color.alphaF();
    }

    if (seg_meta->buffer == SegmentRenderBuffer::kTravelLine) {
        updateTravelLineColors(new_colors, seg_meta->offset * 4);
    }
    else { this->updateColors(new_colors, seg_meta->offset * 4); }
}
}  // namespace ORNL
