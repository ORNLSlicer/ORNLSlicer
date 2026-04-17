#pragma once

#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qqueue.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "threading/abs_slicing_thread.h"
#include "threading/step_thread.h"

namespace ORNL {
/*!
 * \class TraditionalAST
 * \brief Defines the abstract implementation of the slicing thread.
 * \note For more information about the abstract slicing architecture, see the documentation.
 */
class TraditionalAST : public AbstractSlicingThread {
    Q_OBJECT
  public:
    //! \brief Constructor
    TraditionalAST(QString outputLocation, bool skipGcode = false);

  public slots:
    //! \brief Main function that starts slice.
    void doSlice() override;

  signals:
    //! \brief Signal to the active steps to begin computation.
    void stepStart();

  protected slots:
    //! \brief Upon completion of thread running step object, this slot will clean up the thread.
    //!        If more objects are on the queue, then this will run the thread
    void cleanThread() override;

  private:
    //! \brief Queue of steps to be processed.
    QQueue<QSharedPointer<Step>> m_step_queue;

    //! \brief Step threads to run.
    QVector<StepThread*> m_step_threads;

    //! \brief Start size of processing queue.  Used to avoid evaluting .size repeatedly
    int m_queue_start_size;
};
} // namespace ORNL
