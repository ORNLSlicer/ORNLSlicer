#include "threading/step_thread.h"

#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "step/step.h"

namespace ORNL {
StepThread::StepThread() {
    this->moveToThread(&m_internal_thread);
    m_internal_thread.start();
}

StepThread::~StepThread() {
    m_internal_thread.quit();
    m_internal_thread.wait();
}

void StepThread::setStep(const QSharedPointer<Step>& value) { m_step = value; }

void StepThread::doStep() {
    if (!m_step.isNull()) {
        m_step->compute();
    }
    emit completed();
}
} // namespace ORNL
