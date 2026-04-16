#include "managers/sync/sync_manager.h"

#include <qassert.h>
#include <qsharedpointer.h>

#include "managers/sync/link.h"
#include "utilities/enums.h"

namespace ORNL {
SyncManager::SyncManager() {}

void SyncManager::addLink(int from, int to, LinkType type) {
    Q_ASSERT_X(from < to, "sync link creation", "backward links are forbidden");
    Q_ASSERT_X(from != to, "sync link creation", "self-links are forbidden");

    QSharedPointer<Link> link = QSharedPointer<Link>::create();

    m_from_links[from][type].enqueue(link);
    m_to_links[to][type].enqueue(link);
}

void SyncManager::clearLinks() {
    m_from_links.clear();
    m_to_links.clear();
}
} // namespace ORNL
