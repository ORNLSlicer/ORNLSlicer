#pragma once

#include "CGAL/AABB_face_graph_triangle_primitive.h"
#include "CGAL/AABB_traits.h"
#include "CGAL/AABB_tree.h"
#include "CGAL/Exact_predicates_inexact_constructions_kernel.h"
#include "CGAL/Polyhedron_3.h"
#include "CGAL/Polyhedron_items_with_id_3.h"
#include "CGAL/Surface_mesh/Surface_mesh.h"

namespace ORNL {
/*!
 * \brief MeshTypes provides access to CGAL mesh types
 * \note CGAL uses two different structures for its meshes:
 *       1. Polyhedron: is a closed mesh structure that uses a pointer based structure to relate its vertices and faces
 *       2. SurfaceMesh: is a closed or open mesh structure that uses indices to relate its vertices and faces
 *  \note These types can be efficiently converted between each other by using:
 *  \code CGAL::copy_face_graph(source, destination);  \endcode
 */
namespace MeshTypes {
//! \brief CGAL 3D Space Type
typedef CGAL::Exact_predicates_inexact_constructions_kernel Kernel;

//! \brief CGAL 2D point type
typedef Kernel::Point_2 Point_2;

//! \brief CGAL 3D point type
typedef Kernel::Point_3 Point_3;

//! \brief CGAL 3D vector type
typedef Kernel::Vector_3 Vector_3;

//! \brief CGAL 3D Segment type
typedef Kernel::Segment_3 Segment_3;

//! \brief CGAL 3D Plane
typedef Kernel::Plane_3 Plane_3;

//! \brief CGAL 3D closed object type
typedef CGAL::Polyhedron_3<Kernel, CGAL::Polyhedron_items_with_id_3> Polyhedron;

//! \brief Halfedge of a polyhedron
typedef CGAL::Polyhedron_3<Kernel, CGAL::Polyhedron_items_with_id_3>::HalfedgeDS HalfedgeDescriptor;

//! \brief Face of a polyhedron
typedef boost::graph_traits<Polyhedron>::face_descriptor FaceDescriptor;

//! \brief Vertex of a polyhedron
typedef boost::graph_traits<Polyhedron>::vertex_descriptor VertexDescriptor;

//! \brief CGAL 3D open object type
typedef CGAL::Surface_mesh<Kernel::Point_3> SurfaceMesh;

//! \brief Halfedge of a surface mesh
typedef boost::graph_traits<SurfaceMesh>::halfedge_descriptor SM_HalfedgeDescriptor;

//! \brief Vertex of a surface mesh
typedef boost::graph_traits<SurfaceMesh>::vertex_descriptor SM_VertexDescriptor;

//! \brief Face of a surface mesh
typedef boost::graph_traits<SurfaceMesh>::face_descriptor SM_FaceDescriptor;

//! \brief Primitive of Polyhedron AABB
typedef CGAL::AABB_face_graph_triangle_primitive<Polyhedron> Polyhedron_AABB_Primitive;

//! \brief Traits of Polyhedron AABB
typedef CGAL::AABB_traits<Kernel, Polyhedron_AABB_Primitive> Polyhedron_AABB_Traits;

//! \brief Polyhedron AABB Tree
typedef CGAL::AABB_tree<Polyhedron_AABB_Traits> Polyhedron_AABB_Tree;

//! \brief Mapping between Polyhedron segments to primitives
typedef boost::optional<Polyhedron_AABB_Tree::Intersection_and_primitive_id<Segment_3>::Type>
    Polyhedron_Segment_intersection;

//! \brief Mapping between Polyhedron plane to primitives
typedef boost::optional<Polyhedron_AABB_Tree::Intersection_and_primitive_id<Plane_3>::Type>
    Polyhedron_Plane_intersection;

//! \brief Primitive of Polyhedron AABB
typedef CGAL::AABB_face_graph_triangle_primitive<SurfaceMesh> SurfaceMesh_AABB_Primitive;

//! \brief Traits of Polyhedron AABB
typedef CGAL::AABB_traits<Kernel, SurfaceMesh_AABB_Primitive> SurfaceMesh_AABB_Traits;

//! \brief Polyhedron AABB Tree
typedef CGAL::AABB_tree<SurfaceMesh_AABB_Traits> SurfaceMesh_AABB_Tree;

//! \brief Mapping between Polyhedron segments to primitives
typedef boost::optional<SurfaceMesh_AABB_Tree::Intersection_and_primitive_id<Segment_3>::Type>
    SurfaceMesh_Segment_intersection;

//! \brief Mapping between Polyhedron plane to primitives
typedef boost::optional<SurfaceMesh_AABB_Tree::Intersection_and_primitive_id<Plane_3>::Type>
    SurfaceMesh_Plane_intersection;

//! \brief a namespace for inexact cartesian sapce. This used to be the default kernal in ORNLSlicer, however is now used
//! when a package does not support the exact kernal above
namespace SimpleCartesian {
//! \brief CGAL 3D Space Type
typedef CGAL::Simple_cartesian<double> Kernel;

//! \brief CGAL 2D point type
typedef SimpleCartesian::Kernel::Point_2 Point_2;

//! \brief CGAL 3D point type
typedef SimpleCartesian::Kernel::Point_3 Point_3;

//! \brief CGAL 3D closed object type
typedef CGAL::Polyhedron_3<SimpleCartesian::Kernel, CGAL::Polyhedron_items_with_id_3> Polyhedron;

//! \brief CGAL 3D open object type
typedef CGAL::Surface_mesh<SimpleCartesian::Point_3> SurfaceMesh;

//! \brief Vertex of a surface mesh
typedef boost::graph_traits<SurfaceMesh>::vertex_descriptor SM_VertexDescriptor;

//! \brief Face of a surface mesh
typedef boost::graph_traits<SurfaceMesh>::face_descriptor SM_FaceDescriptor;
} // namespace SimpleCartesian
} // namespace MeshTypes
} // namespace ORNL
