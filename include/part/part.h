#pragma once

#include <QObject>
#include <QUuid>
#include <qcontainerfwd.h>
#include <qlist.h>
#include <qmap.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "configs/settings_base.h"
#include "configs/settings_range.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "managers/sync/sync_manager.h"
#include "step/layer/layer.h"
#include "step/layer/scan_layer.h"
#include "utilities/enums.h"

namespace ORNL {
class Step;
/*!
 * \class Part
 * \brief Class for parts. A part contains both meshes(used to generate pathing) and steps(used the store pathing).
 *        A part must contain a root-mesh that is used by the graphics to render it. It may also contain any number of
 *        sub-meshes. Additionally, this class also has a parenting system between parts.
 */
class Part : public QEnableSharedFromThis<Part> {

  public:
    //! \brief a step pair links a scan layer with its corresponding
    //!        printing layer
    struct StepPair {
        QSharedPointer<Layer> printing_layer; // can be either raft or layer type
        QSharedPointer<ScanLayer> scan_layer;
    };

    //! \brief Default Constructor.
    Part();

    //! \brief Copy constructor
    //! \param p: Part to copy
    Part(const QSharedPointer<Part>& p);

    //! \brief Constructor
    //! \param a pointer to a mesh to set at the root
    //! \param source file path for this part.
    //! \param Mesh type mode, defaults to build
    Part(QSharedPointer<MeshBase> root_mesh, QString file_name = "", MeshType mt = MeshType::kBuild);

    void setSourceFile(QString file_name) { m_file_name = file_name; }

    //! \brief Source file for part
    QString sourceFilePath() { return m_file_name; }

    //! \brief Mesh type mode, defaults to build
    MeshType getMeshType() { return m_mesh_type; }

    //! \brief Set Mesh type mode
    void setMeshType(MeshType mt) { m_mesh_type = mt; }

    //! \brief Get name of this part.
    inline QString name() { return m_name; }

    //! \brief Set name of this part.
    inline void setName(QString name) { m_name = name; }

    //! \brief Get the SettingsBase.
    //! \return pointer to SettingsBase
    QSharedPointer<SettingsBase> getSb() const;

    //! \brief Set the SettingsBase.
    //! \param sb: Pointer to SettingsBase to set
    void setSb(const QSharedPointer<SettingsBase>& sb);

    //! \brief gets the ranges on this part
    //! \return ranges in a map indexed with their cantor pair
    QMap<uint, QSharedPointer<SettingsRange>> getSettingsRanges();

    //! \brief creates a new range
    //! \param low the bottom range value (inclusive)
    //! \param high the upper range value (inclusive)
    //! \param group_name optional parameter for the group name
    void createSettingsRange(int low, int high, QString group_name = "");

    //! \brief creates a new range
    //! \param low the bottom range value (inclusive)
    //! \param high the upper range value (inclusive)
    //! \param sb the new settings base to update
    //! \param group_name optional parameter for the group name
    void createSettingsRange(int low, int high, QSharedPointer<SettingsBase> sb, QString group_name = "");

    //! \brief getRange gets the range based on low and high values
    //! \param low the bottom range value (inclusive)
    //! \param high the upper range value (inclusive)
    //! \return a pointer to the range
    QSharedPointer<SettingsRange> getSettingsRange(int low, int high);

    //! \brief Whether template has been applied to selected part.
    bool getTemplateApplied();

    //! \brief remove ranges based on layer values
    //! \param low the bottom range value (inclusive)
    //! \param high the upper range value (inclusive)
    void removeSettingsRange(int low, int high);

    //! \brief clear ranges from part if <no template selected>
    void clearSettingsRanges();

    //! \brief splits the range between two values
    //! \param low the bottom range value (inclusive)
    //! \param high the upper range value (inclusive)
    void splitSettingsRange(int low, int high);

    //! \brief updates a range's limits to new values
    //! \param old_low the old bottom range value (inclusive)
    //! \param old_high the new upper range value (inclusive)
    //! \param new_low the new bottom range value (inclusive)
    //! \param new_high the new upper range value (inclusive)
    void updateSettingsRangeLimits(int old_low, int old_high, int new_low, int new_high);

    //! \brief converts ranges and their settting into json
    //! \return a json with the ranges
    json SettingsRangesToJson();

    //! \brief loads ranges from json
    //! \param input the json to parse
    void loadRangesFromJson(json input);

    /*!
     * \title Mesh Methods
     * \brief A part may be composed of multiple meshes in addition to a root mesh.
     */

    //! \brief Sets the root mesh of the part.
    //! \param the root mesh to set
    void setRootMesh(QSharedPointer<MeshBase> mesh);

    //! \brief Set the root mesh from vetex and face information.
    //! \param a list of vertices
    //! \param a list of faces
    void setRootMesh(const QVector<MeshVertex>& vertices, const QVector<MeshFace>& faces);

    //! \brief Get the root mesh of the part.
    //! \return the root mesh
    QSharedPointer<MeshBase> rootMesh();

    //! \brief Gets the part's sub-meshes
    //! \return a list of meshes
    QVector<QSharedPointer<MeshBase>> subMeshes();

    //! \brief Adds a mesh to the part's sub-mesh list
    //! \param the mesh to add
    void appendSubMesh(QSharedPointer<MeshBase> mesh);

    //! \brief Removes all sub-meshes
    void clearSubMeshes();

    //! \brief Scales sub-meshes to the size of the root mesh
    void scaleSubMeshes();

    //! \brief Test if a mesh is contained in the part.
    //! \param the name of the sub-mesh
    bool containsSubMesh(QString name);

    //! \brief Get a vector of all meshes in this part, including the root mesh.
    QVector<QSharedPointer<MeshBase>> meshes();

