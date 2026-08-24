#include "threading/traditional_ast.h"

#include <qdebug.h>
#include <qfiledevice.h>
#include <qhashfunctions.h>
#include <qlist.h>
#include <qlogging.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qthread.h>
#include <qtmetamacros.h>

#include "managers/session_manager.h"
#include "part/part.h"
#include "step/step.h"
#include "threading/step_thread.h"
#include "utilities/enums.h"
#include "utilities/runtime_diagnostics.h"

namespace ORNL {
TraditionalAST::TraditionalAST(QString outputLocation, bool skipGcode)
    : AbstractSlicingThread(outputLocation, skipGcode) {}

void TraditionalAST::doSlice() {
    if (CSM->parts().empty()) {
        qWarning() << "Attempted to start a slice when no data has been loaded.";
        return;
    }

    m_timer.start();

    m_step_threads.clear();
    m_step_queue.clear();

    if (!m_step_threads.isEmpty() || !m_step_queue.isEmpty()) {
        qWarning() << "Background resources are still cleaning up.  Cannot slice.";
        return;
    }

    this->setMaxSteps(0);

    Diagnostics::logLine(QStringLiteral("Slicing preprocess starting"));
    this->preProcess();
    Diagnostics::logLine(QStringLiteral("Slicing preprocess complete"));

    int total_steps = 0;
    for (QSharedPointer<Part> part : CSM->parts()) { total_steps += part->countStepPairs(); }

    // Instantiate the ideal thread amount.
    for (int i = 0, end = (total_steps > QThread::idealThreadCount() ? QThread::idealThreadCount() : total_steps);
         i < end; ++i) {
        StepThread* st = new StepThread();
        m_step_threads.push_back(st);

        QObject::connect(this, &TraditionalAST::stepStart, st, &StepThread::doStep);
        QObject::connect(st, &StepThread::completed, this, &TraditionalAST::cleanThread);
    }

    // For every selected step in every part, add the step to the queue.
    for (QSharedPointer<Part> part : CSM->parts()) {
        if (part->rootMesh()->type() == MeshType::kClipping)  // Skip parts that were used for clipping
            continue;

        QList<QSharedPointer<Step>> allSteps = part->steps();
        for (QSharedPointer<Step> step : allSteps) {
            if (step->isDirty() && !m_step_queue.contains(step)) { m_step_queue.append(step); }
        }
    }

    m_queue_start_size = m_step_queue.size();

    // For every thread available, give it a step to compute.
    for (StepThread* st : m_step_threads) {
        if (m_step_queue.empty()) break;

        QSharedPointer<Step> step = m_step_queue.dequeue();
        st->setStep(step);
    }

    if (this->shouldCancel()) {
        m_step_threads.clear();
        m_step_queue.clear();
        return;
    }

    if (m_queue_start_size == 0) {
        emit statusUpdate(StatusUpdateStepType::kCompute, 100);

        Diagnostics::logLine(QStringLiteral("Slicing compute complete; starting postprocess"));
        this->postProcess();
        Diagnostics::logLine(QStringLiteral("Slicing postprocess complete"));

        if (this->shouldCancel()) return;

        if (!m_skip_gcode) {
            // Gcode output
            Diagnostics::logLine(QStringLiteral("Slicing G-code generation starting"));
            this->writeGCodeSetup();
            this->writeGCode();
            this->writeGCodeShutdown();
            Diagnostics::logLine(QStringLiteral("Slicing G-code generation complete"));
        }
        if (this->shouldCancel()) return;

        Diagnostics::logLine(QStringLiteral("Slicing sliceComplete emitted"));
        emit sliceComplete();
    }
    else
        emit stepStart();

    // After starting steps, ensure that all threads are disconnected. Otherwise, the compute function will be recalled
    // in cleanThread().
    QObject::disconnect(this, &TraditionalAST::stepStart, nullptr, nullptr);
}

void TraditionalAST::cleanThread() {
    StepThread* st = qobject_cast<StepThread*>(QObject::sender());

    if (this->shouldCancel()) {
        m_step_threads.removeOne(st);
        st->stop();
        delete st;

        if (m_step_threads.isEmpty()) m_step_queue.clear();
    }
    else {
        if (m_queue_start_size == 0)  // If there is nothing to compute then we are done
            emit statusUpdate(StatusUpdateStepType::kCompute, 100);
        else
            emit statusUpdate(
                StatusUpdateStepType::kCompute,
                ((double)m_queue_start_size - (double)m_step_queue.size()) / (double)m_queue_start_size * 100);

        // If the queue is empty, then start destroying unused threads.
        if (m_step_queue.empty()) {
            m_step_threads.removeOne(st);
            st->stop();
            delete st;

            // If all threads have been destroyed, the slice is complete.
            if (m_step_threads.empty()) {
                Diagnostics::logLine(QStringLiteral("Slicing compute complete; starting postprocess"));
                this->postProcess();
                Diagnostics::logLine(QStringLiteral("Slicing postprocess complete"));

                m_elapsed_time = m_timer.elapsed();

                if (this->shouldCancel()) return;

                if (!m_skip_gcode) {
                    // Gcode output
                    Diagnostics::logLine(QStringLiteral("Slicing G-code generation starting"));
                    this->writeGCodeSetup();
                    this->writeGCode();
                    this->writeGCodeShutdown();
                    Diagnostics::logLine(QStringLiteral("Slicing G-code generation complete"));
                }
                if (this->shouldCancel()) return;

                Diagnostics::logLine(QStringLiteral("Slicing sliceComplete emitted"));
                emit sliceComplete();
            }

            return;
        }

        // Move the next item in the queue to the free thread.
        QSharedPointer<Step> step = m_step_queue.dequeue();
        st->setStep(step);

        // Connect the signal, start the step, and disconnect to avoid recieving unrelated signals.
        QObject::connect(this, &TraditionalAST::stepStart, st, &StepThread::doStep);
        emit stepStart();
        QObject::disconnect(this, &TraditionalAST::stepStart, st, &StepThread::doStep);
    }
}
}  // namespace ORNL
