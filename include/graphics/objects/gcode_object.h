#pragma once

#include <utility>
#include <vector>

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/segment_base.h"
#include "graphics/graphics_object.h"
#include "utilities/enums.h"
#include "widgets/gcode_info_control.h"

namespace ORNL {
/*!
 * \brief Graphics that renders GCode as a single object.
 *
 * Unlike all other graphic objects, the GCodeObject acts as container object for a number of segments.
 * The reason, as explained in the based GraphicsObject class, is that having separate buffers for each
 * segment is extremely inefficient. Rendering them all using one buffer dramatically improves performance.
 * This unfortunately increases complexity of the object by a significant amount.
 */
class GCodeObject : public GraphicsObject {
  public:
    //! \brief Constructor.
    //! \param view: View to render to.
    //! \param gcode: GCode segments to visualize.
    //! \param segmentInfoControl: Segment / Bead info display control
    GCodeObject(BaseView* view, QVector<QVector<QSharedPointer<SegmentBase>>> gcode,
                QSharedPointer<GCodeInfoControl> segmentInfoControl);

    //! \brief Hides/Show all segments matching a type.
    //! \param type: Type to hide/show.
    //! \param hide: Hidden or not.
    void hideSegmentType(SegmentDisplayType type, bool hide);

    //! \brief Shows layers between low and high
    //! \param low_layer: Low display.
    //! \param high_layer: high_display.
    void showLayers(uint low_layer, uint high_layer);
    //! \brief Shows down to low_layer.
    void showLow(uint low_layer);
    //! \brief Shows up to high_layer.
    void showHigh(uint high_layer);

    //! \brief Shows segments between low and high
    //! \param low_segment: Low display.
    //! \param high_segment: high_display.
    void showSegments(uint low_segment, uint high_segment);
    //! \brief Shows down to low_segment.
    void showLowSegment(uint low_segment);
    //! \brief Shows up to high_segment.
    void showHighSegment(uint high_segment);

    //! \brief Selects the segment.
    //! \param line_number: line number of segment to select/change color
    void selectSegment(uint line_number);

    //! \brief Deselects the segment.
    //! \param line_number: line number of segment to deselect/change color
    void deselectSegment(uint line_number);

    //! \brief Deselects all segments
    //! \return List of segments that were deselected
    QList<int> deselectAll();

    //! \brief Highlights the segment with the line number.
    //! \param line_number: line number of segment to highlight during hover
    void highlightSegment(uint line_number);

    //! \brief Gets the number of segments that are currently visible.
    uint visibleSegmentCount();

    //! \brief Determines whether a segment is selected
    //! \param line_num: line number of segment to test
    //! \return whether or not the segment is selected/has color change
    bool isCurrentlySelected(int line_num);

    //! \brief Triangles that compose the segments of this object.
    //! \return Pairs of (layer number, Triangles) for each segment.
    const QVector<std::pair<uint, std::vector<Triangle>>> segmentTriangles();

    //! \brief Picks a line segment in optimized render modes that do not keep CPU triangles.
    //! \param projection: Current view projection matrix.
    //! \param view: Current camera view matrix.
    //! \param mouse_ndc_pos: Mouse position in normalized device coordinates.
    //! \param ortho: If the current projection is orthographic.
    //! \return G-code line number, or 0 when no segment is close enough.
    uint pickSegment(const QMatrix4x4& projection, const QMatrix4x4& view, const QPointF& mouse_ndc_pos, bool ortho);

    //! \brief Allows the object to enable instanced gcode uniforms when needed.
    void configureUniforms() override;

  protected:
    //! \brief Overridden draw call to allow segment hiding.
    void draw() override;

