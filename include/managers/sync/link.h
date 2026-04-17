#pragma once

#include <QMutex>
#include <QSharedPointer>
#include <QVariant>
#include <QWaitCondition>

namespace ORNL {
/*!
 * \brief a class to link together steps for synchronization
 * \warning each instance of this class is guarded by a mutex that must first be locked before using
 */
class Link {
  public:
    //! \brief Constructor
    Link();

    //! \brief gets the wait condition
    //! \return the QWaitCondition used to sync
    QSharedPointer<QWaitCondition> getCondition();

    //! \brief gets the mutex used for thread safe accing of this class
    //! \return a QMutex instance
    QSharedPointer<QMutex> getMutex();

    //! \brief gets the payload of the links. This can be any data type that is registered as a QMetaType
    //! \return a qvariant that contains the payload
    QVariant getPayload();

    //! \brief sets the payload of the link
    //! \note for the variant to be valid, it must be registered using:
    //! \code Q_DECLARE_METATYPE(TYPE_NAME); \endcode
    //! \param payload: what to pass in the link
    void setPayload(QVariant payload);

    //! \brief has this link already been woken up
    //! \note this can happen if a thread calls wake on the wait condition before the destination thread is waiting
    //! \return if the link has already be triggered
    bool isWoke();

    //! \brief sets the link to triggered(woke) or not
    //! \param woke:
    void setWoke(bool woke);

  private:
    //! \brief condition used to sync two active threads
    QSharedPointer<QWaitCondition> m_cond;

    //! \brief used to gaurd data in a concurrent manor
    QSharedPointer<QMutex> m_mutex;

    //! \brief the data to pass through the link
    QVariant m_payload;

    //! \brief if the thread has been woke/ triggered
    bool m_woke = false;
};
} // namespace ORNL
