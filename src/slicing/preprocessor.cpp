#include "slicing/preprocessor.h"

#include <limits>

#include <qcontainerfwd.h>
#include <qnumeric.h>
#include <qpair.h>
#include <qsharedpointer.h>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/open_mesh.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "slicing/buffered_slicer.h"
#include "slicing/slicing_utilities.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
Preprocessor::Preprocessor(bool use_cgal_cross_section) {
    // Fetch and sort parts
    m_parts = {
        SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kBuild),
        SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kClipping),
        SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kSettings),
    };

    m_use_cgal_cross_section = use_cgal_cross_section;
}

void Preprocessor::processAll() {
    QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());

    if (m_initial_processing != nullptr)
        if (m_initial_processing(m_parts, global_sb))
            return; // halt slicing

    int total_num_parts = m_parts.build_parts.size();
    int parts_done = 0;
    for (const QSharedPointer<Part>& part : m_parts.build_parts) {
        // Setup settings
        auto part_sb = QSharedPointer<SettingsBase>::create(*global_sb); // Copy global
        part_sb->populate(part->getSb());                                // Fill with part overrides

        ActivePartMeta part_meta(part, part_sb);

        if (m_part_processing != nullptr)
            if (m_part_processing(part, part_sb))
                return; // halt slicing

        part_meta.steps_processed = part->countStepPairs();
        part_meta.part_start = SlicingUtilities::GetPartStart(part, part_meta.steps_processed);

        for (const QSharedPointer<MeshBase>& original_mesh : part->meshes()) {
            QSharedPointer<MeshBase> mesh;
            // Make a new copy of the mesh to prevent the original one from being contaminated
            auto closed_mesh = dynamic_cast<ClosedMesh*>(original_mesh.get());
            if (closed_mesh != nullptr)
                mesh = QSharedPointer<ClosedMesh>::create(ClosedMesh(*closed_mesh));
            else
                mesh = QSharedPointer<OpenMesh>::create(OpenMesh(*dynamic_cast<OpenMesh*>(original_mesh.get())));

            if (m_mesh_processing != nullptr)
                if (m_mesh_processing(mesh, part_sb))
                    return; // halt slicing

            part_meta.steps_processed = part->countStepPairs();
            part_meta.part_start = SlicingUtilities::GetPartStart(part, part_meta.steps_processed);

            BufferedSlicer slicer(mesh, part_sb, m_parts.settings_parts, part->getSettingsRanges(), 0, 0,
                                  m_use_cgal_cross_section);
            QSharedPointer<BufferedSlicer::SliceMeta> next_layer_meta = nullptr;
            int last_step_count = 0;
            do {
                next_layer_meta = slicer.processNextSlice();

                if (next_layer_meta == nullptr)
                    break;

                // Build steps using slicing info
                if (m_step_builder != nullptr)
                    if (m_step_builder(next_layer_meta, part_meta))
                        return; // halt slicing

                last_step_count = next_layer_meta->number;
            } while (next_layer_meta != nullptr);

            part_meta.last_step_count = last_step_count;

            if (m_cross_section_processing != nullptr)
                if (m_cross_section_processing(part_meta))
                    return; // halt slicing
        }

        ++parts_done;
        if (m_status_update != nullptr)
            m_status_update((double)parts_done / (double)total_num_parts * 100);
    }

    if (m_final_processing != nullptr)
        if (m_final_processing(m_parts, global_sb))
            return; // halt slicing
}

void Preprocessor::processInital() {
    m_mesh_slicers.clear();

    QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());

    int previous_buffer_size = 0;
    int future_buffer_size = 0;

    if (GSM->getGlobal()->setting<bool>(PS::Skin::kEnable)) {
        future_buffer_size = GSM->getGlobal()->setting<int>(PS::Skin::kTopCount);
        previous_buffer_size = GSM->getGlobal()->setting<int>(PS::Skin::kBottomCount);
    }

    // Always buffer at least one layer
    if (future_buffer_size == 0)
        future_buffer_size = 1;

    if (m_initial_processing != nullptr)
        if (m_initial_processing(m_parts, global_sb))
            return; // halt slicing

    int slicer_index = 0;
    for (const QSharedPointer<Part>& part : m_parts.build_parts) {
        // Setup settings
        auto part_sb = QSharedPointer<SettingsBase>::create(*global_sb); // Copy global
        part_sb->populate(part->getSb());                                // Fill with part overrides

        if (m_part_processing != nullptr)
            if (m_part_processing(part, part_sb))
                return; // halt slicing

        for (const QSharedPointer<MeshBase>& original_mesh : part->meshes()) {
            QSharedPointer<MeshBase> mesh;
            // Make a new copy of the mesh to prevent the original one from being contaminated
            auto closed_mesh = dynamic_cast<ClosedMesh*>(original_mesh.get());
            if (closed_mesh != nullptr)
                mesh = QSharedPointer<ClosedMesh>::create(ClosedMesh(*closed_mesh));
            else
                mesh = QSharedPointer<OpenMesh>::create(OpenMesh(*dynamic_cast<OpenMesh*>(original_mesh.get())));

            if (m_mesh_processing != nullptr)
                if (m_mesh_processing(mesh, part_sb))
                    return; // halt slicing

            QSharedPointer<BufferedSlicer> slicer =
                QSharedPointer<BufferedSlicer>::create(mesh, part_sb, m_parts.settings_parts, part->getSettingsRanges(),
                                                       previous_buffer_size, future_buffer_size);
            m_mesh_slicers.insert(slicer_index, slicer);
            ++slicer_index;
        }

        ActivePartMeta new_part_meta(part, part_sb);
        new_part_meta.consuming = !part_sb->setting<bool>(MS::PlatformAdhesion::kRaftEnable);
        m_active_parts.insert(part->name(), new_part_meta);
    }
}

