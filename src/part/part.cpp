#include "part/part.h"

#include <qassert.h>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmap.h>
#include <qmatrix4x4.h>
#include <qminmax.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <quuid.h>

#include "configs/settings_range.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/mesh/open_mesh.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/layer.h"
#include "step/layer/scan_layer.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
Part::Part() {
    m_sync = QSharedPointer<SyncManager>::create();
    m_sb = QSharedPointer<SettingsBase>::create();
    m_uuid = QUuid::createUuid();
}

Part::Part(const QSharedPointer<Part>& p) {
    // Copy root and sub meshes
    auto closed_mesh = dynamic_cast<ClosedMesh*>(p->m_root_mesh.get());
    if (closed_mesh != nullptr) {
        m_root_mesh = QSharedPointer<ClosedMesh>::create(*closed_mesh);
    }
    else {
        m_root_mesh = QSharedPointer<OpenMesh>::create(*dynamic_cast<OpenMesh*>(p->m_root_mesh.get()));
    }

    for (QSharedPointer<MeshBase> sub : p->m_sub_meshes) {
        auto closed_sub_mesh = dynamic_cast<ClosedMesh*>(sub.get());
        if (closed_sub_mesh != nullptr) {
            m_sub_meshes.push_back(QSharedPointer<ClosedMesh>::create(*closed_sub_mesh));
        }
        else {
            m_sub_meshes.push_back(QSharedPointer<OpenMesh>::create(*dynamic_cast<OpenMesh*>(sub.get())));
        }
    }

    m_name = p->m_name;
    m_file_name = p->m_file_name;

    m_sb = QSharedPointer<SettingsBase>::create(*p->getSb());

    m_template_applied = false;
    m_current_part_template = "";

    m_uuid = QUuid::createUuid();
}

Part::Part(QSharedPointer<MeshBase> root_mesh, QString file_name, MeshType mt) {
    m_file_name = file_name;
    m_mesh_type = mt;
    m_name = root_mesh->name();
    m_root_mesh = root_mesh;
    m_sync = QSharedPointer<SyncManager>::create();
    m_sb = QSharedPointer<SettingsBase>::create();
    m_template_applied = false;

    m_uuid = QUuid::createUuid();
}

QSharedPointer<SettingsBase> Part::getSb() const { return m_sb; }

QMap<uint, QSharedPointer<SettingsRange>> Part::getSettingsRanges() { return m_ranges; }

void Part::createSettingsRange(int low, int high, QString group_name) {
    int min = qMin(low, high);
    int max = qMax(low, high);
    // if the range is in an exisitng group, use the group's settings base
    QSharedPointer<SettingsBase> sb = nullptr;
    if (group_name.length() > 0) {
        for (auto i = m_ranges.begin(), end = m_ranges.end(); i != end; ++i) {
            if (i.value()->groupName() == group_name) {
                sb = i.value()->getSb();
                break;
            }
        }
    }
    QSharedPointer<SettingsRange> new_range;
    if (sb != nullptr)
        new_range = QSharedPointer<SettingsRange>::create(min, max, group_name, sb); // use found sb
    else
        new_range = QSharedPointer<SettingsRange>::create(
            min, max, group_name, QSharedPointer<SettingsBase>::create(*m_sb)); // make new sb as a copy of the part's

    m_ranges[MathUtils::cantorPair(min, max)] = new_range;
    m_range_from_template[MathUtils::cantorPair(min, max)] =
        false; // not from template. Set corresponding index to false.
}

void Part::createSettingsRange(int low, int high, QSharedPointer<SettingsBase> sb, QString group_name) {
    int min = qMin(low, high);
    int max = qMax(low, high);
    QSharedPointer<SettingsRange> new_range = QSharedPointer<SettingsRange>::create(min, max, group_name, sb);
    m_ranges[MathUtils::cantorPair(min, max)] = new_range;
    m_range_from_template[MathUtils::cantorPair(min, max)] = true; // from template
    m_current_part_template = GSM->getCurrentTemplate();
    m_template_applied = true;
}

QSharedPointer<SettingsRange> Part::getSettingsRange(int low, int high) {
    int min = qMin(low, high);
    int max = qMax(low, high);
    return m_ranges[MathUtils::cantorPair(min, max)];
}

bool Part::getTemplateApplied() { return m_template_applied; }

void Part::setCurrentPartTemplate(QString current_template) { m_current_part_template = current_template; }

QString Part::getCurrentPartTemplate() { return m_current_part_template; }

