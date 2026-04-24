#include "managers/sync/link.h"

#include <qassert.h>
#include <qmutex.h>
#include <qsharedpointer.h>
#include <qvariant.h>
#include <qwaitcondition.h>

//! \macro checks to make sure the mutex for a link is already locked
//!        called everytime something is accessed/ updated within a link
#define VARIFY_LOCK(mutex)                                                                                             \
    { Q_ASSERT_X(!mutex->tryLock(), "sync manager", "link's mutex must be locked before accessing/ updating"); }

namespace ORNL {
Link::Link() {
    m_cond = QSharedPointer<QWaitCondition>::create();
    m_mutex = QSharedPointer<QMutex>::create();
}

QSharedPointer<QWaitCondition> Link::getCondition() {
    VARIFY_LOCK(m_mutex);
    return m_cond;
}

QSharedPointer<QMutex> Link::getMutex() { return m_mutex; }

QVariant Link::getPayload() {
    VARIFY_LOCK(m_mutex);
    return m_payload;
}

void Link::setPayload(QVariant payload) {
    VARIFY_LOCK(m_mutex);
    m_payload = payload;
}

bool Link::isWoke() {
    VARIFY_LOCK(m_mutex);
    return m_woke;
}

void Link::setWoke(bool woke) {
    VARIFY_LOCK(m_mutex);
    m_woke = woke;
}
} // namespace ORNL
