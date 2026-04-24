#pragma once

#include <QThread>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "step/step.h"

namespace ORNL {
/*!
 * \class StepThread
 * \brief Thread that Step objects are computed on.
 */
class StepThread : public QObject {
    Q_OBJECT
  public:
    //! \brief Constructor
    StepThread();

    //! \brief Destructor
    ~StepThread();

    //! \brief Set the step for operation.
    void setStep(const QSharedPointer<Step>& value);

  public slots:
    //! \brief Perfom the computation for the step in this thread.
    void doStep();

  signals:
    //! \brief Signal that computation has concluded.
    void completed();

  private:
    // Internal thread.
    QThread m_internal_thread;

    // Step to compute.
    QSharedPointer<Step> m_step;
};

} // namespace ORNL
