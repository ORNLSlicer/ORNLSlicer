#pragma once

#include <utility>
#include <vector>

#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qsharedpointer.h>
#include <qtypes.h>

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

  protected:
    //! \brief Overridden draw call to allow segment hiding.
    void draw();

  private:
    //! \brief Segment metadata.
    struct SegmentDisplayMeta {
        //! \brief Segment location in GL buffer.
        uint offset = 0;
        //! \brief Segment length in GL buffer.
        uint length = 0;

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