  private:
    //! \brief Segment metadata.
    struct SegmentDisplayMeta {
        //! \brief Segment location in GL buffer.
        uint offset = 0;
        //! \brief Segment length in GL buffer.
        uint length = 0;
        //! \brief Instanced bead template group, when using instanced rendering.
        uint instance_group = 0;
        //! \brief Offset in the group's instance buffer, when using instanced rendering.
        uint instance_offset = 0;
        //! \brief Segment start for optimized picking.
        QVector3D pick_start;
        //! \brief Segment end for optimized picking.
        QVector3D pick_end;
        //! \brief Approximate selection radius in display units.
        float pick_radius = 0.0f;

        //! \brief If this segment is hidden.
        bool hidden = false;
        //! \brief Segment type.
        SegmentDisplayType type = SegmentDisplayType::kLine;
        //! \brief Segment color.
        QColor original_color;
        QColor current_color;

        //! \brief Layer this segment belongs to.
        uint layer;
        //! \brief GCode line this segment corresponds to.
        uint line;

        bool operator==(const SegmentDisplayMeta& rhs) const {
            return offset == rhs.offset && length == rhs.length && hidden == rhs.hidden && type == rhs.type &&
                   original_color == rhs.original_color && current_color == rhs.current_color && layer == rhs.layer &&
                   line == rhs.line;
        }
    };

    //! \brief Paints a segment different color.
    //! \param seg_meta: Segment to paint.
    //! \param color: Color to paint.
    void paintSegment(QSharedPointer<SegmentDisplayMeta> seg_meta, QColor color);

    //! \brief Instanced bead data for one display-width/display-height shape.
    struct InstancedBeadGroup {
        float width = 0.0f;
        float height = 0.0f;
        uint template_vertex_count = 0;

        std::vector<float> template_vertices;
        std::vector<float> template_normals;
        std::vector<float> instances;

        QSharedPointer<QOpenGLVertexArrayObject> vao;
        QOpenGLBuffer template_vbo {QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer template_nbo {QOpenGLBuffer::VertexBuffer};
        QOpenGLBuffer instance_vbo {QOpenGLBuffer::VertexBuffer};
    };

    //! \brief Adds an instance record and returns (group index, instance index).
    std::pair<uint, uint> appendInstancedBead(const QSharedPointer<SegmentBase>& segment);

    //! \brief Creates GL buffers for all instanced bead groups.
    void populateInstancedBeadsGL();

    //! \brief Draws visible instanced bead runs.
    void drawInstancedBeads();

    //! \brief Draws a contiguous run from one instanced bead group.
    void drawInstancedBeadRun(uint group_index, uint first_instance, uint instance_count);

    //! \brief Updates one instanced bead's color in CPU and GL buffers.
    void paintInstancedBead(QSharedPointer<SegmentDisplayMeta> seg_meta, QColor color);

    //! \brief True when very large gcode is rendered as lightweight GL lines instead of bead meshes.
    bool m_lightweight_lines = false;

    //! \brief True when line gcode is rendered with a shared bead mesh and per-segment instances.
    bool m_instanced_beads = false;

    //! \brief Shared bead template groups keyed by display width/height.
    std::vector<QSharedPointer<InstancedBeadGroup>> m_instanced_bead_groups;

    //! \brief Segment metadata container.
    QVector<QVector<QSharedPointer<SegmentDisplayMeta>>> m_segments;

    //! \brief Lowest layer shown.
    uint m_low_layer = 0;
    //! \brief Highest layer shown.
    uint m_high_layer = 1;

    //! \brief Lowest segment shown.
    uint m_low_segment = 0;
    //! \brief Highest segment shown.
    uint m_high_segment = 1;
    //! \brief Offset for lowest possible segment
    uint m_segment_offset = 0;

    //! \brief Currently selected segment.
    QHash<int, QSharedPointer<SegmentDisplayMeta>> m_selected_segments;

    //! \brief Currently highlighted segment.
    QSharedPointer<SegmentDisplayMeta> m_highlighted_segment;

    //! \brief Hidden segment types.
    SegmentDisplayType m_hidden_type = SegmentDisplayType::kNone;

    //! \brief Segment / Bead info display control
    QSharedPointer<GCodeInfoControl> m_segment_info_control;
};
} // namespace ORNL
