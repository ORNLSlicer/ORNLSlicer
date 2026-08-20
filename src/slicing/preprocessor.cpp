#include "slicing/preprocessor.h"

#include <qcontainerfwd.h>
#include <qsharedpointer.h>

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/open_mesh.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "slicing/buffered_slicer.h"
#include "slicing/slicing_utilities.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
Preprocessor::Preprocessor(bool use_cgal_cross_section, bool include_build_plate_gap) {
    // Fetch and sort parts
    m_parts = {
        SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kBuild),
        SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kClipping),
        SlicingUtilities::GetPartsByType(CSM->parts(), MeshType::kSettings),
    };

    m_use_cgal_cross_section = use_cgal_cross_section;
    m_include_build_plate_gap = include_build_plate_gap;
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
                                  m_use_cgal_cross_section, m_include_build_plate_gap);
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
