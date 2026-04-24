#pragma once

#include <QVBoxLayout>
#include <QWidget>
#include <qcontainerfwd.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qsize.h>
#include <qtmetamacros.h>

#include "geometry/segment_base.h"
#include "graphics/view/gcode_view.h"
#include "widgets/gcode_info_control.h"
#include "widgets/part_widget/model/part_meta_model.h"
#include "widgets/view_controls_toolbar.h"

namespace ORNL {
/*!
 * \brief Contains the GCodeView for display of GCode.
 */
class GCodeWidget : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param parent: A widget.
    GCodeWidget(QWidget* parent = nullptr);

    //! \brief Get the view for this widget.
    GCodeView* view();

    //! \brief sets the part meta model to track
    //! \param meta the model the track
    void setPartMeta(QSharedPointer<PartMetaModel> meta);

  signals:
    //! \brief signals when this widget has been resized
    //! \param size
    void resized(QSize size);

  public slots:
    //! \brief Adds the GCode to the view after it has been loaded.
    void addGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode);
    //! \brief Clears the GCode in the view.
    void clear();
    //! \brief Uses an orthographic projection in place of the standard perspective.
    void setOrthoView(bool status);

    //! \brief shows/ hides Segment / Bead info in the gcode view
    //! \param show, toggle show / hide
    void showSegmentInfo(bool status);

    //! \brief shows ghost models in the gcode view
    //! \param status show/ hide ghost
    void showGhosts(bool status);

    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief Updates printer settings.
    void handleModifiedSetting(QString key);

    //! \brief Zooms in the view.
    void zoomIn();

    //! \brief Zooms out the view.
    void zoomOut();

    //! \brief Resets zoom to default.
    void resetZoom();

    //! \brief Resets the camera to the default position.
    void resetCamera();

  private:
    //! \brief Setup the static widgets and their layouts.
    void setupWidget();

    //! \brief 1. Setup the widgets.
    void setupSubWidgets();
    //! \brief 2. Setup the layouts.
    void setupLayouts();
    //! \brief 3. Setup the positions of the widgets.
    void setupPosition();
    //! \brief 4. Setup the events for the various widgets.
    void setupEvents();

    //! \brief automatically called when widget resizes
    //! \param event the resize event
    void resizeEvent(QResizeEvent* event);

    //! \brief OpenGL view that displays gcode
    GCodeView* m_gcode_view;

    //! \brief layout of the widget
    QVBoxLayout* m_layout;

    //! \brief camera controls
    ViewControlsToolbar* m_view_controls;

    //! \brief Segment / Bead info control
    QSharedPointer<GCodeInfoControl> m_segment_info_control;
};
} // namespace ORNL