QMap<uint, bool> Part::getRangesFromTemplate() { return m_range_from_template; }

bool Part::currentPartTemplateEqualToSetTemplate(QString set_template) {
    return m_current_part_template == set_template;
}

void Part::removeSettingsRange(int low, int high) {
    int min = qMin(low, high);
    int max = qMax(low, high);
    m_ranges.remove(MathUtils::cantorPair(min, max));
}

void Part::clearSettingsRanges() { m_ranges.clear(); }

json Part::SettingsRangesToJson() {
    json output;

    for (auto& range : m_ranges) {
        json range_json;
        range_json[Constants::Settings::Session::Range::kLow] = range->low();
        range_json[Constants::Settings::Session::Range::kHigh] = range->high();
        range_json[Constants::Settings::Session::Range::kName] = range->groupName();
        range_json[Constants::Settings::Session::Range::kSettings] = range->getSb()->json();
        output.push_back(range_json);
    }

    return output;
}

void Part::loadRangesFromJson(json input) {
    m_ranges.clear();
    for (auto& range_json : input) {
        int low = range_json[Constants::Settings::Session::Range::kLow];
        int high = range_json[Constants::Settings::Session::Range::kHigh];
        QString group = range_json[Constants::Settings::Session::Range::kName];
        auto sb = QSharedPointer<SettingsBase>::create();
        sb->json(range_json[Constants::Settings::Session::Range::kSettings]);
        createSettingsRange(low, high, sb, group);
    }
}

void Part::splitSettingsRange(int low, int high) {
    // create new single ranges for endpoints with copies of setttings
    // then delete the range
    int min = qMin(low, high);
    int max = qMax(low, high);
    QSharedPointer<SettingsBase> sb = m_ranges[MathUtils::cantorPair(min, max)]->getSb();

    QSharedPointer<SettingsRange> new_range_low = QSharedPointer<SettingsRange>::create(min, min, "", sb);
    m_ranges[MathUtils::cantorPair(min, min)] = new_range_low;

    QSharedPointer<SettingsRange> new_range_high = QSharedPointer<SettingsRange>::create(max, max, "", sb);
    m_ranges[MathUtils::cantorPair(max, max)] = new_range_high;

    removeSettingsRange(min, max);
}

void Part::updateSettingsRangeLimits(int old_low, int old_high, int new_low, int new_high) {
    // get the old one, to use its sb and group name
    int old_min = qMin(old_low, old_high);
    int old_max = qMax(old_low, old_high);
    QSharedPointer<SettingsRange> old = getSettingsRange(old_min, old_max);

    // make a new range at the new location
    int min = qMin(new_low, new_high);
    int max = qMax(new_low, new_high);
    createSettingsRange(min, max, old->getSb(), old->groupName());

    // delete the old range
    removeSettingsRange(old_min, old_max);
}

void Part::setRootMesh(QSharedPointer<MeshBase> mesh) { m_root_mesh = mesh; }

void Part::setRootMesh(const QVector<MeshVertex>& vertices, const QVector<MeshFace>& faces) {
    m_root_mesh = QSharedPointer<ClosedMesh>(new ClosedMesh(vertices, faces));
}

QSharedPointer<MeshBase> Part::rootMesh() { return m_root_mesh; }

QVector<QSharedPointer<MeshBase>> Part::subMeshes() { return m_sub_meshes; }

void Part::appendSubMesh(QSharedPointer<MeshBase> mesh) { m_sub_meshes.push_back(mesh); }

void Part::clearSubMeshes() { m_sub_meshes.clear(); }

void Part::scaleSubMeshes() {
    for (auto mesh : m_sub_meshes) {
        mesh->scaleUniform(this->m_root_mesh->dimensions());
    }
}

bool Part::containsSubMesh(QString name) {
    for (QSharedPointer<MeshBase> mesh : m_sub_meshes) {
        if (name == mesh->name())
            return true;
    }

    return false;
}

QVector<QSharedPointer<MeshBase>> Part::meshes() {
    QVector<QSharedPointer<MeshBase>> ret = m_sub_meshes;
    ret.prepend(m_root_mesh);

    return ret;
}

