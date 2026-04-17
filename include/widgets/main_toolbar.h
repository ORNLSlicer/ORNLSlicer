#pragma once

#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <qobject.h>
#include <qsize.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "utilities/enums.h"

namespace ORNL {
/*!
 * \brief a widget that displays various controls for views in the program
 */
class MainToolbar : public QToolBar {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param optional parent pointer
    MainToolbar(QWidget* parent = nullptr);

  signals:
    //! \brief signals when the view is changed by the tabs on the toolbar
    //! \param index: the index of the tab selected
    void viewChanged(int index);

    //! \brief signals when the slice button was pressed
    void slice();

    //! \brief signals when the export gcode button was pressed
    void exportGCode();

    //! \brief signals when the slicing plane should/ shouldn't be displayed
    //! \param checked: if the planes should be displayed
    void showSlicingPlanes(bool checked);

    //! \brief signals when the overhang should/ shouldn't be displayed
    //! \param checked: if overhang should be displayed
    void showOverhang(bool checked);

    //! \brief signals when the overhang should/ shouldn't be displayed
    //! \param checked: if overhang should be displayed
    void showSeams(bool checked);

    //! \brief signals when the part names should/ shouldn't be displayed
    //! \param checked: if the part names should be displayed
    void showLabels(bool checked);

    //! \brief signals when the 2D button is hit while in the GCode view
    //! \param checked: if the view should use an orthographic projection
    void setOrthoGcode(bool checked);

    //! \brief shows/ hides ghost models
    //! \param value the status
    void showGhosts(bool value);

    //! \brief shows/ hides Segment / Bead info
    //! \param show, toggle show / hide
    void showSegmentInfo(bool show);

    //! \brief signals that a new part needs to be loaded
    //! \param mt: the type of part/ mesh this will become (ie build or settings)
    void loadModel(MeshType mt);

  public slots:
    //! \brief sets the tab selected in the toolbar
    //! \param index: the index of the view
    void setView(int index);

    //! \brief updates the toolbar position and size based on parent resize event
    //! \param new_size: the size of the parent
    void resize(QSize new_size);

    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief enables/ disables the slicing button
    //! \param status: if the slicing button is enabled
    void setSliceAbility(bool status);

    //! \brief enables/ disables the export Gcode button
    //! \param status: if the export button is enabled
    void setExportAbility(bool status);

    //! \brief updates seams button from settings
    //! \param setting_key: the new settings
    void handleModifiedSetting(const QString& setting_key);

  private:
    //! \brief sets up the widget
    void setup();

    //! \brief sets up all the subwidgets
    void setupSubWidgets();

    //! \brief builds and returns a tab group
    //! \return a constructed tab bar
    QTabBar* buildTabs();

    //! \brief builds and returns a icon-only button
    //! \param icon_loc the path to the icon
    //! \param tooltip the tooltip of the icon
    //! \param toggle if this button acts as a toggle
    //! \return a constructed button
    QToolButton* buildIconButton(const QString& icon_loc, const QString& tooltip, bool toggle);

    //! \brief builds and returns the add menu
    //! \return a built add menu
    QMenu* buildAddMenu();

    //! \brief builds and returns the shape menu
    //! \return a built shape menu
    QMenu* buildShapeMenu();

    //! \brief enables the correct buttons based on the view index
    void enableCorrectOptions();

    //! \brief opens a dialog that asks for a new part/ mesh name, double checks that it's not already used
    //! \return the new name
    QString promptForName();

    //! \brief opens a dialog that asks for a number to be filled
    //! \param label the label for the dialog
    //! \param unit_text the text to display for the unit
    //! \param unit_conversion the conversion value to apply
    //! \param ok flag that is set if the user cancels
    //! \return the size converted
    double promptForSize(const QString& label, const QString& unit_text, const double unit_conversion, bool& ok);

    //! \brief the parent widget
    QWidget* m_parent;

    //! \brief a tab bar that allows switching views
    QTabBar* m_tabs;

    //! \brief toolbar buttons
    QToolButton* m_add_btn;
    QToolButton* m_shape_btn;
    QToolButton* m_slicing_planes_btn;
    QToolButton* m_seam_btn;
    QToolButton* m_overhang_button;
    QToolButton* m_billboarding_button;
    QToolButton* m_segment_info_button;
    QToolButton* m_2d_gcode_btn;
    QToolButton* m_show_ghosts_btn;
    QToolButton* m_export_gcode_btn;
    QToolButton* m_slice_btn;
};
} // namespace ORNL
