#pragma once

#include <QObject>
#include <QSize>
#include <QToolBar>
#include <QToolButton>
#include <qtmetamacros.h>

namespace ORNL {
/*!
 * \class ViewControlsToolbar
 * \brief a toolbar widget that holds buttons to control the view camera
 */
class ViewControlsToolbar : public QToolBar {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param parent optional parent widget
    ViewControlsToolbar(QWidget* parent = nullptr);

  signals:
    //! \brief sets the camera to iso view
    void setIsoView();
    //! \brief sets the camera to front view
    void setFrontView();
    //! \brief sets the camera to side view
    void setSideView();
    //! \brief sets the camera to top view
    void setTopView();

  public slots:
    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief adjusts size and position based on parent redraw
    //! \param new_size the new parent's new size
    void resize(QSize new_size);

    void setEnabled(bool status);

  private:
    //! \brief sets up the widget and style
    void setupWidget();

    //! \brief sets up the sub widgets
    void setupSubWidgets();

    //! \brief Constructs a flexible space between buttons
    void makeSpace();

    //! \brief the parent
    QWidget* m_parent;

    QToolButton* m_iso_btn;
    QToolButton* m_front_btn;
    QToolButton* m_side_btn;
    QToolButton* m_top_btn;
};
} // namespace ORNL