void Part::setTransformation(const QMatrix4x4& mtrx) {
    // Only apply if the root mesh was changed
    if (!qFuzzyCompare(mtrx, m_root_mesh->transformation())) {
        m_root_mesh->setTransformation(mtrx);

        for (auto mesh : m_sub_meshes) {
            mesh->setTransformation(mtrx);
        }
        this->setStepsDirty();

        // If this was a settings mesh then all other parts are now also dirty
        if (m_root_mesh->type() == kSettings) {
            for (const auto& part : CSM->parts()) {
                part->setStepsDirty();
            }
        }
    }
}

void Part::adoptChild(QSharedPointer<Part> p) {
    m_children.append(p);
    p->m_parent = this->sharedFromThis();
}

void Part::orphanChild(QSharedPointer<Part> p) {
    m_children.removeOne(p);
    p->m_parent.reset();
}

QList<QSharedPointer<Part>> Part::children() { return m_children; }

QSharedPointer<Part> Part::parent() { return m_parent; }

void Part::prependStep(QSharedPointer<Step> step) {
    if (step == nullptr)
        return;

    StepPair new_group;
    switch (step->getType()) {
        case StepType::kRaft:
        case StepType::kLayer:
            new_group.printing_layer = qSharedPointerDynamicCast<Layer>(step);
            break;
        case StepType::kScan:
            new_group.scan_layer = qSharedPointerDynamicCast<ScanLayer>(step);
            break;
        case StepType::kAll:
            Q_ASSERT(false); // a step shouldn't have type of 'All'
            break;
    }
    m_step_pairs.push_front(new_group);
}

void Part::appendStep(QSharedPointer<Step> step) {
    if (step == nullptr)
        return;

    StepPair new_group;
    switch (step->getType()) {
        case StepType::kRaft:
        case StepType::kLayer:
            new_group.printing_layer = qSharedPointerDynamicCast<Layer>(step);
            break;
        case StepType::kScan:
            new_group.scan_layer = qSharedPointerDynamicCast<ScanLayer>(step);
            break;
        case StepType::kAll:
            Q_ASSERT(false); // a step shouldn't have type of 'All'
            break;
    }
    m_step_pairs.push_back(new_group);
}

void Part::clearSteps() { m_step_pairs.clear(); }

void Part::addScanLayerToStep(int step_index, QSharedPointer<ScanLayer> scan_layer) {
    Q_ASSERT(step_index < m_step_pairs.size());
    m_step_pairs[step_index].scan_layer = scan_layer;
}

int Part::countStepPairs() { return m_step_pairs.size(); }

QSharedPointer<Step> Part::step(int index, StepType type) {
    // this function could return a nullptr if the type isn't on this specified step
    Q_ASSERT(index < m_step_pairs.size());
    QSharedPointer<Step> result = nullptr;
    StepPair step_group = m_step_pairs[index];
    switch (type) {
        case StepType::kLayer:
            if (step_group.printing_layer != nullptr && step_group.printing_layer->getType() == StepType::kLayer)
                result = step_group.printing_layer;
            break;
        case StepType::kRaft:
            if (step_group.printing_layer != nullptr && step_group.printing_layer->getType() == StepType::kRaft)
                result = step_group.printing_layer;
            break;
        case StepType::kScan:
            result = step_group.scan_layer;
            break;
        case StepType::kAll: // this function should not be called with this step type
            Q_ASSERT(false);
            break;
    }
    return result;
}

bool Part::stepGroupContains(int index, StepType type) {
    Q_ASSERT(index < m_step_pairs.size());
    bool result = false;
    StepPair step_group = m_step_pairs[index];
    switch (type) {
        case StepType::kLayer:
            if (step_group.printing_layer != nullptr && step_group.printing_layer->getType() == StepType::kLayer)
                result = true;
            break;
        case StepType::kRaft:
            if (step_group.printing_layer != nullptr && step_group.printing_layer->getType() == StepType::kRaft)
                result = true;
            break;
        case StepType::kScan:
            if (step_group.scan_layer != nullptr)
                result = true;
            break;
        case StepType::kAll:
            Q_ASSERT(false); // function shouldn't be called with type 'All'
            break;
    }
    return result;
}

void Part::removeStepFromGroup(int index, StepType type) {
    Q_ASSERT(index < m_step_pairs.size());
    StepPair step_group = m_step_pairs[index];
    switch (type) {
        case StepType::kLayer:
            if (step_group.printing_layer != nullptr && step_group.printing_layer->getType() == StepType::kLayer)
                step_group.printing_layer = nullptr;
            break;
        case StepType::kRaft:
            if (step_group.printing_layer != nullptr && step_group.printing_layer->getType() == StepType::kRaft)
                step_group.printing_layer = nullptr;
            break;
        case StepType::kScan:
            step_group.scan_layer = nullptr;
            break;
        case StepType::kAll: // shouldn't call this function with type 'All'
            Q_ASSERT(false); // removeStep(index) should be called instead
            break;
    }

    if (step_group.printing_layer == nullptr && step_group.scan_layer == nullptr)
        m_step_pairs.removeAt(index);
}

