#pragma once

#include <functional>

#include <qcontainerfwd.h>
#include <qhash.h>
#include <qobject.h>
#include <qsharedpointer.h>

#include "configs/settings_base.h"
#include "geometry/mesh/mesh_base.h"
#include "part/part.h"
#include "slicing/buffered_slicer.h"
#include "units/unit.h"

namespace ORNL {
//!
//! \class Preprocessor
//! \brief An iterative system to cross-section and prepare parts/ meshes to have tool-paths generated. At a high
//!        level, this class iterates through parts, then meshes and then cross-sections. You can supply custom
//!        processing in the form of: \code std::functions<bool(SomeKindOfProcessing)> \endcode to various stages of the
//!        pipeline. See individual modes for exactly what each one of these functions are called.
//!   This class is separated into two major modes:
//!         A. Whole-part approach:
//!             This method replaces the old system used by PolymerSlicer. It assumes that all parts can be sliced from
//!             start to end with all information. This mode can be triggered by using the processAll() call.
//!
//!             Preprocessing occurs in this order:
//!                 1. Initial Processing: provides access to all parts/ global settings before any steps are created
//!                 2. Part Processing: provides access to single part and its settings
//!                 3. Mesh Processing: provides access to single mesh and its parent part's settings
//!                 4. Step Builder: serves as instructions to build steps from a single cross-sectional slice
//!                 5. Cross Section Processing: allows for access to all cross-sections in a mesh
//!                 6. Status Update: provides access to a percentage of the total number of parts done
//!                 7. Final Processing: provides access to all parts/ global settings after all steps are created
//!
//!         B. Single-step approach:
//!             This method enables real time slicing. It differs from the whole-part approach in that it only computes
//!             a single layer/ step per call. This method utilizes two calls: processInitial() runs code once before
//!             any steps are generated, and processNext() which loads each part with up to one layer to process next.
//!
//!             Preprocessing occurs in this order:
//!                 Before any layers can be processed:
//!                     1. Initial Processing: provides access to all parts/ global settings before any steps are
//!                     created
//!                     2. Part Processing: provides access to single part and its settings
//!                     3. Mesh Processing: provides access to single mesh and its parent part's settings
//!                 On each call to processNext():
//!                     1. Step Builder: serves as instructions to build a single step from a single cross-sectional
//!                     slice for a part
//!                     2. Cross Section Processing: allows for access to newly generated step on part
//!                     3. Final Processing: provides access to all parts/ global settings after all steps are created
//!                     for this layer
//!                 \note processNext() will only slice parts who form the next global layer. It also returns a boolean
//!                 to signal if a layer was
//!                       created or when no more layers can be processed.
class Preprocessor {
  public:
    //! \struct Parts
    //! \brief provides access to all parts, sorted by type
    struct Parts {
        QVector<QSharedPointer<Part>> build_parts;
        QVector<QSharedPointer<Part>> clipping_parts;
        QVector<QSharedPointer<Part>> settings_parts;
    };

    //! \struct ActivePartMeta
    //! \brief stores information about where a part is in the slicing process
    struct ActivePartMeta {
        ActivePartMeta(QSharedPointer<Part> _part = nullptr, QSharedPointer<SettingsBase> _part_sb = nullptr,
                       int _steps_processed = 0, Distance _current_height = 0.0, int _part_start = 0,
                       bool _consuming = true) {
            part = _part;
            part_sb = _part_sb;
            steps_processed = _steps_processed;
            current_height = _current_height;
            part_start = _part_start;
            consuming = _consuming;
        }

        QSharedPointer<Part> part;
        QSharedPointer<SettingsBase> part_sb;
        int steps_processed = 0;
        Distance current_height = 0.0;
        int part_start = 0;
        int last_step_count = 0;
        bool consuming = true;
    };

    //! \typedef Processing
    //! \brief A function that can be used to process all parts and global settings
    //! \param const Parts& parts
    //! \param const QSharedPointer<SettingsBase>& global_settings
    //! \return a boolean flag to signal a halt in slicing
    typedef std::function<bool(const Parts& parts, const QSharedPointer<SettingsBase>& global_settings)> Processing;

