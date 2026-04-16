#pragma once

#include <QDialog>
#include <QProgressBar>
#include <qlist.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class SliceDialog
 * \brief Dialog that shows the current slice progress.
 */
class SliceDialog : public QDialog {
    Q_OBJECT
  public:
    //! \brief Standard constructor
    SliceDialog(QWidget* parent = nullptr);

  signals:
    //! \brief Signal slice to cancel when user clicks the button
    void cancelSlice();

  public slots:
    //! \brief Receive status updates from slicing process
    //! \param type Current step type process
    //! \param percentage Percentage completed
    void updateStatus(StatusUpdateStepType type, int percentage);

  private:
    //! \brief Setup the static widgets and their layouts.
    void setupUi();

    //! \brief List of progress bars to update via status
    QList<QProgressBar*> m_progress_bars;
};
} // namespace ORNL
