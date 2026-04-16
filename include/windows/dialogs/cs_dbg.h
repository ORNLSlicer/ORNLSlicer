#pragma once

#include <QDialog>
#include <QMap>
#include <QSet>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "part/part.h"

// Forward
class QTableWidget;
class QComboBox;
class QGraphicsView;
class QScrollBar;
class QHBoxLayout;
class QSpinBox;

namespace ORNL {
/*!
 * \class CsDebugDialog
 * \brief Debugger dialog to view the cross sectional geometry of any loaded part in any plane.
 */
class CsDebugDialog : public QDialog {
    Q_OBJECT
  public:
    //! \brief Constructor.
    explicit CsDebugDialog(QWidget* parent = nullptr);

  private slots:
    //! \brief Paint graphics view.
    void paintGraphicsView(int height = -1);
    //! \brief Get updates from combo boxes.
    void updateAxis(int idx);
    //! \brief Change view to selection.
    void selectFromRow(int row, int col);
    //! \brief Change the layer painted.
    void changeLayer(int height);

  private:
    //! \brief Change the current part.
    void changePart(QSharedPointer<Part> part);
    //! \brief Update the scrollbar.
    void updateScroll();

    //! \brief Setup the static widgets and their layouts.
    void setupUi();

    //! \brief 1. Setup the window properties.
    void setupWindow();
    //! \brief 2. Setup the widgets.
    void setupWidgets();
    //! \brief 3. Setup the layouts and insert their children.
    void setupLayouts();
    //! \brief 4. Setup the table from the master json.
    void setupTable();
    //! \brief 5. Setup the insertions for all UI elements.
    void setupInsert();
    //! \brief 6. Setup the events for the various widgets.
    void setupEvents();

    //! \brief Current part
    QSharedPointer<Part> m_part;

    int m_scroll_min;
    int m_scroll_max;

    //! \brief Lookup part from combobox.
    QMap<QComboBox*, QSharedPointer<Part>> m_part_lookup;

    // Table
    QTableWidget* m_table;

    // Scroll
    QScrollBar* m_scrollbar;
    QSpinBox* m_spinbox;

    // Layout
    QHBoxLayout* m_layout;

    // Graphics View
    QGraphicsView* m_view;
};
} // namespace ORNL
