#pragma once

#include <utility>
#include <vector>

#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/property_map.h>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qmatrix4x4.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/plane.h"
#include "geometry/point.h"
#include "geometry/polygon.h"
#include "geometry/polyline.h"
#include "units/derivative_units.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
//! \class MeshBase
//! \brief A base type for 3D objects
class MeshBase {
  public:
    //! \brief Default constructor
    MeshBase();

    //! \brief Full constructor
    //! \param vertices: the mesh vertices of the mesh
    //! \param faces: the mesh faces of the mesh
    //! \param name: the name of this mesh
    //! \param path: the file path
    //! \param type: the type of this mesh
    MeshBase(const QVector<MeshVertex>& vertices, const QVector<MeshFace>& faces, const QString& name = QString(),
             const QString& path = QString(), MeshType type = MeshType::kBuild);

    //! \brief Copy Constructor
    //! \param mesh: a mesh
    MeshBase(const QSharedPointer<MeshBase> mesh);

    //! \brief Get the vertices.
    //! \return a vector of the mesh vertices
    const QVector<MeshVertex> vertices();

    //! \brief Get the faces.
    //! \return a vector of the mesh faces
    const QVector<MeshFace> faces();

    //! \brief Get the untransformed verticies.
    //! \return a vector of the original verticies.
    const QVector<MeshVertex> originalVertices();

    //! \brief Get the untransformed faces.
    //! \return a vector of the original faces.
    const QVector<MeshFace> originalFaces();

    //! \brief centers this mesh about its-self
    virtual void center() = 0;

    //! \brief Change the mesh's transformation matrix
    //! \param matrix: a translation matrix
    void setTransformation(const QMatrix4x4& matrix);

    //! \brief Change the mesh's transformations matrixes
    //! \param matrixes: a list of tramsformation matrix
    void setTransformations(const QVector<QMatrix4x4> matrixes);

    //! \brief align axis
    //! \param matrix: a translation matrix
    void alignAxis(const QMatrix4x4& matrix);

    //! \brief align axis
    //! \param matrix: a translation matrix
    void resetAlignedAxis(const QMatrix4x4& matrix);

    //! \brief Get the transformation matrix.
    //! \return the transformation of this mesh
    const QMatrix4x4& transformation() const;

    //! \brief Get the transformations matrixes.
    //! \return the transformations of this mesh
    QVector<QMatrix4x4> transformations();

    //! \brief Scale mesh to fit within a given volume.
    //! \param bounds: the dimensions to scale to
    void scale(Distance3D bounds);

    //! \brief Scale mesh to fit uniformly within a volume.
    //! \param bounds: the dimensions to scale to
    void scaleUniform(Distance3D bounds);

    //! \brief computes the centroid of the mesh
    //! \return the centroid
    virtual Point centroid();

    //! \brief computes the original centroid of the mesh
    //! \return the original centroid
    virtual Point originalCentroid();

    //! \brief computes the optimal(tight) bouding box for this part and return the 8 points that form it
    //! \note this returns the tightest bounding box, \see boundingBox() for axis-aligned version
    //! \return eight points that form the bouding box
    virtual QVector<Point> optimalBoundingBox() = 0;

    //! \brief computes the axis-aligned bouding box for this part and return the 8 points that form it
    //! \note this returns axis-aligned bounding box, \see optimalBoundingBox() for tight bounding box
    //! \return eight points that form the bouding box
    virtual QVector<Point> boundingBox() = 0;

    //! \brief computes the surface area for the mesh
    //! \return the surface area
    virtual Area area() = 0;

    //! \brief intersects a plane with this mesh
    //! \note this can result in points, polylines, or polygons
    //! \param plane the plane to intersect with
    //! \return a list of polylines (includes single points) and polygons that result
    virtual std::pair<QVector<Polyline>, QVector<Polygon>> intersect(Plane plane) = 0;

    //! \brief Get the dimensions of mesh.
    //! \return the dimensions of the mesh as 3D distance
    Distance3D dimensions();

    //! \brief Get the original dimensions of mesh.
    //! \return the dimensions of the mesh as 3D distance
    Distance3D originalDimensions();

    //! \brief Get the max point on the mesh.
    //! \return the min point on the bounding box
    Point max();

    //! \brief Get the min point on the mesh.
    //! \return the min point on the bounding box
    Point min();

    //! \brief returns the minimum and maximum point on the mesh
    //! \param vector - axis to consider min/max on
    //! \note generally, the vector should be the normal of the slicing plane
    std::pair<Point, Point> getAxisExtrema(QVector3D vector);

    //! \brief the name of the mesh
    //! \return the name of the mesh
    //! \note not all meshes will have a name
    const QString& name();

