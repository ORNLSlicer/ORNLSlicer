#include "geometry/mesh/open_mesh.h"

#include <array>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include <CGAL/Advancing_front_surface_reconstruction.h>
#include <CGAL/Aff_transformation_3.h>
#include <CGAL/IO/read_points.h>
#include <CGAL/Named_function_parameters.h>
#include <CGAL/Optimal_bounding_box/oriented_bounding_box.h>
#include <CGAL/Origin.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/transform.h>
#include <CGAL/Polygon_mesh_slicer.h>
#include <CGAL/Polyhedron_items_with_id_3.h>
#include <CGAL/aff_transformation_tags.h>
#include <CGAL/boost/graph/Face_filtered_graph.h>
#include <CGAL/boost/graph/copy_face_graph.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/boost/graph/graph_traits_Surface_mesh.h>
#include <CGAL/boost/graph/iterator.h>
#include <CGAL/disable_warnings.h>
#include <QFileInfo>
#include <qcontainerfwd.h>
#include <qfiledevice.h>
#include <qhashfunctions.h>
#include <qlogging.h>
#include <qmap.h>
#include <qsharedpointer.h>
#include <qstringview.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/plane.h"
#include "geometry/polygon.h"
#include "geometry/polyline.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
OpenMesh::OpenMesh() : MeshBase() {}

OpenMesh::OpenMesh(const QVector<MeshVertex>& vertices, const QVector<MeshFace>& faces) : MeshBase(vertices, faces) {
    m_representation = SurfaceMeshFromVerticesAndFaces(m_vertices, m_faces);
    m_original_representation = m_representation; // Create a copy
    updateDims();
}

OpenMesh::OpenMesh(const QString& name, const QString& path, const QVector<MeshVertex>& vertices,
                   const QVector<MeshFace>& faces, MeshType type)
    : MeshBase(vertices, faces, name, path, type) {
    m_representation = SurfaceMeshFromVerticesAndFaces(m_vertices, m_faces);
    m_original_representation = m_representation; // Create a copy
    updateDims();
}

OpenMesh::OpenMesh(MeshTypes::SurfaceMesh poly, QString name, QString file) : MeshBase() {
    m_name = name;
    m_file = file;

    m_representation = MeshTypes::SurfaceMesh(poly);

    auto vertices_and_faces = VerticesAndFacesFromSurfaceMesh(m_representation);
    m_vertices_original = m_vertices = m_vertices_aligned = vertices_and_faces.first;
    m_faces_original = m_faces = m_faces_aligned = vertices_and_faces.second;
    m_original_representation = m_representation;
    updateDims();
}

OpenMesh::OpenMesh(QSharedPointer<OpenMesh> mesh) : MeshBase(mesh) {
    m_representation = mesh->m_representation;
    m_original_representation = mesh->m_original_representation;
}

MeshTypes::SurfaceMesh OpenMesh::surface_mesh() { return m_representation; }

void OpenMesh::center() {
    //! Compose translation
    CGAL::Aff_transformation_3<MeshTypes::Kernel> translation(CGAL::Translation(), -centroid().toVector_3());

    //! Apply translation to CGAL mesh
    CGAL::Polygon_mesh_processing::transform(translation, m_original_representation);
    CGAL::Polygon_mesh_processing::transform(translation, m_representation);

    //! Update mesh vertices
    size_t i = 0;
    for (const auto& v : m_original_representation.points()) {
        m_vertices[i].location = m_vertices_aligned[i].location = m_vertices_original[i].location =
            QVector3D(v.x(), v.y(), v.z());
        i++;
    }
}

QVector<Point> OpenMesh::optimalBoundingBox() {
    std::array<MeshTypes::Point_3, 8> obb_points;
    CGAL::oriented_bounding_box(m_representation, obb_points, CGAL::parameters::use_convex_hull(true));
    QVector<Point> points;
    return points;
}

QVector<Point> OpenMesh::boundingBox() {
    MeshTypes::SurfaceMesh_AABB_Tree tree(CGAL::faces(m_representation).first, CGAL::faces(m_representation).second,
                                          m_representation);
    auto box = tree.bbox();

    QVector<Point> points;

    points.push_back(Point(box.xmin(), box.ymin(), box.zmin()));
    points.push_back(Point(box.xmax(), box.ymin(), box.zmin()));
    points.push_back(Point(box.xmax(), box.ymax(), box.zmin()));
    points.push_back(Point(box.xmin(), box.ymax(), box.zmin()));
    points.push_back(Point(box.xmin(), box.ymin(), box.zmax()));
    points.push_back(Point(box.xmax(), box.ymin(), box.zmax()));
    points.push_back(Point(box.xmax(), box.ymax(), box.zmax()));
    points.push_back(Point(box.xmin(), box.ymax(), box.zmax()));

    return points;
}

