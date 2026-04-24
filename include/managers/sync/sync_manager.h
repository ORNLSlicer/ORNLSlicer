#pragma once

#include <QMap>
#include <QQueue>
#include <QString>
#include <qsharedpointer.h>
#include <qvariant.h>

#include "managers/sync/link.h"
#include "utilities/enums.h"

namespace ORNL {
/*!
 * \class SyncManager
 * \brief The SyncManager class manages synchronizing multiple threads using links.
 *        Links are create between threads and are indexed so that a link is triggered from one thread to another. This
 * indexing can be layer/ step number, thread id or an uniquely identifiable information. An enum is used to specify the
 * type of link.
 */
class SyncManager {
  public:
    //!
    //! \brief Constructor
    //!
    SyncManager();

    //!
    //! \brief adds a links from one intex to another
    //! \param from: thread that will trigger
    //! \param to: thread that will wait for trigger
    //! \param type: the type of link this is
    //!
    void addLink(int from, int to, LinkType type);

    //!
    //! \brief removes all links from the manager
    //!
    void clearLinks();

    //!
    //! \brief waits for the source thread to call its wait function and deliver the payload
    //! \arg template param is the type of payload to receive
    //! \param layer_num: the id of this thread/ step/ layer
    //! \param type: the type of link to wait on
    //! \return a payload
    //!
    template <class T> T wait(int layer_num, LinkType type) {
        // Does a link exists at this index?
        if (m_to_links.contains(layer_num)) {
            // Does a link of the this type exist?
            if (m_to_links[layer_num].contains(type)) {
                auto link = m_to_links[layer_num][type].dequeue();

                // Lock this link to access/ edit
                auto mutex = link->getMutex();
                mutex->lock();

                // If this link was woken before this thread was spawned then skip waiting
                if (!link->isWoke())
                    link->getCondition()->wait(&(*mutex));

                // Extract payload
                QVariant variant = link->getPayload();
                mutex->unlock();
                return variant.value<T>();
            }
        }

        // If no link exists return default contructor of this type
        return T();
    }

    //!
    //! \brief instructs any threads waiting on this one to wake and passes a payload
    //! \arg template param is the type of payload to send
    //! \param layer_num: the id of this thread/ step/ layer
    //! \param type: the type of the link
    //! \param payload: the data to send
    //!
    template <class T> void wake(int layer_num, LinkType type, T payload) {
        // Does a link exists at this index?
        if (m_from_links.contains(layer_num)) {
            // Does a link of the this type exist?
            if (m_from_links[layer_num].contains(type)) {
                auto link = m_from_links[layer_num][type].dequeue();

                // Lock this link to access/ edit
                auto mutex = link->getMutex();
                mutex->lock();

                // The paylod is stored using a QVariant type
                QVariant variant;
                variant.value<T>();
                variant.setValue(payload);
                link->setPayload(variant);

                // Wake any threads currently waiting and set a flag for any that have not spawned yet
                link->getCondition()->wakeAll();
                link->setWoke(true);

                // Free this link
                mutex->unlock();
            }
        }
    }

  private:
    //! \brief pointers to links that start at a given index
    QMap<int, QMap<LinkType, QQueue<QSharedPointer<Link>>>> m_from_links;

    //! \brief pointers to links that end at a given index
    QMap<int, QMap<LinkType, QQueue<QSharedPointer<Link>>>> m_to_links;
};
} // namespace ORNL
