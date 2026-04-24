#pragma once

#include <QToolBar>
#include <QToolButton>
#include <QVector3D>
#include <qkeysequence.h>
#include <qsharedpointer.h>
#include <qsize.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "units/unit.h"
#include "widgets/part_widget/input/tool_bar_align_input.h"
#include "widgets/part_widget/input/tool_bar_input.h"

namespace ORNL {
/*!
 * \class PartToolbar
 * \brief a widget toolbar that controls buttons and controls to manipulate parts
 */
class PartToolbar : public QToolBar {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param model the part(s) that are being edited
    //! \param parent optional parent pointer
    PartToolbar(QSharedPointer<PartMetaModel> model, QWidget* parent = nullptr);

    //! \brief updates the units used for translation
    //! \param new_unit: the new unit
    //! \param old_unit: the old unit
    void updateTranslationUnits(Distance new_unit, Distance old_unit);

    //! \brief updates the angle units
    //! \param new_unit: the new unit
    //! \param old_unit: the old unit
    void updateAngleUnits(Angle new_unit, Angle old_unit);

  signals:
    //! \brief signals to center parts
    void centerParts();

    //! \brief signals to drop parts to floor
    void dropPartsToFloor();

    //! \brief signals with alignment info
    //! \param plane the plane the align in on
    void setupAlignment(QVector3D plane);

  public slots:
    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief updates size and position based on parent size
    //! \param new_size the size of parent
    void resize(QSize new_size);

    //! \brief closes all control box windows
    void closeAllControls();

    //! \brief enables/ disables the buttons on the toolbar
    //! \param status enable/ disable
    void setEnabled(bool status);

  private:
    //! \brief setups base widget
    void setupWidget();

    //! \brief setups sub-widgets
    void setupSubWidgets();

    //! \brief Constructs a flexible spacer widget
    void makeSpace();

    //! \brief sets up translation button and controls
    void setupTranslation();

    //! \brief sets up rotation button and controls
    void setupRotation();

    //! \brief sets up scale button and controls
    void setupScale();

    //! \brief sets up align button and controls
    void setupAlign();

    //! \brief sets up center button
    void setupCenter();

    //! \brief sets up drop to floor button
    void setupDropToFloor();

    //! \brief builds a icon-only button
    //! \param icon_loc the icon path
    //! \param tooltip the tooltip
    //! \param toggle if the button is a toggle
    //! \return the new built button
    QToolButton* buildIconButton(const QString& icon_loc, const QString& tooltip, bool toggle);

    //! \brief optional parent
    QWidget* m_parent;

    //! \brief Model for parts loaded.
    QSharedPointer<PartMetaModel> m_model;

    // Translate
    QToolButton* m_translation_btn;
    ToolbarInput* m_translation_controls;

    // Rotate
    QToolButton* m_rotation_btn;
    ToolbarInput* m_rotation_controls;

    // Scale
    QToolButton* m_scale_btn;
    ToolbarInput* m_scale_controls;

    // Align
    QToolButton* m_align_btn;
    ToolbarAlignInput* m_align_controls;

    // Center
    QToolButton* m_center_btn;

    // Drop to floor
    QToolButton* m_drop_to_floor_btn;
};
} // namespace ORNL
