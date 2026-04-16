#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QToolButton>
#include <QVector3D>
#include <QWidget>
#include <qpoint.h>
#include <qtmetamacros.h>

namespace ORNL {
/*!
 * \class ToolbarAlignInput
 * \brief Provides a list of buttons in a box to align the part
 */
class ToolbarAlignInput : public QFrame {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param parent: the part widget this sits on
    explicit ToolbarAlignInput(QWidget* parent);

    //! \brief sets the position of the widget
    //! \param pos the new position
    void setPos(QPoint pos);

  public slots:
    //! \brief closes this input immediately(no animation)
    void closeInput();

    //! \brief toggles displaying box
    void toggleInput();

    //! \brief setup stylesheets for controlling theme
    void setupStyle();

  signals:
    //! \brief signals when to start alignment for a given direction
    //! \param dir direction to align along
    void setAlignment(QVector3D dir);

  private:
    //! \brief setups the widget
    void setupWidget();

    //! \brief setups any subwidgets (the buttons)
    void setupSubWidgets();

    //! \brief setups the layout
    void setupLayouts();

    //! \brief adds buttons to layout
    void setupInsert();

    //! \brief setups up open/ close animation
    void setupAnimations();

    //! \brief connects signals to buttons
    void setupEvents();

    //! \brief draws and returns a vertical separator using a QFrame
    //! \return a new vertical line
    QFrame* buildSeparator();

    //! \brief Tool buttons to control the align signals
    QToolButton* m_align_bottom_btn;
    QToolButton* m_align_top_btn;
    QToolButton* m_align_right_btn;
    QToolButton* m_align_left_btn;
    QToolButton* m_align_front_btn;
    QToolButton* m_align_back_btn;

    //! \brief The layout.
    QHBoxLayout* m_layout;

    //! \brief Animations
    QPropertyAnimation* m_open_ani;
    QPropertyAnimation* m_close_ani;

    // Location on parent
    QPoint m_pos;

    // Keeps track of the open status
    bool is_open = false;
};
} // Namespace ORNL
