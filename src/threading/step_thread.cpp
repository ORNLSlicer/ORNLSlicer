#include "threading/step_thread.h"

#include <QMetaObject>

#include <qsharedpointer.h>
#include <qthread.h>
#include <qtmetamacros.h>

#include "step/step.h"

namespace ORNL {
StepThread::StepThread() {
    this->moveToThread(&m_internal_thread);
    m_internal_thread.start();
}

StepThread::~StepThread() {
    stop();
}

void StepThread::setStep(const QSharedPointer<Step>& value) {
    m_step = value;
}

void StepThread::stop() {
    if (!m_internal_thread.isRunning()) return;

    if (QThread::currentThread() == &m_internal_thread) {
        m_internal_thread.quit();
        return;
    }

    QMetaObject::invokeMethod(this, [this] { m_internal_thread.quit(); }, Qt::BlockingQueuedConnection);
    m_internal_thread.wait();
}

void StepThread::doStep() {
    if (!m_step.isNull()) { m_step->compute(); }
    emit completed();
}
}  // namespace ORNL
