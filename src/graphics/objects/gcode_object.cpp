#include "graphics/objects/gcode_object.h"

#include <GL/gl.h>

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
#include "graphics/base_view.h"
#include "graphics/graphics_object.h"
#include "utilities/enums.h"
#include "widgets/gcode_info_control.h"

namespace ORNL {
GCodeObject::GCodeObject(BaseView* view, QVector<QVector<QSharedPointer<SegmentBase>>> gcode,
                         QSharedPointer<GCodeInfoControl> segmentInfoControl) {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;

    m_segment_info_control = segmentInfoControl;
    m_segment_info_control->setGCode(gcode);

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
            seg_meta->offset = vertices.size() / 3;

            segment->createGraphic(vertices, normals, colors);

            seg_meta->length = (vertices.size() / 3) - seg_meta->offset;

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

    this->populateGL(view, vertices, normals, colors, GL_TRIANGLES);
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
    for (uint i = m_low_layer; i <= m_high_layer; i++) {
        for (const auto& segment : m_segments[i]) {
            if (segment->hidden)
                continue;

            this->view()->glDrawArrays(this->renderMode(), segment->offset, segment->length);
        }
    }
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

    this->updateColors(new_colors, seg_meta->offset * 4);
}
} // namespace ORNL
