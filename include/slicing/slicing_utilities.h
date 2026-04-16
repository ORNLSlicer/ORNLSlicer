#pragma once

#include "configs/settings_base.h"
#include "geometry/mesh/closed_mesh.h"
#include "part/part.h"
#include "step/global_layer.h"
#include "threading/abs_slicing_thread.h"

namespace ORNL {
/*!
 * \brief Provides access to methods used with polymer slicing
 */
class SlicingUtilities {
  public:
    /*!
     * \brief identifies meshes from a list of parts with a certain type
     * \param parts: a list of part who's root mesh might by a certain type
     * \param mt: the type to search for
     * \return a list of meshes that are the type
     */
    static QVector<QSharedPointer<MeshBase>> GetMeshesByType(QMap<QString, QSharedPointer<Part>> parts, MeshType mt);

    /*!
     * \brief identifies meshes from a list of parts with a certain type
     * \param parts: a list of part who's root mesh might by a certain type
     * \param mt: the type to search for
     * \return a list of parts that are have the same type as mt
     */
    static QVector<QSharedPointer<Part>> GetPartsByType(QMap<QString, QSharedPointer<Part>> parts, MeshType mt);

    /*!
     * \brief clips a mesh with a list of clippers
     * \param mesh: the subject mesh
     * \param clippers: a list of clippers
     */
    static void ClipMesh(QSharedPointer<MeshBase> mesh, QVector<QSharedPointer<MeshBase>> clippers);

    /*!
     * \brief Performs mesh-mesh intersection
     * \param mesh: subject mesh
     * \param intersect: mesh to intersect with
     */
    static void IntersectMesh(QSharedPointer<ClosedMesh> mesh, QSharedPointer<ClosedMesh> intersect);

    /*!
     * \brief Performs mesh-mesh union
     * \param mesh: subject mesh
     * \param to_union: mesh to union with
     */
    static void UnionMesh(QSharedPointer<ClosedMesh> mesh, QSharedPointer<ClosedMesh> to_union);

    /*!
     * \brief gets the part's start step. Useful if there is a raft enabled
     * \param part: the part to look at
     * \param current_steps: the number of steps currently
     * \return the index of the step to start at
     */
    static int GetPartStart(QSharedPointer<Part> part, int current_steps);

    /*!
     * \brief determines the default slicing axis given certain settings, as well as the mesh's min and max points
     * \param sb: the settings to use
     * \param mesh: the mesh to analyze
     * \return a tuple containing: the plane, mesh min, and mesh max
     */
    static std::tuple<Plane, Point, Point> GetDefaultSlicingAxis(QSharedPointer<SettingsBase> sb,
                                                                 QSharedPointer<MeshBase> mesh);

    /*!
     * \brief shift the slicing plane along a normal or skeleton
     * \param sb: the settings to use
     * \param slicing_plane: the slicing plane to shift
     * \param last_height: height of last layer
     */
    static void ShiftSlicingPlane(QSharedPointer<SettingsBase> sb, Plane& slicing_plane, Distance last_height);

    /*!
     * \brief determines if two parts overlap
     * \pre this assumes the parts exist for the entire build volume and therefore only checks at a single cross-section
     *      this saves same, but is only really useful for settings parts
     * \param parts: the parts to check
     * \param slicing_plane: the plane used to slice the parts
     * \return if the parts overlap
     */
    static bool doPartsOverlap(QVector<QSharedPointer<Part>> parts, Plane slicing_plane);
};
} // namespace ORNL
