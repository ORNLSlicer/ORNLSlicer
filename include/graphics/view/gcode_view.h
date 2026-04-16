#pragma once

#include <QVector>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmap.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/segment_base.h"
#include "graphics/base_view.h"
#include "graphics/objects/gcode_object.h"
#include "utilities/enums.h"
#include "widgets/gcode_info_control.h"
#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {
// Forward
class PrinterObject;

/*!
 * \brief View that displays generated GCode and provides interactivity with it.
 *
 * The GCode View has only one object that it renders (besides the printer): GCodeObject. The
 * view's main role is that of manager for this object.
 */
class GCodeView : public BaseView {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param sb: Settings to use.
    //! \param segmentInfoControl: Segment info display control
    GCodeView(QSharedPointer<SettingsBase> sb, QSharedPointer<GCodeInfoControl> segmentInfoControl);

  public slots:
    //! \brief Changes the view to use an orthographic projection instead of the normal perspective view.
    void useOrthographic(bool ortho);

    //! \brief Generates and renders a list of segments.
    //! \param gcode: Segments of GCode. Each outer vector is a layer, each inner is a specific segment.
    void addGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode);

    //! \brief Hides segments of a certain type.
    //! \param type: Type of segment to hide.
    //! \param hidden: If this type should be hidden.
    void hideSegmentType(SegmentDisplayType type, bool hidden);

    //! \brief updates segment with regular or reduced width
    //! \param use_true_width if the true width should be used
    void updateSegmentWidths(bool use_true_width);

    //! \brief Updates the printer based on changes to the settings.
    //! \param sb: New settings object.
    void updatePrinterSettings(QSharedPointer<SettingsBase> sb);

    //! \brief Sets the lowest layer to show.
    void setLowLayer(uint low_layer);
    //! \brief Sets the highest layer to show.
    void setHighLayer(uint high_layer);

    //! \brief Sets the lowest segment to show.
    void setLowSegment(uint low_segment);
    //! \brief Sets the highest layer to show.
    void setHighSegment(uint high_segment);

    //! \brief Updates segments. Adjusts index as segments are 1-based
    //! while widget's block number is 0-based
    //! \param linesToAdd: lines to select
    //! \param lineToRemove: lines to deselect
    void updateSegments(QList<int> linesToAdd, QList<int> linesToRemove);

    //! \brief Resets the view and deletes the GCodeObject.
    void clear();

    //! \brief sets the part meta model that is tracked
    //! \param meta a pointer to the part meta from part widget
    void setMeta(QSharedPointer<PartMetaModel> meta);

    //! \brief shows/ hides ghosted models
    //! \param show status of models
    void showGhosts(bool show);

    //! \brief Moves the camera to its default zoom and orientation.
    virtual void resetCamera() override;

  signals:
    //! \brief Signal that the passed lines were selected/deselected
    //! \param linesToAdd: lines to select
    //! \param lineToRemove: lines to deselect
    void updateSelectedSegments(QList<int> linesToAdd, QList<int> linesToRemove);

    //! \brief Signal that the max segments has changed
    //! \param max: new max segments
    void maxSegmentChanged(uint max);

  protected:
    //! \brief Initalizes the view with the printer and the associated objects.
    void initView() override;

    //! \brief Alters the view matrix depending on the projection type and the new view size.
    void resizeGL(int width, int height) override;

    //! \brief Handles the translation of the printer in the volume due to camera movement.
    //! \param v: Translation vector
    //! \param absolute: If this translation is relative to (0, 0, 0).
    void translateCamera(QVector3D v, bool absolute) override;

    //! \brief Handles the following: Segment deselection
    void handleLeftClick(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Segment selection
    void handleLeftDoubleClick(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Orthographic rotation blocking
    void handleRightMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Segment hover highlighting
    void handleMouseMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Orthographic in zoom
    void handleWheelForward(QPointF mouse_ndc_pos, float delta) override;

    //! \brief Handles the following: Orthographic out zoom
    void handleWheelBackward(QPointF mouse_ndc_pos, float delta) override;

  private:
    //! \brief Picks a segment based on the mouse position.
    //! \param mouse_ndc_pos: Mouse normalized location.
    //! \param gog: GCode object to search through.
    //! \return Segment line number.
    uint pickSegment(const QPointF& mouse_ndc_pos, QSharedPointer<GCodeObject> gog);

    //! \brief Settings for the view.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief Printer object in view. The GCode object is a child of this printer.
    QSharedPointer<PrinterObject> m_printer;
    //! \brief Main GCodeObject.
    QSharedPointer<GCodeObject> m_gcode_object;

    //! \brief m_meta_model tracks the states of the parts and their transformations
    QSharedPointer<PartMetaModel> m_meta_model;

    //! \brief loaded gcode
    QVector<QVector<QSharedPointer<SegmentBase>>> m_gcode;

    //! \brief if segments should be draw with real or a reduced segment width
    bool m_use_true_segment_widths = true;

    //! \brief m_ghosted_parts a map of part meta items and their respective models held in graphics
    QMap<QSharedPointer<PartMetaItem>, QSharedPointer<PartObject>> m_ghosted_parts;

    //! \brief Current view state.
    struct {
        //! \brief Lowest layer shown.
        uint low_layer = 0;
        //! \brief Highest layer shown.
        uint high_layer = 1;

        //! \brief If using the orthographic projection or not.
        bool ortho = false;
        //! \brief Current zoom level.
        float zoom_factor = 1;

        //! \brief If ghosted models are currently being displayed
        bool showing_ghosts = false;

        //! \brief Hidden segment types.
        SegmentDisplayType hidden_type = SegmentDisplayType::kNone;
    } m_state;

    //! \brief Segment / Bead info display control
    QSharedPointer<GCodeInfoControl> m_segment_info_control;
};
} // namespace ORNL