    //! \brief Apply a transformation matrix.
    //! \param a transformation matrix
    void setTransformation(const QMatrix4x4& mtrx);

    /*!
     * \title Parenting Methods
     * \brief A part can contain a list of children. Note that in contrast to the view of the parts, transformations are
     *        always global in this class.
     */

    //! \brief Makes another part a child of this part.
    //! \param Part that is to become a child.
    void adoptChild(QSharedPointer<Part> p);

    //! \brief Removes another part from the list of children.
    //! \param Part to be removed.
    void orphanChild(QSharedPointer<Part> p);

    //! \brief Get the children of this part.
    QList<QSharedPointer<Part>> children();

    //! \brief Get the parent of this part.
    QSharedPointer<Part> parent();

    /*!
     * \title Step Methods
     * \brief A part contains a list of steps that are printed in order. If a part has sub-meshes the steps are combined
     *        into a single list.
     */

    //! \brief Prepend a step in the list of steps.
    //! \param step: step to prepend
    void prependStep(QSharedPointer<Step> step);

    //! \brief Append a step.
    //! \param step: step to append
    void appendStep(QSharedPointer<Step> step);

    //! \brief Clear all steps.
    void clearSteps();

    //! \brief Inserts a step at an index
    //! \param index: where to add the step
    //! \param step: the step to add
    void addScanLayerToStep(int step_index, QSharedPointer<ScanLayer> scan_layer);

    //! \brief Get the number of step groups
    //! \param type: Type of step to count
    //! \return Number of steps pairs.
    int countStepPairs();

    //! \brief Get the step at an index.
    //! \param index: index of the step group
    //! \param type: step type to get
    //! \return a pointer to the step
    QSharedPointer<Step> step(int index, StepType type);

    //! \brief Check if step group contains specific type of step
    //! \param index: index of the step group
    //! \param type: step type to get
    //! \return whether or not step is contained
    bool stepGroupContains(int index, StepType type);

    //! \brief Remove the step from its step group at an index
    //! \param index: index of the step group
    //! \param type: step type to remove
    void removeStepFromGroup(int index, StepType type);

    //! \brief Replaces a step at an index with a new one
    //! \param index: the step group to replace step in
    //! \param step: the new step
    void replaceStep(int index, QSharedPointer<Step> step);

    //! \brief Get a list of steps matching a type (or all).
    //! \param type Type of steps to get.
    //! \default StepType::kAll
    //! \return List of steps.
    QList<QSharedPointer<Step>> steps(StepType type = StepType::kAll);

    //! \brief Get a specific step group
    //! \param index: step group index
    //! \return map containing all steps in a group
    StepPair& getStepPair(int index);

    //! \brief acessor for the last step pair on this part
    StepPair& getLastStepPair();

    //! \brief Gets list of step pairs that are dirty
    QList<StepPair> getDirtyStepPairs();

    //! \brief Removes all steps starting at an index
    //! \param index: the step to start removing at
    void clearStepsFromIndex(int index);

    //! \brief Removes a step
    //! \param index: the step to remove
    void removeStepAtIndex(int index);

    //! \brief Sets all steps in this part to be dirty
    void setStepsDirty();

    //! \brief whether or not any step in the part is dirty
    //! \return if any step is marked dirty
    bool isPartDirty();

    //! \brief gets the step sync manager
    //! \return the manager for step synchronization
    QSharedPointer<SyncManager> getSync() const;

    //! \brief sets the step sync manager
    //! \param the new step synchronizer
    void setSync(const QSharedPointer<SyncManager>& sync);

    //! \brief Sets template being applied to currently selected part.
    void setCurrentPartTemplate(QString current_template);

    //! \brief Get template being applied to currently selected part.
    QString getCurrentPartTemplate();

    //! \brief Whether template applied to selected part is same as template selected by user.
    bool currentPartTemplateEqualToSetTemplate(QString set_template);

    //! \brief Get map of whether each range is from a template or entered by user.
    QMap<uint, bool> getRangesFromTemplate();

    //! \brief accessor for part id
    //! \returns universally unique identifier for this part
    QUuid getId();

  private:
    //! \brief a unique identifier for the part
    QUuid m_uuid;

    //! \brief Name for the overall part.
    QString m_name;

    //! \brief Whether tmeplate has already been applied to selected part.
    bool m_template_applied;

    //! \brief Template applied to currently selected part.
    QString m_current_part_template;

    //! \brief Children of this part.
    QList<QSharedPointer<Part>> m_children;

    //! \brief Parent of this part. Internally a raw pointer to prevent double free.
    QSharedPointer<Part> m_parent = nullptr;

    //! \brief m_root_mesh: the mesh used for graphic representation of the part
    QSharedPointer<MeshBase> m_root_mesh;

    //! \brief m_sub_meshes: a list of sub-meshes owned by this part
    QVector<QSharedPointer<MeshBase>> m_sub_meshes;

    //! \brief Step pairs that constitute the part
    QList<StepPair> m_step_pairs;

    //! \brief Manages syncing of mutiple step threads
    QSharedPointer<SyncManager> m_sync;

    //! \brief Settings for the part.
    QSharedPointer<SettingsBase> m_sb;

    //! \brief Cantor pairs and their respective ranges
    QMap<uint, QSharedPointer<SettingsRange>> m_ranges;

    //! \brief Whether each range is from a template or entered by user. Used to set colors in layer bar.
    QMap<uint, bool> m_range_from_template;

    //! \brief Source file path for the part.
    QString m_file_name;

    //! \brief Mesh type mode, defaults to build
    MeshType m_mesh_type = MeshType::kBuild;
};
} // namespace ORNL