Area OpenMesh::area() {
    double area = CGAL::Polygon_mesh_processing::area(m_representation);
    Area a;
    return a.from(area, (micron * micron));
}

MeshTypes::SurfaceMesh OpenMesh::extractUpwardFaces() {
    MeshTypes::Polyhedron poly;
    CGAL::copy_face_graph(m_representation, poly);

    CGAL::set_halfedgeds_items_id(poly);
    std::vector<std::size_t> segment_ids(CGAL::num_faces(poly));
    FacetPropMap<std::size_t> segment_property_map(segment_ids);

    for (auto face : CGAL::faces(poly)) {
        QVector<QVector3D> points;

        // Compute normal
        auto halfedge_index = face->facet_begin();
        do {
            auto point = halfedge_index->vertex()->point();
            points.push_back(QVector3D(point.x(), point.y(), point.z()));
        } while (++halfedge_index != face->facet_begin());

        QVector3D normal = QVector3D::crossProduct(points[1] - points[0], points[2] - points[0]).normalized();
        // This face is upward facing
        if (normal.z() > 0.0)
            segment_property_map[face] = 1; // Set a flag
        else
            segment_property_map[face] = 0;
    }

    typedef CGAL::Face_filtered_graph<MeshTypes::Polyhedron> FilteredMesh;
    FilteredMesh segment_mesh(poly, 0, segment_property_map);
    segment_mesh.set_selected_faces(1, segment_property_map);

    MeshTypes::SurfaceMesh output;
    CGAL::copy_face_graph(segment_mesh, output);
    return output;
}

std::pair<QVector<Polyline>, QVector<Polygon>> OpenMesh::intersect(Plane plane) {
    // Slicer constructor from the mesh
    CGAL::Polygon_mesh_slicer<MeshTypes::SurfaceMesh, MeshTypes::Kernel> slicer(m_representation);

    QVector<std::vector<MeshTypes::Kernel::Point_3>> cgal_polylines;
    slicer(plane.toCGALPlane(), std::back_inserter(cgal_polylines));

    QVector<Polyline> result_polylines;
    QVector<Polygon> result_polygons;

    for (auto& cgal_polyline : cgal_polylines) {
        if (cgal_polyline.front() == cgal_polyline.back()) // Can this be closed into a polygon?
        {
            Polygon new_polygon;
            for (auto& point : cgal_polyline) {
                new_polygon.append(Point::FromCGALPoint(point));
            }
            result_polylines.push_back(new_polygon);
        }
        else {
            Polyline new_line;
            for (auto& point : cgal_polyline) {
                new_line.append(Point::FromCGALPoint(point));
            }
            result_polylines.push_back(new_line);
        }
    }
    return std::make_pair(result_polylines, result_polygons);
}

MeshTypes::SurfaceMesh OpenMesh::SurfaceMeshFromVerticesAndFaces(const QVector<MeshVertex>& vertices,
                                                                 const QVector<MeshFace>& faces) {
    MeshTypes::SurfaceMesh sm;
    typedef MeshTypes::SurfaceMesh::Vertex_index VertexIndex;
    QMap<uint, VertexIndex> points;

    for (uint i = 0, end = vertices.size(); i < end; ++i)
        points[i] = sm.add_vertex(vertices[i].toPoint3());

    for (auto& face : faces) {
        sm.add_face(points[face.vertex_index[0]], points[face.vertex_index[1]], points[face.vertex_index[2]]);
    }
    return sm;
}

