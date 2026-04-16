#pragma once

#include <cstddef>
#include <utility>

#include <CGAL/Modifier_base.h>
#include <CGAL/Polyhedron_incremental_builder_3.h>
#include <QFileInfo>
#include <QThread>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <qcontainerfwd.h>
#include <qmatrix4x4.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/mesh_base.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
class Mesh;
class MeshVertex;
class MeshFace;

/*!
 * \class MeshLoader
 * \brief Loads a mesh from a file in a separate thread.
 */
class MeshLoader : public QThread {
    Q_OBJECT
  public:
    //! \struct MeshData
    //! \brief Holds a pointer to a mesh and the raw data/ size
    struct MeshData {
        QSharedPointer<MeshBase> mesh = nullptr;
        void* raw_data = nullptr;
        size_t size = 0;
    };

    //! \brief Constructor for thread
    MeshLoader(QString file_path, MeshType mt = MeshType::kBuild, QMatrix4x4 transform = QMatrix4x4(),
               Distance unit = Distance(mm));

    //! \brief Function that is run when start is called on this thread.
    void run() override;

    //! \brief loads meshes from a file off the thread
    //! \param file_path the path to the file
    //! \param mt the mesh type
    //! \param transform the transform to apply
    //! \param unit the unit to scale with
    //! \param raw_data optional raw data
    //! \param file_size optional file size
    //! \return a list of loaded meshes and their data
    //! \note if raw_data and file_size are not provided, the mesh will be loaded from the file directly
    static QVector<MeshData> LoadMeshes(QString file_path, MeshType mt = MeshType::kBuild,
                                        QMatrix4x4 transform = QMatrix4x4(), Distance unit = Distance(mm),
                                        void* raw_data = nullptr, size_t file_size = 0);

  signals:
    //! \brief Sends the model to the project manager
    void newMesh(MeshData data);

    //! \brief Emits error signal
    void error(QString msg);

  private:
    //! \brief loads raw data from a path
    //! \param file_path the path to load from
    //! \return a pair containing the raw data as a void ptr and the size in bytes
    static std::pair<void*, size_t> LoadRawData(QString file_path);

    //! \brief builds a surface mesh from an assimp mesh
    //! \param mesh the assimp mesh
    //! \return a surface mesh
    static MeshTypes::SurfaceMesh BuildSurfaceMesh(aiMesh* mesh);

    //! \class MeshBuilderAssimp
    //! \brief CGAL builder to convert an assimp mesh into a CGAL polyhedron
    template <class HDS> class MeshBuilderAssimp : public CGAL::Modifier_base<HDS> {
      public:
        //! \brief Builds a polyhedron incrementally using faces
        //! \param vertices: the mesh's vertices
        //! \param faces: the mesh's faces
        MeshBuilderAssimp(aiMesh* mesh) : m_mesh(mesh) {};

        void operator()(HDS& hds) {
            CGAL::Polyhedron_incremental_builder_3<HDS> builder(hds, false);

            builder.begin_surface(m_mesh->mNumVertices, m_mesh->mNumFaces);

            // Add all vertices
            for (unsigned int i = 0; i < m_mesh->mNumVertices; i++) {
                builder.add_vertex(MeshTypes::Point_3(m_mesh->mVertices[i].x * 1000, // Scale from mm to micron
                                                      m_mesh->mVertices[i].y * 1000, m_mesh->mVertices[i].z * 1000));
            }

            for (unsigned int i = 0; i < m_mesh->mNumFaces; i++) {
                builder.begin_facet();
                for (unsigned int j = 0; j < m_mesh->mFaces[i].mNumIndices; j++) {
                    int index = m_mesh->mFaces[i].mIndices[j];
                    builder.add_vertex_to_facet(index);
                }

                if (builder.error()) {
                    m_error = true;
                    builder.rollback();
                    return;
                }

                builder.end_facet();
            }

            if (builder.error()) {
                m_error = true;
                builder.rollback();
            }

            builder.end_surface();

            if (builder.check_unconnected_vertices())
                builder.remove_unconnected_vertices();
        }

        bool wasError() { return m_error; }

      private:
        aiMesh* m_mesh;
        bool m_error = false;
    };

    //! \brief path to file
    QString m_file_path;

    //! \brief Actual name.
    QString m_name;

    //! \brief The type of mesh this will be
    MeshType m_mesh_type;

    //! \brief the default transform to apply
    QMatrix4x4 m_transform;

    //! \brief the default unit scaling to apply
    Distance m_unit;

}; // class MeshLoader
} // namespace ORNL
