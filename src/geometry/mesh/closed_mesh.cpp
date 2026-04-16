#include "geometry/mesh/closed_mesh.h"

#include <array>
#include <cstddef>
#include <iterator>
#include <list>
#include <utility>
#include <vector>

#include <CGAL/Aff_transformation_3.h>
#include <CGAL/Named_function_parameters.h>
#include <CGAL/Object.h>
#include <CGAL/Optimal_bounding_box/oriented_bounding_box.h>
#include <CGAL/Origin.h>
#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/repair_degeneracies.h>
#include <CGAL/Polygon_mesh_processing/repair_self_intersections.h>
#include <CGAL/Polygon_mesh_processing/transform.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Polygon_mesh_slicer.h>
#include <CGAL/Polyhedron_items_with_id_3.h>
#include <CGAL/aff_transformation_tags.h>
#include <CGAL/boost/graph/Face_filtered_graph.h>
#include <CGAL/boost/graph/copy_face_graph.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/boost/graph/graph_traits_Surface_mesh.h>
#include <CGAL/boost/graph/helpers.h>
#include <CGAL/boost/graph/iterator.h>
#include <CGAL/boost/graph/properties.h>
#include <CGAL/boost/graph/properties_Polyhedron_3.h>
#include <qcontainerfwd.h>
#include <qhashfunctions.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "geometry/mesh/advanced/mesh_types.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/mesh/mesh_face.h"
#include "geometry/mesh/mesh_vertex.h"
#include "geometry/mesh/open_mesh.h"
#include "geometry/path.h"
#include "geometry/plane.h"
#include "geometry/polygon.h"
#include "geometry/polyline.h"
#include "geometry/segments/line.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
ClosedMesh::ClosedMesh() : MeshBase() { m_is_closed = true; }

ClosedMesh::ClosedMesh(const QVector<MeshVertex>& vertices, const QVector<MeshFace>& faces)
    : MeshBase(vertices, faces) {
    m_representation = MeshTypes::Polyhedron(PolyhedronFromVerticesAndFaces(m_vertices, m_faces));
    m_original_representation = MeshTypes::Polyhedron(m_representation); // Create a copy
    updateDims();
    m_is_closed = true;
}

ClosedMesh::ClosedMesh(const QString& name, const QString& path, const QVector<MeshVertex>& vertices,
                       const QVector<MeshFace>& faces, MeshType type)
    : MeshBase(vertices, faces, name, path, type) {
    m_representation = MeshTypes::Polyhedron(PolyhedronFromVerticesAndFaces(m_vertices, m_faces));
    m_original_representation = MeshTypes::Polyhedron(m_representation); // Create a copy
    updateDims();
    m_is_closed = true;
}

ClosedMesh::ClosedMesh(MeshTypes::Polyhedron poly, QString name, QString file) : MeshBase() {
    m_name = name;
    m_file = file;

    CGAL::copy_face_graph(poly, m_representation);

    auto vertices_and_faces = FacesAndVerticesFromPolyhedron(m_representation);
    m_vertices_original = m_vertices = m_vertices_aligned = vertices_and_faces.first;
    m_faces_original = m_faces = m_faces_aligned = vertices_and_faces.second;
    CGAL::copy_face_graph(m_representation, m_original_representation);
    updateDims();
    m_original_dimensions = m_dimensions;
    m_is_closed = true;
}

ClosedMesh::ClosedMesh(QSharedPointer<ClosedMesh> mesh) : MeshBase(mesh) {
    m_representation = mesh->m_representation;
    m_original_representation = mesh->m_original_representation;
    m_is_closed = true;
}

MeshTypes::Polyhedron ClosedMesh::polyhedron() { return m_representation; }

