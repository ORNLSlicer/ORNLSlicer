#pragma once

#include <array>
#include <cstddef>
#include <utility>

#include <CGAL/boost/graph/graph_traits_Surface_mesh.h>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
//! \class OpenMesh
//! \brief A class that represents an open 3D volume
class OpenMesh : public MeshBase {
  public:
    //! \brief Default constructor
    OpenMesh();

    //! \brief Full constructor
    //! \param name the name
    //! \param path the file path
    //! \param vertices the vertices
    //! \param faces the faces
    //! \param type the type
    OpenMesh(const QString& name, const QString& path, const QVector<MeshVertex>& vertices,
             const QVector<MeshFace>& faces, MeshType type = MeshType::kBuild);

    //! \brief Contructor for vertices and faces
    //! \param vertices the vertices
    //! \param faces the faces
    OpenMesh(const QVector<MeshVertex>& vertices, const QVector<MeshFace>& faces);

    //! \brief Contructor to build a mesh from the CGAL type
    //! \param poly the CGAL polyhedron
    //! \param name the name
    //! \param file the file path
    OpenMesh(MeshTypes::SurfaceMesh poly, QString name = QString(), QString file = QString());

    //! \brief Copy contructor
    //! \param mesh
    OpenMesh(QSharedPointer<OpenMesh> mesh);

    //! \brief returns the internal cgal mesh for this object
    //! \return the cgal surface mesh
    MeshTypes::SurfaceMesh surface_mesh();

    //! \brief centers this mesh about its-self
    void center() override;

    //! \brief computes the optimal(tight) bouding box for this part and return the 8 points that form it
    //! \note this returns the tightest bounding box, \see boundingBox() for axis-aligned version
    //! \return eight points that form the bouding box
    QVector<Point> optimalBoundingBox() override;

    //! \brief computes the axis-aligned bouding box for this part and return the 8 points that form it
    //! \note this returns axis-aligned bounding box, \see optimalBoundingBox() for tight bounding box
    //! \return eight points that form the bouding box
    QVector<Point> boundingBox() override;

    //! \brief computes the surface area for the mesh
    //! \return the surface area
    Area area() override;

    //! \brief makes a surface mesh of just the upward normal faces
    //! \return a surface mesh
    MeshTypes::SurfaceMesh extractUpwardFaces() override;

    //! \brief intersects a plane with this mesh
    //! \note this can result in points, polylines, or polygons
    //! \param plane the plane to intersect with
    //! \return a list of polylines (includes single points) and polygons that result
    std::pair<QVector<Polyline>, QVector<Polygon>> intersect(Plane plane) override;

    //! \brief converts vertices and faces to a surface mesh
    //! \param vertices the vertices
    //! \param faces the faces
    //! \return a surface mesh
    static MeshTypes::SurfaceMesh SurfaceMeshFromVerticesAndFaces(const QVector<MeshVertex>& vertices,
                                                                  const QVector<MeshFace>& faces);

    //! \brief converts a surface mesh into vertices and faces
    //! \param sm the surface mesh
    //! \return a pair of vertices and faces
    static std::pair<QVector<MeshVertex>, QVector<MeshFace>>
    VerticesAndFacesFromSurfaceMesh(MeshTypes::SurfaceMesh& sm);

    //! \brief Builds and returns a surface mesh from a point cloud
    //! \param file_path the file to build the cloud from
    //! \return a pointer to the new mesh if it could be loaded
    static QSharedPointer<OpenMesh> BuildMeshFromPointCloud(const QString& file_path);

  private:
    //! \brief converts vertices and faces into polyhedron, used to keep the two in sync
    void convert() override;

    //! \brief loads points from a .matrix file
    //! \param file_path the file
    //! \return points in a point cloud
    static QVector<MeshTypes::Point_3> loadMatrixFile(const QString& file_path);

    //! \brief loads points from a .xyz file
    //! \param file_path the file
    //! \return points in a point cloud
    static QVector<MeshTypes::Point_3> loadXYZFile(const QString& file_path);

    //! \struct ConstructPointCloud
    //! \brief A trait used to build a point cloud mesh
    struct ConstructPointCloud {
        MeshTypes::SurfaceMesh& mesh;
        template <typename PointIterator>
        ConstructPointCloud(MeshTypes::SurfaceMesh& mesh, PointIterator b, PointIterator e) : mesh(mesh) {
            for (; b != e; ++b) {
                boost::graph_traits<MeshTypes::SurfaceMesh>::vertex_descriptor v;
                v = add_vertex(mesh);
                mesh.point(v) = *b;
            }
        }
        ConstructPointCloud& operator=(const std::array<std::size_t, 3> f) {
            typedef boost::graph_traits<MeshTypes::SurfaceMesh>::vertex_descriptor vertex_descriptor;
            typedef boost::graph_traits<MeshTypes::SurfaceMesh>::vertices_size_type size_type;
            mesh.add_face(vertex_descriptor(static_cast<size_type>(f[0])),
                          vertex_descriptor(static_cast<size_type>(f[1])),
                          vertex_descriptor(static_cast<size_type>(f[2])));
            return *this;
        }
        ConstructPointCloud& operator*() { return *this; }
        ConstructPointCloud& operator++() { return *this; }
        ConstructPointCloud operator++(int) { return *this; }
    };

    //! \brief the CGAL representation of this mesh
    MeshTypes::SurfaceMesh m_representation;

    //! \brief the original CGAL representation of this mesh
    MeshTypes::SurfaceMesh m_original_representation;
};
} // namespace ORNL