    //! \typedef PartProcessing
    //! \brief A function that can be used to process a single part and its settings
    //! \param QSharedPointer<Part> part
    //! \param QSharedPointer<SettingsBase> part_sb
    //! \return a boolean flag to signal a halt in slicing
    typedef std::function<bool(QSharedPointer<Part> part, QSharedPointer<SettingsBase> part_sb)> PartProcessing;

    //! \typedef MeshProcessing
    //! \brief A function that can be used to process a single mesh and its parent part's settings
    //! \param QSharedPointer<Mesh> mesh
    //! \param QSharedPointer<SettingsBase> part_sb
    //! \return a boolean flag to signal a halt in slicing
    typedef std::function<bool(QSharedPointer<MeshBase> mesh, QSharedPointer<SettingsBase> part_sb)> MeshProcessing;

    //! \typedef StepBuilder
    //! \brief A function that can be used to build step(s) from cross-sections
    //! \param QSharedPointer<BufferedSlicer::SliceMeta>
    //! \param ActivePartMeta& meta
    //! \return a boolean flag to signal a halt in slicing
    typedef std::function<bool(QSharedPointer<BufferedSlicer::SliceMeta>, ActivePartMeta& meta)> StepBuilder;

    //! \typedef CrossSectionProcessing
    //! \brief A function that can be used to process cross-section(s)
    //! \param ActivePartMeta& meta
    //! \return a boolean flag to signal a halt in slicing
    typedef std::function<bool(ActivePartMeta& meta)> CrossSectionProcessing;

    //! \typedef StatusUpdate
    //! \brief A function that can makes use of the percentage of parts sliced
    //! \note only called with processAll()
    //! \param double percentage
    typedef std::function<void(double percentage)> StatusUpdate;

    //! \brief Constructor the fetches and sorts part from the session
    Preprocessor(bool use_cgal_cross_section = false);

    //! \brief Processes all parts, meshes and cross-sections at once
    void processAll();

    //! \brief Initial processing run on all parts and meshes, building slicer objects
    void processInital();

    //! \brief Processes the next printable global layer
    //! \return true if a new layer was created
    bool processNext();

    //! \brief Provides access to all parts/ global settings before any steps are created
    //! \param \see Processing
    void addInitialProcessing(Processing processing);

    //! \brief Provides access to all parts/ global settings after all steps are created
    //! \param \see Processing
    void addFinalProcessing(Processing processing);

    //! \brief Provides access to single part and its settings
    //! \param \see PartProcessing
    void addPartProcessing(PartProcessing processing);

    //! \brief Provides access to single mesh and its parent part's settings
    //! \param \see MeshProcessing
    void addMeshProcessing(MeshProcessing processing);

    //! \brief Instructions to build steps from a cross-sectional slice
    //! \param \see StepBuilder
    void addStepBuilder(StepBuilder builder);

    //! \brief Provides access to parts after steps have been built
    //! \param \see CrossSectionProcessing
    void addCrossSectionProcessing(CrossSectionProcessing processing);

    //! \brief Provides access to a percentage of the total number of parts done
    //! \note Only called when using processAll()
    //! \param \see StatusUpdate
    void addStatusUpdate(StatusUpdate update);

    //! \brief gets sorted parts
    //! \return Sorted Parts struct
    Parts getParts();

  private:
    //! \brief Callable functions used by the preprocessor
    StepBuilder m_step_builder;
    Processing m_initial_processing;
    Processing m_final_processing;
    PartProcessing m_part_processing;
    MeshProcessing m_mesh_processing;
    CrossSectionProcessing m_cross_section_processing;
    StatusUpdate m_status_update;

    bool m_use_cgal_cross_section = false;

    //! \brief Sorted parts
    Parts m_parts;

    //! \brief A list of Slicers matched with their part indices
    QHash<int, QSharedPointer<BufferedSlicer>> m_mesh_slicers;

    //! \brief A list of ActivePartMeta matched with their part name
    QHash<QString, ActivePartMeta> m_active_parts;
};
} // namespace ORNL
