#pragma once

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>
#include <qlist.h>
#include <qobject.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <qsize.h>
#include <qtmetamacros.h>

#include "graphics/view/part_view.h"
#include "part/part.h"
#include "widgets/part_widget/model/part_meta_model.h"
#include "widgets/part_widget/part_control/part_control.h"
#include "widgets/part_widget/part_toolbar.h"
#include "widgets/view_controls_toolbar.h"

namespace ORNL {
/*!
 * \class PartWidget
 * \brief contains UI components for part view, buttons and the part control.
 */
class PartWidget : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor
    //! \param parent: a widget
    PartWidget(QWidget* parent = nullptr);

    QSet<QSharedPointer<Part>> parts();

    //! \brief gets a pointer to the widget's part meta model
    //! \return a pointer
    QSharedPointer<PartMetaModel> getPartMeta();

    //! \brief gets name to first listed part
    //! \return string of name
    QString getFirstPartName();

  signals:
    //! \brief Signal of parts that have been selected.
    void selected(QSet<QSharedPointer<Part>> pl, QSharedPointer<Part> mp);

    //! \brief Signal that a part was added.
    void added(QSharedPointer<Part> p);

    //! \brief Signal that a part was removed.
    void removed(QSharedPointer<Part> pl);

    //! \brief Signal that a slice should be started.
    void slice();

    //! \brief Signal that the part widget was resized
    //! \param new_size the new size
    void resized(QSize new_size);

    //! \brief Signal that the part rotation message needs to be displayed in main windows status bar
    void displayRotationInfoMsg();

  public slots:
    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief Takes a screenshot of the view.
    void takeScreenshot();

    //! \brief Undoes the last action.
    void undo();

    //! \brief Redoes the last action.
    void redo();

    //! \brief Copies the selected parts.
    void copy();

    //! \brief Pastes the selected parts.
    void paste();

    //! \brief Adds a part to the control and view.
    //! \param part: a pointer to the new part
    void add(QSharedPointer<Part> part);

    //! \brief Reloads the selected parts in the view.
    void reload();

    //! \brief Deletes the selected parts from the view.
    void remove();

    //! \brief Deletes a specified part from the view.
    void remove(QSharedPointer<Part> part);

    //! \brief Remove all parts from view.
    void clear();

    //! \brief Zooms in the view.
    void zoomIn();

    //! \brief Zooms out the view.
    void zoomOut();

    //! \brief Resets zoom to default.
    void resetZoom();

    //! \brief Resets the camera to the default position.
    void resetCamera();

    //! Updates printer and slicing plane render with settings.
    //! \param setting_key: the new settings
    void handleModifiedSetting(const QString& setting_key);

    //! \brief Enables / disables the buttons for part control.
    //! \param status: the status to change
    void setEnabled(bool status);

    //! \brief Preslice update. This function exists temporarily to get current parts into the session
    //!        pending a better refactor of the whole session scheme.
    void preSliceUpdate();

    //! \brief enables/ disables showing slicing planes
    //! \param show enables/ disables
    void showSlicingPlanes(bool show);

    //! \brief enables/ disables showing labels
    //! \param show enables/ disables
    void showLabels(bool show);

    //! \brief enables/ disables showing seams
    //! \param show enables/ disables
    void showSeams(bool show);

    //! \brief enables/ disables showing overhang
    //! \param show enables/ disables
    void showOverhang(bool show);

    //! \brief updates parts in session manager with new transformation
    //! \note this is pending a better refactor
    void updatePartTransformations();

  private slots:
    //! \brief Updates to model.
    void modelAdditionUpdate(QSharedPointer<PartMetaItem> item);
    void modelSelectionUpdate(QSharedPointer<PartMetaItem> item);
    void modelTransformationUpdate(QSharedPointer<PartMetaItem> item);
    void modelRemovalUpdate(QSharedPointer<PartMetaItem> item);

    //! \brief Generate issues for positioning.
    void positionIssues(QList<QSharedPointer<Part>> opl, QList<QSharedPointer<Part>> fpl);

    //! \brief Set labels text.
    void setStatusSelection(QString name);
    void setStatusIssue(QString issue);

  private:
    //! \brief called when the widget is resized
    //! \param event: the event
    void resizeEvent(QResizeEvent* event);

    //! \brief Setup the static widgets and their layouts.
    void setupWidget();

    //! \brief 1. Setup the widgets.
    void setupSubWidgets();

    //! \brief 2. Setup the layouts.
    void setupLayouts();

    //! \brief 3. Setup the positions of the widgets.
    void setupPosition();

    //! \brief 4. Setup the inputs.
    void setupInputs();

    //! \brief 5. Setup the events for the various widgets.
    void setupEvents();

    //! \brief Layouts
    QVBoxLayout* m_layout;

    //! \brief OpenGL
    PartView* m_part_view;

    //! \brief left hand size toolbar
    PartToolbar* m_toolbar;

    //! \brief camera controls
    ViewControlsToolbar* m_view_controls;

    //! \brief Labels
    QLabel* m_selection_label;

    //! \brief Color of emphasized text
    QString m_accentColor;

    //! \brief Status label text
    struct {
        QString selected_part;
        QString issues;
    } m_status_state;

    //! \brief Part Control Widget
    PartControl* m_part_control;

    //! \brief Model for parts loaded.
    QSharedPointer<PartMetaModel> m_model;
};
} // Namespace ORNL