void ClosedMesh::center() {
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

QVector<Point> ClosedMesh::optimalBoundingBox() {
    std::array<MeshTypes::Point_3, 8> obb_points;
    CGAL::oriented_bounding_box(m_representation, obb_points, CGAL::parameters::use_convex_hull(true));
    QVector<Point> points;
    return points;
}

QVector<Point> ClosedMesh::boundingBox() {
    MeshTypes::Polyhedron_AABB_Tree tree(CGAL::faces(m_representation).first, CGAL::faces(m_representation).second,
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

Area ClosedMesh::area() {
    double area = 0.0;
    area = CGAL::Polygon_mesh_processing::area(m_representation);
    Area a;
    return a.from(area, (micron * micron));
}

Volume ClosedMesh::volume() {
    double volume = 0.0;
    volume = CGAL::Polygon_mesh_processing::volume(m_representation);
    Volume v;
    return v.from(volume, (micron * micron * micron));
}

QVector<Point> ClosedMesh::intersect(Point start, Point end) {
    MeshTypes::Polyhedron_AABB_Tree tree(CGAL::faces(m_representation).first, CGAL::faces(m_representation).second,
                                         m_representation);
    MeshTypes::Kernel::Segment_3 segment(start.toCartesian3D(), end.toCartesian3D());

    std::list<MeshTypes::Polyhedron_Segment_intersection> intersections;
    tree.all_intersections(segment, std::back_inserter(intersections));

    QVector<Point> intersection_points;
    for (auto intersection : intersections) {
        auto point = boost::get<MeshTypes::Point_3>(&(intersection->first));
        intersection_points.push_back(Point(*point));
    }
    return intersection_points;
}

QVector<Point> ClosedMesh::intersect(LineSegment line) { return intersect(line.start(), line.end()); }

QVector<Point> ClosedMesh::intersect(Polyline line) {
    QVector<Point> intersections;
    for (int i = 1, end = line.size(); i < end; ++i)
        intersections.append(intersect(line[i - 1], line[i]));

    return intersections;
}

QVector<Point> ClosedMesh::intersect(Path path) {
    QVector<Point> intersections;
    for (auto& seg : path)
        intersections.append(intersect(seg->start(), seg->end()));

    if (path.front()->start() == path.back()->end()) // Is this path is a closed loop
        intersections.append(intersect(path.back()->end(), path.front()->start()));

    return intersections;
}

QVector<Point> ClosedMesh::intersect(Polygon poly) {
    QVector<Point> intersections;
    for (int i = 1, end = poly.size(); i < end; ++i)
        intersections.append(intersect(poly[i - 1], poly[i]));

    intersections.append(intersect(poly.last(), poly.first())); // Add loop back line

    return intersections;
}

std::pair<QVector<Polyline>, QVector<Polygon>> ClosedMesh::intersect(Plane plane) {
    // Slicer constructor from the mesh
    CGAL::Polygon_mesh_slicer<MeshTypes::Polyhedron, MeshTypes::Kernel> slicer(m_representation);

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
            result_polygons.push_back(new_polygon);
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

void ClosedMesh::difference(ClosedMesh& clipper) {
    auto clip = clipper.polyhedron();
    CGAL::Polygon_mesh_processing::corefine_and_compute_difference(m_representation, clip, m_representation);

    // Convert back to a mesh
    auto vertices_and_faces = FacesAndVerticesFromPolyhedron(m_representation);
    m_vertices = vertices_and_faces.first;
    m_faces = vertices_and_faces.second;

    updateDims();
}

void ClosedMesh::intersection(ClosedMesh mesh_to_intersect) {
    // Convert both to CGAL polyhedrons
    MeshTypes::Polyhedron subject = polyhedron();
    MeshTypes::Polyhedron clip = mesh_to_intersect.polyhedron();

    MeshTypes::Polyhedron out;
    CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(subject, clip, out);

    // Convert back to a mesh
    auto vertices_and_faces = FacesAndVerticesFromPolyhedron(out);
    m_vertices = vertices_and_faces.first;
    m_faces = vertices_and_faces.second;

    updateDims();
}

void ClosedMesh::mesh_union(ClosedMesh mesh_to_union) {
    // Convert both to CGAL polyhedrons
    MeshTypes::Polyhedron subject = polyhedron();
    MeshTypes::Polyhedron clip = mesh_to_union.polyhedron();

    MeshTypes::Polyhedron out;
    CGAL::Polygon_mesh_processing::corefine_and_compute_union(subject, clip, out);

    // Convert back to a mesh
    auto vertices_and_faces = FacesAndVerticesFromPolyhedron(out);
    m_vertices = vertices_and_faces.first;
    m_faces = vertices_and_faces.second;

    updateDims();
}

QSharedPointer<OpenMesh> ClosedMesh::toOpenMesh() {
    MeshTypes::SurfaceMesh output;
    CGAL::copy_face_graph(m_representation, output);
    auto new_mesh = QSharedPointer<OpenMesh>::create(output, this->name(), this->path());
    return new_mesh;
}

MeshTypes::SurfaceMesh ClosedMesh::extractUpwardFaces() {
    CGAL::set_halfedgeds_items_id(m_representation);
    std::vector<std::size_t> segment_ids(CGAL::num_faces(m_representation));
    FacetPropMap<std::size_t> segment_property_map(segment_ids);

    for (auto face : CGAL::faces(m_representation)) {
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
    FilteredMesh segment_mesh(m_representation, 0, segment_property_map);
    segment_mesh.set_selected_faces(1, segment_property_map);

    MeshTypes::SurfaceMesh output;
    CGAL::copy_face_graph(segment_mesh, output);
    return output;
}

std::pair<ClosedMesh, ClosedMesh> ClosedMesh::splitWithPlane(Plane plane) {
    MeshTypes::Polyhedron internal_mesh = m_representation;
    MeshTypes::Polyhedron negative_mesh;
    MeshTypes::Polyhedron positive_mesh;

    CGAL::copy_face_graph(internal_mesh, negative_mesh);
    CGAL::copy_face_graph(internal_mesh, positive_mesh);

    CGAL::Polygon_mesh_processing::clip(negative_mesh, plane.toCGALPlane(),
                                        CGAL::Polygon_mesh_processing::parameters::clip_volume(true));

    // Invert plane normal
    plane.normal(-plane.normal());

    CGAL::Polygon_mesh_processing::clip(positive_mesh, plane.toCGALPlane(),
                                        CGAL::Polygon_mesh_processing::parameters::clip_volume(true));

    return std::pair<ClosedMesh, ClosedMesh>(ClosedMesh(negative_mesh), ClosedMesh(positive_mesh));
}

std::pair<bool, Area> ClosedMesh::crossSectionalArea(Plane plane, QVector<Polygon> boundary_curves) {
    // Take cross-section to find area
    std::list<std::vector<MeshTypes::Point_3>> cross_section;
    CGAL::Polygon_mesh_slicer<MeshTypes::Polyhedron, MeshTypes::Kernel> slicer(m_representation);
    slicer(plane.toCGALPlane(), std::back_inserter(cross_section));

    Area a = 0;
    bool intersecting = false;
    if (cross_section.size() == 0) // No cross section generated
        a = 0;
    else {
        for (auto section : cross_section) {
            auto list = cross_section.front();
            Polyline polyline(list);
            a += polyline.close().area();
        }
    }

    // We also need to check to see if it intersects with any other cuts
    return std::pair<bool, Area>(intersecting || CheckIntersectingCurves(plane, boundary_curves), a);
}

MeshTypes::Polyhedron ClosedMesh::PolyhedronFromVerticesAndFaces(QVector<MeshVertex>& vertices,
                                                                 QVector<MeshFace>& faces) {
    MeshTypes::Polyhedron mesh;
    MeshBuilder<MeshTypes::HalfedgeDescriptor> builder(vertices, faces);
    mesh.delegate(builder);
    return mesh;
}

std::pair<QVector<MeshVertex>, QVector<MeshFace>>
ClosedMesh::FacesAndVerticesFromPolyhedron(MeshTypes::Polyhedron& mesh) {
    QVector<MeshFace> mesh_faces;
    mesh_faces.reserve(CGAL::faces(mesh).size());

    QVector<MeshVertex> mesh_vertices;
    mesh_vertices.reserve(CGAL::vertices(mesh).size());

    //! \note Because a surface mesh is indexed and not pointer based, it is faster to let CGAL index the polyhedron
    //!       and then convert it to faces and vertices.
    MeshTypes::SurfaceMesh sm;

    CGAL::copy_face_graph(mesh, sm);

    //! Compute normals of mesh faces and vertices
    auto vnormals =
        sm.add_property_map<MeshTypes::SM_VertexDescriptor, MeshTypes::Vector_3>("v:normals", CGAL::NULL_VECTOR).first;
    auto fnormals =
        sm.add_property_map<MeshTypes::SM_FaceDescriptor, MeshTypes::Vector_3>("f:normals", CGAL::NULL_VECTOR).first;
    CGAL::Polygon_mesh_processing::compute_normals(sm, vnormals, fnormals);

    for (auto v : sm.vertices())
        mesh_vertices.push_back(
            MeshVertex(Point(sm.point(v)).toQVector3D(), QVector3D(vnormals[v].x(), vnormals[v].y(), vnormals[v].z())));

    for (auto f : CGAL::faces(sm)) {
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

void ClosedMesh::CleanPolyhedron(MeshTypes::Polyhedron& polyhedron) {
    CGAL::Polygon_mesh_processing::remove_degenerate_faces(polyhedron);
    CGAL::Polygon_mesh_processing::remove_isolated_vertices(polyhedron);
    CGAL::Polygon_mesh_processing::remove_connected_components_of_negligible_size(polyhedron);
    CGAL::Polygon_mesh_processing::experimental::remove_self_intersections(polyhedron);

    // If the mesh is not closed, fill holes
    if (!polyhedron.is_closed()) {
        for (boost::graph_traits<MeshTypes::Polyhedron>::halfedge_descriptor h : halfedges(polyhedron)) {
            if (CGAL::is_border(h, polyhedron)) {
                std::vector<boost::graph_traits<MeshTypes::Polyhedron>::face_descriptor> patch_facets;
                std::vector<boost::graph_traits<MeshTypes::Polyhedron>::vertex_descriptor> patch_vertices;
                bool success = std::get<0>(CGAL::Polygon_mesh_processing::triangulate_refine_and_fair_hole(
                    polyhedron, h, std::back_inserter(patch_facets), std::back_inserter(patch_vertices),
                    CGAL::parameters::vertex_point_map(get(CGAL::vertex_point, polyhedron))
                        .geom_traits(MeshTypes::Kernel())));
            }
        }
    }
}

void ClosedMesh::convert() {
    m_representation = MeshTypes::Polyhedron(PolyhedronFromVerticesAndFaces(m_vertices, m_faces));
}

bool ClosedMesh::CheckIntersectingCurves(Plane& plane, QVector<Polygon> boundary_curves) {
    for (Polygon curve : boundary_curves) {
        for (Point p : curve) {
            if (plane.evaluatePoint(p) >= 0)
                return true;
        }
    }
    return false;
}
} // namespace ORNL