bool Preprocessor::processNext() {
    QSharedPointer<SettingsBase> global_sb = QSharedPointer<SettingsBase>::create(*GSM->getGlobal());
    bool steps_created = false;
    QVector<QPair<QSharedPointer<Part>, QSharedPointer<BufferedSlicer>>> min_slicers;

    LayerOrdering order_method = global_sb->setting<LayerOrdering>(ES::PrinterConfig::kLayerOrdering);
    if (order_method == LayerOrdering::kByHeight) {
        Plane min_plane;
        Distance min_distance = std::numeric_limits<double>::max();

        // Peek next layers on all parts/ meshes to determine what comes next
        int slicer_index = 0;
        for (auto& part : m_parts.build_parts) {
            for (auto& mesh : part->meshes()) {
                auto slicer = m_mesh_slicers[slicer_index];
                auto next_slice = slicer->peekNextSlice();
                if (next_slice != nullptr) {
                    Distance layer_dist =
                        MathUtils::linePlaneIntersection(Point(0, 0, 0), next_slice->plane.normal(), next_slice->plane)
                            .distance();

                    if (layer_dist < min_distance) {
                        min_distance = layer_dist;
                        min_plane = next_slice->plane;
                        min_slicers.clear();
                        min_slicers.push_back(qMakePair(part, slicer));
                    }
                    else if (qFuzzyCompare(layer_dist(), min_distance()) &&
                             next_slice->plane.normal() == min_plane.normal()) {
                        min_slicers.push_back(qMakePair(part, slicer));
                    }
                }

                ++slicer_index;
            }
        }
    }
    else if (order_method == LayerOrdering::kByLayerNumber) {
        int min_index = std::numeric_limits<int>::max();

        // Peek next layers on all parts/ meshes to determine what comes next
        int slicer_index = 0;
        for (auto& part : m_parts.build_parts) {
            for (auto& mesh : part->meshes()) {
                auto slicer = m_mesh_slicers[slicer_index];
                auto next_slice = slicer->peekNextSlice();
                if (next_slice != nullptr) {
                    if (slicer->getSliceCount() < min_index) {
                        min_index = slicer->getSliceCount();
                        min_slicers.clear();
                        min_slicers.push_back(qMakePair(part, slicer));
                    }
                    else if (slicer->getSliceCount() == min_index) {
                        min_slicers.push_back(qMakePair(part, slicer));
                    }
                }

                ++slicer_index;
            }
        }
    }
    else if (order_method == LayerOrdering::kByPart) {
        int slicer_index = 0;
        for (auto& part : m_parts.build_parts) {
            for (auto& mesh : part->meshes()) {
                auto slicer = m_mesh_slicers[slicer_index];
                auto next_slice = slicer->peekNextSlice();
                if (next_slice != nullptr) {
                    min_slicers.push_back(qMakePair(part, slicer));
                    break;
                }

                ++slicer_index;
            }

            if (!min_slicers.isEmpty())
                break;
        }
    }

    for (auto& parts_with_slicers : min_slicers) {
        auto& part = parts_with_slicers.first;
        int current_steps = part->countStepPairs();
        int part_start = SlicingUtilities::GetPartStart(part, current_steps);

        auto& slicer = parts_with_slicers.second;

        // Setup settings
        auto part_sb = QSharedPointer<SettingsBase>::create(*global_sb); // Copy global
        part_sb->populate(part->getSb());                                // Fill with part overrides

        QSharedPointer<BufferedSlicer::SliceMeta> next_step_meta = nullptr;
        if (m_active_parts[part->name()].consuming)
            next_step_meta = slicer->processNextSlice();
        else
            next_step_meta = slicer->peekNextSlice();

        if (next_step_meta == nullptr)
            break;
        else
            steps_created = true;

        // Build step using slicing info
        if (m_step_builder != nullptr)
            if (m_step_builder(next_step_meta, m_active_parts[part->name()]))
                return false; // halt slicing

        if (m_cross_section_processing != nullptr)
            if (m_cross_section_processing(m_active_parts[part->name()]))
                return false; // halt slicing

        ++m_active_parts[part->name()].steps_processed;
    }

    if (m_final_processing != nullptr)
        if (m_final_processing(m_parts, global_sb))
            return false; // halt slicing

    return steps_created;
}

void Preprocessor::addInitialProcessing(Processing processing) { m_initial_processing = processing; }

void Preprocessor::addFinalProcessing(Processing processing) { m_final_processing = processing; }

void Preprocessor::addPartProcessing(PartProcessing processing) { m_part_processing = processing; }

void Preprocessor::addMeshProcessing(MeshProcessing processing) { m_mesh_processing = processing; }

void Preprocessor::addCrossSectionProcessing(CrossSectionProcessing processing) {
    m_cross_section_processing = processing;
}

void Preprocessor::addStepBuilder(StepBuilder builder) { m_step_builder = builder; }

void Preprocessor::addStatusUpdate(StatusUpdate update) { m_status_update = update; }

Preprocessor::Parts Preprocessor::getParts() { return m_parts; }
} // namespace ORNL