std::pair<QVector<MeshVertex>, QVector<MeshFace>>
OpenMesh::VerticesAndFacesFromSurfaceMesh(MeshTypes::SurfaceMesh& sm) {
    QVector<MeshFace> mesh_faces;
    mesh_faces.reserve(CGAL::faces(sm).size());

    QVector<MeshVertex> mesh_vertices;
    mesh_vertices.reserve(CGAL::vertices(sm).size());

    //! Compute normals of mesh faces and vertices
    auto vnormals =
        sm.add_property_map<MeshTypes::SM_VertexDescriptor, MeshTypes::Vector_3>("v:normals", CGAL::NULL_VECTOR).first;
    auto fnormals =
        sm.add_property_map<MeshTypes::SM_FaceDescriptor, MeshTypes::Vector_3>("f:normals", CGAL::NULL_VECTOR).first;
    CGAL::Polygon_mesh_processing::compute_normals(sm, vnormals, fnormals);

    for (const auto& v : sm.vertices())
        mesh_vertices.push_back(
            MeshVertex(Point(sm.point(v)).toQVector3D(), QVector3D(vnormals[v].x(), vnormals[v].y(), vnormals[v].z())));

    for (const auto& f : CGAL::faces(sm)) {
        int vertex_index = 0;
        MeshFace face;
        for (auto v_id : vertices_around_face(sm.halfedge(f), sm)) {
            face.vertex_index[vertex_index] = v_id;
            vertex_index++;
        }

        face.normal = QVector3D(fnormals[f].x(), fnormals[f].y(), fnormals[f].z());

        mesh_vertices[face.vertex_index[0]].connected_faces.push_back(mesh_faces.size());
        mesh_vertices[face.vertex_index[1]].connected_faces.push_back(mesh_faces.size());
        mesh_vertices[face.vertex_index[2]].connected_faces.push_back(mesh_faces.size());

        mesh_faces.push_back(face);
    }

    for (int faceIndex = 0; faceIndex < mesh_faces.size(); faceIndex++) {
        MeshFace& face = mesh_faces[faceIndex];
        face.connected_face_index[0] =
            GetFaceIdxWithPoints(face.vertex_index[0], face.vertex_index[1], faceIndex, mesh_vertices);
        face.connected_face_index[1] =
            GetFaceIdxWithPoints(face.vertex_index[1], face.vertex_index[2], faceIndex, mesh_vertices);
        face.connected_face_index[2] =
            GetFaceIdxWithPoints(face.vertex_index[2], face.vertex_index[0], faceIndex, mesh_vertices);
    }

    return std::pair<QVector<MeshVertex>, QVector<MeshFace>>(mesh_vertices, mesh_faces);
}

QSharedPointer<OpenMesh> OpenMesh::BuildMeshFromPointCloud(const QString& file_path) {
    QVector<MeshTypes::Point_3> p;

    QFileInfo file(file_path);

    QString suffix = file.suffix();
    if (suffix == "xyz")
        p = loadXYZFile(file_path);
    else if (suffix == "matrix")
        p = loadMatrixFile(file_path);

    if (p.isEmpty())
        return nullptr; // No points loaded

    std::vector<MeshTypes::Point_3> points(p.begin(), p.end());
    MeshTypes::SurfaceMesh m;

    try {
        ConstructPointCloud construct(m, points.begin(), points.end());
        CGAL::advancing_front_surface_reconstruction(points.begin(), points.end(), construct);

        auto new_mesh = QSharedPointer<OpenMesh>::create(m, file.baseName(), file_path);
        new_mesh->setType(MeshType::kBuild);
        return new_mesh;
    } catch (...) { return nullptr; }
}

void OpenMesh::convert() {
    m_representation = MeshTypes::SurfaceMesh(SurfaceMeshFromVerticesAndFaces(m_vertices, m_faces));
}

QVector<MeshTypes::Point_3> OpenMesh::loadMatrixFile(const QString& file_path) {
    // Open file
    QFile height(file_path);

    if (!height.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file";
        return QVector<MeshTypes::Point_3>();
    }

    QVector<MeshTypes::Point_3> points;
    double x_loc = 5.0;
    double y_loc = 5.0;
    while (!height.atEnd()) {
        QByteArray line = height.readLine();
        for (auto& entry : line.split(',')) {
            // Scale to micron
            points.append(MeshTypes::Point_3(x_loc * 1000, y_loc * 1000, entry.toDouble() * 1000));
            x_loc += 12.0;
        }
        x_loc = 5.0;
        y_loc += 12.0;
    }
    height.close();
    return points;
}

QVector<MeshTypes::Point_3> OpenMesh::loadXYZFile(const QString& file_path) {
    QVector<MeshTypes::Point_3> points;
    CGAL::IO::read_points(file_path.toStdString(), std::back_inserter(points));
    for (auto it = points.begin(); it != points.end(); ++it) // Scale to micron
        *it = MeshTypes::Point_3(it->x() * 1000, it->y() * 1000, it->z() * 1000);

    return points;
}
} // namespace ORNL
