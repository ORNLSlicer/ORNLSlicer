#pragma once

#include <limits>

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
class SeamObject;

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

    //! \brief Moves the camera toward the print volume.
    void zoomIn();
    //! \brief Moves the camera away from the print volume.
    void zoomOut();
    //! \brief Moves the camera to its default zoom.
    void resetZoom() override;

    //! \brief Set camera to view from top.
    void setTopView();
    //! \brief Set camera to view from side.
    void setSideView();
    //! \brief Set camera to view from front.
    void setFrontView();
    //! \brief Set camera to view from the forward direction.
    void setForwardView();
    //! \brief Set camera to view from an isometric direction.
    void setIsoView();

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

    //! \brief Shows or hides optimization point graphics.
    void showSeams(bool show);

    //! \brief Updates optimization point graphics based on changes to the settings.
    //! \param sb: New settings object.
    void updateOptimizationSettings(QSharedPointer<SettingsBase> sb);

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
    //! \brief Notification that a draggable optimization point setting edit has started.
    void optimizationPointDragStarted(QString x_setting, QString y_setting);

    //! \brief Notification that a draggable optimization point setting edit has changed.
    void optimizationPointDragged(QString x_setting, double x, QString y_setting, double y);

    //! \brief Notification that a draggable optimization point setting edit has finished.
    void optimizationPointDragFinished(QString x_setting, double x, QString y_setting, double y);

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

    //! \brief Applies camera pan to the G-code view camera target.
    //! \param v World-space translation vector to apply.
    //! \param absolute If true, set the camera target to \p v; otherwise apply \p v as a relative pan.
    //! \note This override keeps G-code navigation centered on the printer volume.
    void translateCamera(QVector3D v, bool absolute) override;

    //! \brief Applies shared camera rotation unless the G-code view is orthographic.
    //! \param screen_delta Horizontal and vertical NDC-style delta requested by keyboard or mouse input.
    //! \note Orthographic G-code previews intentionally keep the top-down orientation locked.
    void rotateCamera(QVector2D screen_delta) override;

    //! \brief Handles the following: Segment deselection
    void handleLeftClick(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Optimization point drag
    void handleLeftMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Optimization point drag finalization
    void handleLeftRelease(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Segment selection
    void handleLeftDoubleClick(QPointF mouse_ndc_pos) override;

    //! \brief Handles mouse-drag camera rotation unless orthographic mode is active.
    //! \param mouse_ndc_pos Mouse position in normalized device coordinates.
    void handleRightMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Segment hover highlighting
    void handleMouseMove(QPointF mouse_ndc_pos) override;

    //! \brief Handles the following: Orthographic in zoom
    void handleWheelForward(QPointF mouse_ndc_pos, float delta) override;

    //! \brief Handles the following: Orthographic out zoom
    void handleWheelBackward(QPointF mouse_ndc_pos, float delta) override;

  private:
    //! \brief Enables hover tracking only when the visible segment count is small enough for interactive picking.
    void updateHoverTracking();

    //! \brief Removes ghosted part meshes from the G-code view.
    void clearGhosts();

    //! \brief Rebuilds ghosted part meshes when ghost display is enabled.
    void rebuildGhosts();

    //! \brief Removes the visible-range true-width overlay from the G-code object.
    void clearTrueWidthOverlay();

    //! \brief Rebuilds the visible-range true-width overlay when the current range is below the preference threshold.
    void refreshTrueWidthOverlay();

    //! \brief Returns a printable-segment subset for the currently visible layer and segment range.
    QVector<QVector<QSharedPointer<SegmentBase>>> visiblePrintableGCodeSubset() const;

    //! \brief Picks a segment based on the mouse position.
    //! \param mouse_ndc_pos: Mouse normalized location.
    //! \param gog: GCode object to search through.
    //! \return Segment line number.
    uint pickSegment(const QPointF& mouse_ndc_pos, QSharedPointer<GCodeObject> gog);

    //! \brief Refreshes camera-dependent segment info display.
    void updateSegmentInfoViewMatrix();

    //! \brief Begins dragging an optimization point if the cursor is over one.
    bool beginOptimizationPointDrag(QPointF mouse_ndc_pos);

    //! \brief Updates an active optimization point drag.
    bool updateOptimizationPointDrag(QPointF mouse_ndc_pos, bool finish);

    //! \brief Finishes an active optimization point drag.
    void finishOptimizationPointDrag(QPointF mouse_ndc_pos);

    //! \brief Settings for the view.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief Printer object in view. The GCode object is a child of this printer.
    QSharedPointer<PrinterObject> m_printer;
    //! \brief Main GCodeObject.
    QSharedPointer<GCodeObject> m_gcode_object;
    //! \brief Optional true-width overlay for the currently visible range when the full preview uses thin lines.
    QSharedPointer<GCodeObject> m_true_width_overlay_object;

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

        //! \brief Lowest segment shown inside the visible layer range.
        uint low_segment = 0;
        //! \brief Highest segment shown inside the visible layer range.
        uint high_segment = std::numeric_limits<uint>::max();

        //! \brief If using the orthographic projection or not.
        bool ortho = false;
        //! \brief Current zoom level.
        float zoom_factor = 1;

        //! \brief If ghosted models are currently being displayed
        bool showing_ghosts = false;

        //! \brief If optimization point graphics are shown.
        bool seams_shown = false;

        //! \brief If an optimization point is being dragged.
        bool dragging_seam = false;

        //! \brief Optimization point currently being dragged.
        QSharedPointer<SeamObject> dragged_seam;

        //! \brief X setting controlled by the dragged optimization point.
        QString dragged_seam_x_setting;

        //! \brief Y setting controlled by the dragged optimization point.
        QString dragged_seam_y_setting;

        //! \brief Cursor-to-point offset retained during optimization point drag.
        QVector3D dragged_seam_offset;

        //! \brief Hidden segment types.
        SegmentDisplayType hidden_type = SegmentDisplayType::kNone;
    } m_state;

    //! \brief Cache key for the visible-range true-width overlay.
    struct TrueWidthOverlayKey {
        uint low_layer = 0;
        uint high_layer = 0;
        uint low_segment = 0;
        uint high_segment = 0;
        GCodePreviewMode preview_mode = GCodePreviewMode::kAuto;
        int vertex_threshold = 0;
        bool use_true_widths = false;

        bool operator==(const TrueWidthOverlayKey& other) const {
            return low_layer == other.low_layer && high_layer == other.high_layer &&
                   low_segment == other.low_segment && high_segment == other.high_segment &&
                   preview_mode == other.preview_mode && vertex_threshold == other.vertex_threshold &&
                   use_true_widths == other.use_true_widths;
        }
    };

    TrueWidthOverlayKey m_true_width_overlay_key;
    bool m_true_width_overlay_key_valid = false;

    //! \brief Segment / Bead info display control
    QSharedPointer<GCodeInfoControl> m_segment_info_control;
};
} // namespace ORNL