void Part::replaceStep(int index, QSharedPointer<Step> step) {
    Q_ASSERT(index < m_step_pairs.size());

    if (step == nullptr)
        return;

    StepPair new_group;
    switch (step->getType()) {
        case StepType::kRaft:
        case StepType::kLayer:
            new_group.printing_layer = qSharedPointerDynamicCast<Layer>(step);
            break;
        case StepType::kScan:
            new_group.scan_layer = qSharedPointerDynamicCast<ScanLayer>(step);
            break;
        case StepType::kAll:
            Q_ASSERT(false); // a step shouldn't have type of 'All'
            break;
    }
    m_step_pairs[index] = new_group;
}

Part::StepPair& Part::getStepPair(int index) {
    Q_ASSERT(index < m_step_pairs.size());
    StepPair& ptr = m_step_pairs[index];
    return ptr;
}

Part::StepPair& Part::getLastStepPair() {
    StepPair& ptr = m_step_pairs[m_step_pairs.size() - 1];
    return ptr;
}

QList<Part::StepPair> Part::getDirtyStepPairs() {
    QList<StepPair> result;
    for (StepPair step_grp : m_step_pairs) {
        //  a step group is dirty if either its printing or scan layer is dirty
        if ((step_grp.printing_layer != nullptr && step_grp.printing_layer->isDirty()) ||
            (step_grp.scan_layer != nullptr && step_grp.scan_layer->isDirty())) {
            result.push_back(step_grp);
        }
    }
    return result;
}

QList<QSharedPointer<Step>> Part::steps(StepType type) {
    // step type is defaulted to all in function def
    QList<QSharedPointer<Step>> result;

    switch (type) {
        case StepType::kAll:
            for (StepPair step_grp : m_step_pairs) {
                if (step_grp.printing_layer != nullptr) {
                    result.push_back(step_grp.printing_layer);
                }

                if (step_grp.scan_layer != nullptr) {
                    result.push_back(step_grp.scan_layer);
                }
            }
            break;
        case StepType::kLayer:
            for (StepPair step_grp : m_step_pairs) {
                if (step_grp.printing_layer != nullptr && step_grp.printing_layer->getType() == StepType::kLayer) {
                    result.push_back(step_grp.printing_layer);
                }
            }
            break;
        case StepType::kRaft:
            for (StepPair step_grp : m_step_pairs) {
                if (step_grp.printing_layer != nullptr && step_grp.printing_layer->getType() == StepType::kRaft) {
                    result.push_back(step_grp.printing_layer);
                }
            }
            break;
        case StepType::kScan:
            for (StepPair step_grp : m_step_pairs) {
                if (step_grp.scan_layer != nullptr) {
                    result.push_back(step_grp.scan_layer);
                }
            }
            break;
    }

    return result;
}

void Part::clearStepsFromIndex(int index) {
    while (m_step_pairs.size() > index)
        m_step_pairs.removeAt(index);
}

void Part::removeStepAtIndex(int index) {
    Q_ASSERT(index < m_step_pairs.size());
    m_step_pairs.removeAt(index);
}

void Part::setStepsDirty() {
    for (StepPair step_grp : m_step_pairs) {
        if (step_grp.printing_layer != nullptr) {
            step_grp.printing_layer->setDirtyBit(true);
        }
        if (step_grp.scan_layer != nullptr) {
            step_grp.scan_layer->setDirtyBit(true);
        }
    }
}

bool Part::isPartDirty() {
    // a part is dirty if at least one layer is dirty
    for (StepPair step_grp : m_step_pairs) {
        if (step_grp.printing_layer->isDirty() || (step_grp.scan_layer != nullptr && step_grp.scan_layer->isDirty())) {
            return true;
        }
    }
    // a part is "clean" only if all of its steps are not dirty
    return false;
}

QSharedPointer<SyncManager> Part::getSync() const { return m_sync; }

void Part::setSync(const QSharedPointer<SyncManager>& sync) { m_sync = sync; }

QUuid Part::getId() { return m_uuid; }

} // Namespace ORNL
