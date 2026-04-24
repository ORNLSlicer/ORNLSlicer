#pragma once

#include <QFont>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QWidget>
#include <qhashfunctions.h>
#include <qtmetamacros.h>

namespace ORNL {

/*!
 * \class RemoveButton
 * \brief Widget that defines the hide/delete button on the top-right of the header.
 * Qt does not signal transition events for nested child widgets, so this button must provide
 * state to the header for purposes of drawing.  If the cursor is hovering over the button,
 * the header is no longer highlighted.
 */
class RemoveButton : public QPushButton {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! isDelete Whether or not button signals delete or hide when clicked
    //! parent Parent widget
    RemoveButton(bool isDelete, QWidget* parent);

    //! \brief Whether or not cursor is inside button bounds
    //! \return boolean whether or not cursor is inside button bounds
    bool isInside();

  signals:
    //! \brief Emitted to header whenever the cursor is within the button
    //! Signals header to update coloring as Qt does not signal transition events for nested
    //! child widgets
    void childTransition();

  private:
    //! \brief Whether the cursor is within the bounds of the button or not
    //! Qt does not signal events for nested child widgets
    bool m_inside;
};

/*!
 * \class SettingHeader
 * \brief Widget that defines the header on the tab (the rounded box you click on).
 *
 * The relationship between the the various settings widgets is as follows:
 * SettingsBar - Main container for all widgets. Has a tab widget which contains SettingPanes and seach bar.
 *      SettingsPane - Contains a scroll area used to display SettingsTabs.
 *          SettingsTab - Contains a SettingsHeader(the thing you click on to expand the tab) and a container
 *                        for all settings under that category.
 */
class SettingHeader : public QFrame {
    Q_OBJECT
  public:
    /*!
     * \brief Constructor
     * \param parent    The parent of this object (for QObject).
     * \param name      The name of the tab (usually the minor category).
     * \param icon      The icon for the tab.
     * \param canDelete Whether or not the header will delete or hide itself (default = hide)
     */
    explicit SettingHeader(QWidget* parent = nullptr, QString name = QString(), QIcon icon = QIcon(),
                           bool canDelete = false);

    //! \brief Sets the dimensions and style of the setting header frame.
    void setupWidget();

    //! \brief Constructs the widgets within the setting header frame and the layout that holds the subwidgets.
    //! Subwidgets include the icon, the label text, the expand/collapse arrow, and the hide button.
    void setupSubWidgets();

    /*!
     * \brief Set the tab's name.
     * \param new_name  Name of the tab.
     */
    void setName(QString new_name);

    /*!
     * \brief Set the tab's icon.
     * \param new_icon  New icon to display.
     */
    void setIcon(QIcon new_icon);

    /*!
     * \brief Set the status of the tab. Status is if the tab is expanded or not.
     * \param status    If the tab should be expanded or not.
     */
    void setStatus(bool status);

    //! Trigger header to show itself
    void showHeader();

  signals:
    //! \brief Signal that the tab was expanded.
    void expand();

    //! \brief Signal that the tab was shrunk.
    void shrink();

    //! \brief Signal setting tab of header's deletion
    void deleteHeader();

    //! \brief Signal setting tab of header's hide
    void hideHeader();

  public slots:
    //! \brief Update background after a transition (remove or add highlighting)
    void updateBackground();

    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief Either delete or hide header based on constructor settings
    void deleteOrHideHeader();

  private:
    // Qt Overrides.
    void mousePressEvent(QMouseEvent* event);

    //! \brief Title of the setting header
    QString m_name;

    //! \brief Current icon of the setting header
    QIcon m_icon;

    //! \brief Frame that holds the pixmap of the current icon
    QLabel* m_picture;

    //! \brief Label that holds the title text
    QLabel* m_label;

    //! \brief Frame that holds the expand/collapse arrows
    QLabel* m_arrow;

    //! \brief Font of the label text
    QFont font;

    //! \brief Internal status of the setting header
    bool m_status;

    //! \brief whether or not the header can delete itself or just hide itself
    bool m_can_delete;

    //! \brief Remove/hide button in the top right corner of header
    RemoveButton* m_remove;
};

} // Namespace ORNL