    //! \brief sets the name of the mesh
    //! \param name: the new name
    void setName(const QString& name);

    //! \brief the file path of this mesh
    //! \return the file path
    //! \note not all meshes will have a file path
    const QString& path();

    //! \brief sets the file path of the mesh
    //! \param path: the new file path
    void setPath(const QString& path);

    //! \brief gets the unit used to import this mesh
    //! \return the unit used to import this file
    Distance unit();

    //! \brief sets the unit used to import this mesh
    //! \param unit the unit
    void setUnit(const Distance unit);

    //! \brief gets the type of mesh
    //! \return a mesh type
    const MeshType type();

    //! \brief Sets the type of mesh this is
    //! \param type: the new mesh type
    void setType(const MeshType type);

    //! \brief is this mesh closed
    //! \return if this mesh is closed
    bool isClosed();

    //! \brief gets the type of generator used for this mesh
    //! \return the generator type
    const MeshGeneratorType genType();

    //! \brief sets the type of generator used to create this mesh
    //! \param type: the type
    void setGenType(MeshGeneratorType type);

    //! \brief Converter for the information needed to populate OpenGL buffers
    //! \return a pointer to the original vertex data + size
    std::pair<float*, uint> glVertexArray();

    //! \brief Converter for the information needed to populate OpenGL buffers
    //! \return a pointer to the original normal data + size
    std::pair<float*, uint> glNormalArray();

    virtual MeshTypes::SurfaceMesh extractUpwardFaces() = 0;

    template <class ValueType>
    //! \struct FacetPropMap
    //! \brief Property map used to for associating a facet with an integer as id to an element in a vector stored
    //! internally
    struct FacetPropMap : public boost::put_get_helper<ValueType&, FacetPropMap<ValueType>> {
        typedef boost::graph_traits<MeshTypes::Polyhedron>::face_descriptor key_type;
        typedef ValueType value_type;
        typedef value_type& reference;
        typedef boost::lvalue_property_map_tag category;
        FacetPropMap(std::vector<ValueType>& internal_vector) : m_internal_vector(internal_vector) {}
        reference operator[](key_type key) const { return m_internal_vector[key->id()]; }

      private:
        std::vector<ValueType>& m_internal_vector;
    };

  protected:
    //! \brief instructs child class to update it's underlying CGAL representation from the faces and vertices
    //! \note this is called by setTransformation()
    virtual void convert() = 0;

    //! \brief computes dimensions
    void updateDims();

    //! \brief fetches the id of a face with indices idx0 and idx1
    //! \param idx0: vertex 1
    //! \param idx1: vertex 2
    //! \param notFaceIdx: the id of the face to ignore
    //! \param vertices: the list of vertice
    //! \return the index of a face
    static int GetFaceIdxWithPoints(int idx0, int idx1, int notFaceIdx, QVector<MeshVertex>& vertices);

    //! \brief the name of the mesh
    QString m_name;

    //! \brief the path to where the file is located
    QString m_file;

    //! \brief the unit used to import this mesh (defaults to mm)
    Distance m_imported_unit = Distance(mm);

    //! \brief the type of mesh this is
    MeshType m_type = MeshType::kBuild;

    //! \brief If and what generator that mash was created with
    MeshGeneratorType m_gen_type = MeshGeneratorType::kNone;

    //! \brief Current vertex information.
    QVector<MeshVertex> m_vertices;
    //! \brief Current face information.
    QVector<MeshFace> m_faces;

    //! \brief Original vertex information.
    QVector<MeshVertex> m_vertices_original;

    //! \brief Original face information.
    QVector<MeshFace> m_faces_original;

    //! \brief Aligned vertex information.
    QVector<MeshVertex> m_vertices_aligned;

    //! \brief Aligned face information.
    QVector<MeshFace> m_faces_aligned;

    //! \brief Transformation matrix which defines Translation, Rotation, and Scale.
    QMatrix4x4 m_transformation;

    //! \brief List of Transformation matrix which defines Translation, Rotation, and Scale.
    QVector<QMatrix4x4> m_all_transformations;

    //! \brief Dimensions: the size of the mesh
    Distance3D m_dimensions;

    //! \brief Original Dimensions: the size of the mesh
    Distance3D m_original_dimensions;

    //! \brief The min point on the mesh
    Point m_min;

    //! \brief The max point on the mesh
    Point m_max;

    //! \brief if this mesh is closed or not
    bool m_is_closed = false;

  private:
    //! \brief check if transformation is rotation
    //! \param matrix: a translation matrix
    bool isRotationalTransform(const QMatrix4x4& matrix, QQuaternion& rotation);
};
} // namespace ORNL
