#include "step/step.h"

#include <QFileInfo>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon_list.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/island/island_base.h"
#include "utilities/enums.h"

namespace ORNL {
Step::Step(const QSharedPointer<SettingsBase>& sb) : m_sb(sb), m_dirty_bit(true) {
    // NOP
}

QSharedPointer<SettingsBase> Step::getSb() const { return m_sb; }

void Step::setSb(const QSharedPointer<SettingsBase>& sb) { m_sb = sb; }

void Step::setDirtyBit(bool dirty) { m_dirty_bit = dirty; }

bool Step::isDirty() { return m_dirty_bit; }

QSharedPointer<SyncManager> Step::getSync() const { return m_sync; }

void Step::setSync(const QSharedPointer<SyncManager>& sync) { m_sync = sync; }

StepType Step::getType() { return m_type; }

void Step::setType(StepType type) { m_type = type; }

void Step::flagIfDirtySettings(const QSharedPointer<SettingsBase>& sb) {
    if (m_sb->json() != sb->json()) {
        this->setDirtyBit(true);
    }
}

void Step::setOrientation(Plane slicing_plane, Point shift) {
    m_slicing_plane = slicing_plane;
    m_shift_amount = shift;
}

void Step::setGeometry(const PolygonList& geometry, const QVector3D& averageNormal) { m_geometry = geometry; }

void Step::addIsland(IslandType type, QSharedPointer<IslandBase> island) {
    m_islands.insert(static_cast<int>(type), island);
}

void Step::updateIslands(IslandType type, QVector<QSharedPointer<IslandBase>> islands) {
    m_islands.remove(static_cast<int>(type));

    for (QSharedPointer<IslandBase> isl : islands) {
        m_islands.insert(static_cast<int>(type), isl);
    }
}

const PolygonList& Step::getGeometry() const { return m_geometry; }

QVector3D Step::getNormal() { return m_slicing_plane.normal(); }

Point Step::getShift() { return m_shift_amount; }

Plane Step::getSlicingPlane() { return m_slicing_plane; }

void Step::setCompanionFileLocation(QDir path) {
    path.setNameFilters(QStringList() << "*.dat");
    path.setFilter(QDir::Files);

    for (QString oldFile : path.entryList()) {
        path.remove(oldFile);
    }

    m_path = path;
}

QList<QSharedPointer<IslandBase>> Step::getIslands(IslandType type) {
    if (type == IslandType::kAll) {
        return m_islands.values();
    }
    else {
        return m_islands.values(static_cast<int>(type));
    }
}

void Step::setRaftShift(QVector3D shift) {
    m_raft_shift = shift;
    m_shift_amount += shift;
}

QVector3D Step::getRaftShift() { return m_raft_shift; }
} // namespace ORNL
