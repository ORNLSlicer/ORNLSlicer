#pragma once

#include <QGridLayout>
#include <QLineEdit>
#include <QScrollBar>
#include <QTextEdit>
#include <QToolButton>
#include <QWidget>
#include <qobject.h>
#include <qtmetamacros.h>

namespace ORNL {
/*!
 * \class CmdWidget
 * \brief Widget that dislays text and acts as a terminal.
 * \note This class can likely be thrown out (especially if MessageHandler() is removed).
 */
class CmdWidget : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor.
    explicit CmdWidget(QWidget* parent = nullptr);

    //! \brief Stream operator for output.
    void operator<<(QString str);

    //! \brief Function for output.
    void print(QString str);

  public slots:
    //! \brief Function to run a command. This currently only appends the string if not empty.
    void runCmd(QString cmd);

    //! \brief Appends a string to the output.
    void append(QString str);

  private:
    //! \brief Setup the static widgets and their layouts.
    void setupWidget();

    //! \brief Step 1: Setup the widgets.
    void setupSubWidgets();
    //! \brief Step 2: Setup the positions of the widgets.
    void setupLayout();
    //! \brief Step 3: Setup the insertion into layouts.
    void setupInsert();
    //! \brief Step 4: Setup the events for the various widgets.
    void setupEvents();

    // Layout
    QGridLayout* m_layout;

    // Widgets
    QTextEdit* m_output;
};
} // Namespace ORNL
