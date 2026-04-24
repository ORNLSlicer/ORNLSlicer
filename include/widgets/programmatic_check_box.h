#pragma once

#include <QCheckBox>
#include <qhashfunctions.h>
#include <qtmetamacros.h>
#include <qwidget.h>

namespace ORNL {
/*!
 * \class ProgrammaticCheckBox
 * \brief Custom widget for tri-state checkbox that only allows partial checking
 * via programmatic means while limiting user to checked/unchecked states.
 */
class ProgrammaticCheckBox : public QCheckBox {
    Q_OBJECT
  public:
    //! \brief Constructor.
    ProgrammaticCheckBox(QString str, QWidget* parent);

  public slots:
    //! \brief Override to enforce checked/unchecked transition
    void nextCheckState() override;
};
} // Namespace ORNL
