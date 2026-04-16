#include "geometry/mesh/advanced/mesh_skeleton.h"

#include "CGAL/AABB_face_graph_triangle_primitive.h"
#include "CGAL/AABB_traits.h"
#include "CGAL/AABB_tree.h"
#include "CGAL/Polygon_mesh_processing/repair_self_intersections.h"
#include "CGAL/Polygon_mesh_processing/self_intersections.h"
#include "utilities/mathutils.h"

namespace ORNL {
MeshSkeleton::MeshSkeleton() {}

MeshSkeleton::MeshSkeleton(QSharedPointer<ClosedMesh> mesh) : m_mesh(mesh) {}

MeshSkeleton::MeshSkeleton(MeshTypes::Polyhedron mesh) { CGAL::copy_face_graph(mesh, m_polyhedron); }

void MeshSkeleton::compute() {
    // Convert to CGAL Type if we have an ORNLSlicer type
    if (m_mesh != nullptr)
        CGAL::copy_face_graph(m_mesh->polyhedron(), m_polyhedron);

    // Verify preconditions
    assert(CGAL::is_triangle_mesh(m_polyhedron));
    assert(CGAL::is_closed(m_polyhedron));

    if (CGAL::Polygon_mesh_processing::does_self_intersect(m_polyhedron)) // Fix any self-intersections
        CGAL::Polygon_mesh_processing::experimental::remove_self_intersections(m_polyhedron);
    // CGAL::Polygon_mesh_processing::remove_self_intersections(m_polyhedron);

    //! \brief Performs the MCF mesh skeletonization.
    //! \note the mesh must be closed and triangulated
    CGAL::extract_mean_curvature_flow_skeleton(m_polyhedron, m_skeletonization);

    // Extract each point
    for (Skeletonization::Skeleton::vertex_descriptor v : CGAL::make_range(vertices(m_skeletonization))) {
        m_skeleton.push_back(MeshTypes::Point_3(m_skeletonization[v].point.x(), m_skeletonization[v].point.y(),
                                                m_skeletonization[v].point.z()));
    }
}

void MeshSkeleton::order() {
    //! \note  To perform a curve fit the points must be in order along the curve.
    //! \brief This is done: Compare Z -> Compare X -> Compare Y. If the values are
    //!        within 10 microns they are considered reasonably close
    std::sort(m_skeleton.begin(), m_skeleton.end(), [](const Point& lhs, const Point& rhs) {
        bool isLess;

        //! \note Epsilon is used to here compare reasonably close values. We use 10 microns here
        double epsilon = 10.0;
        if (fabs(lhs.z() - rhs.z()) < epsilon)     // Z is same
            if (fabs(lhs.x() - rhs.x()) < epsilon) // X is same
                isLess = lhs.y() < rhs.y();        // Compare by Y
            else
                isLess = lhs.x() < rhs.x(); // Compare by X
        else
            isLess = lhs.z() < rhs.z(); // Compare Z

        return isLess;
    });
}

void MeshSkeleton::extend() {
    typedef MeshTypes::SimpleCartesian::Kernel::Point_3 Point3;
    typedef MeshTypes::SimpleCartesian::Kernel::Segment_3 Segment3;
    typedef CGAL::AABB_face_graph_triangle_primitive<MeshTypes::SimpleCartesian::Polyhedron> Primitive;
    typedef CGAL::AABB_traits<MeshTypes::SimpleCartesian::Kernel, Primitive> Traits;
    typedef CGAL::AABB_tree<Traits> Tree;
    typedef boost::optional<Tree::Intersection_and_primitive_id<Segment3>::Type> Segment_intersection;

    // Builds an AABB tree
    Tree tree(faces(m_polyhedron).first, faces(m_polyhedron).second, m_polyhedron);

    // The farthest away the ray can exit is the diagonal of the bounding box
    double max_len = qSqrt(qPow(m_mesh->dimensions().x(), 2) + qPow(m_mesh->dimensions().y(), 2) +
                           qPow(m_mesh->dimensions().z(), 2));

    // Compute the end segment
    Point start = m_skeleton.back();
    Plane plane = getFinalPlane();
    plane.shiftAlongNormal(max_len);
    Point end = plane.point();

    auto start_p = start.toCartesian3D();
    auto end_p = end.toCartesian3D();

    Segment3 end_segment_query(MeshTypes::SimpleCartesian::Point_3(start_p.x(), start_p.y(), start_p.z()),
                               MeshTypes::SimpleCartesian::Point_3(end_p.x(), end_p.y(), end_p.z()));

    Segment_intersection end_intersection = tree.any_intersection(end_segment_query);
    if (end_intersection) {
        // gets intersection object
        const Point3* end_point = boost::get<Point3>(&(end_intersection->first));
        if (end_point) {
            auto new_point = *end_point;
            m_skeleton.push_back(Point(new_point.x(), new_point.y(), new_point.z()));
        }
    }

    // Compute the start segment
    start = m_skeleton.front();
    plane = getFirstPlane();
    plane.shiftAlongNormal(max_len);
    end = plane.point();

    start_p = start.toCartesian3D();
    end_p = end.toCartesian3D();

    Segment3 start_segment_query(MeshTypes::SimpleCartesian::Point_3(start_p.x(), start_p.y(), start_p.z()),
                                 MeshTypes::SimpleCartesian::Point_3(end_p.x(), end_p.y(), end_p.z()));

    Segment_intersection start_intersection = tree.any_intersection(start_segment_query);
    if (start_intersection) {
        // gets intersection object
        const Point3* start_point = boost::get<Point3>(&(start_intersection->first));
        if (start_point) {
            auto new_point = *start_point;
            m_skeleton.push_back(Point(new_point.x(), new_point.y(), new_point.z()));
        }
    }

    // Project the start point down to the lowest z in case it was not already there
    if (m_mesh->min().z() < m_skeleton.front().z())
        m_skeleton.front().z(m_mesh->min().z());

    // Project the end point up to the highest z in case it was not already there
    if (m_mesh->max().z() > m_skeleton.last().z())
        m_skeleton.last().z(m_mesh->max().z());

    Plane initial = getFirstPlane();
    // This need to be flipped to point up the curve
    initial.normal(-initial.normal());
    m_last_plane = initial;
}

Point MeshSkeleton::getPointOnBezierCurve(double time) {
    double x = 0;
    double y = 0;
    double z = 0;

    int order = m_skeleton.size() - 1;

    //! \brief This calculates the point on the n-th order bezier curve at a given time value: SUM( nCi * (1-t)^(3-i) *
    //! t^i * Point(x/y/z) ) \note T is on the interval 0 to 1 \note The order is the number of skeleton points minus 1
    for (int pointIndex = 0; pointIndex <= order; pointIndex++) {
        x += MathUtils::findBinomialCoefficients(order, pointIndex) * powf((1 - time), (order - pointIndex)) *
             powf(time, pointIndex) * m_skeleton[pointIndex].x();
        y += MathUtils::findBinomialCoefficients(order, pointIndex) * powf((1 - time), (order - pointIndex)) *
             powf(time, pointIndex) * m_skeleton[pointIndex].y();
        z += MathUtils::findBinomialCoefficients(order, pointIndex) * powf((1 - time), (order - pointIndex)) *
             powf(time, pointIndex) * m_skeleton[pointIndex].z();
    }

    return Point(x, y, z);
}

Plane MeshSkeleton::getFinalPlane() {
    //! \note The the normal vector is approximately the same as the final normal on the bezier curve
    Point final = getPointOnBezierCurve(1);
    Point previous = getPointOnBezierCurve(0.99);

    QVector3D normal = (final - previous).toQVector3D();
    normal.normalize();

    return Plane(final, normal);
}

Plane MeshSkeleton::getFirstPlane() {
    //! \note The the normal vector is approximately the same as the first normal on the bezier curve
    Point final = getPointOnBezierCurve(0);
    Point previous = getPointOnBezierCurve(0.01);

    QVector3D normal = (final - previous).toQVector3D();
    normal.normalize();

    if (normal.z() > 0.0) // The normal must point down towards the bed
        normal = -normal;

    return Plane(final, normal);
}

Point MeshSkeleton::getFirstPoint() { return m_skeleton.first(); }

Point MeshSkeleton::getLastPoint() { return m_skeleton.last(); }

MeshSkeleton::Skeletonization::Skeleton MeshSkeleton::getSkeleton() { return m_skeletonization; }

void MeshSkeleton::setPlane(Plane& p) { m_last_plane = p; }

Plane MeshSkeleton::findNextPlane(Distance layer_height) {
    Point last_point = m_last_plane.point();
    Point next_point;

    double start_time = m_time;

    do {
        m_time += 0.001; // A very small interval is used to approximate more accurately
        next_point = getPointOnBezierCurve(m_time);

        Distance traveled = (last_point - next_point).toQVector3D().length();

        if (traveled == layer_height) // The next value of time is the right length
        {
            m_last_plane.point(next_point);
            QVector3D normal = (next_point - last_point).toQVector3D();
            normal.normalize();
            m_last_plane.normal(normal);
            break;
        }
        else if (traveled > layer_height) // The next value of time is past the layer height
        {
            // Find the exact value between the points
            QVector3D vec = (next_point - last_point).toQVector3D();
            vec.normalize();
            vec *= layer_height();
            last_point = last_point + vec;

            m_last_plane.point(last_point);
            QVector3D normal = (next_point - last_point).toQVector3D();
            normal.normalize();
            m_last_plane.normal(normal);
            break;
        }
        else // We have not travel far enough yet
        {
            layer_height -= traveled;
        }
        last_point = next_point;

    } while (m_time < 1.0);

    // If we are past the end then shift along final normal
    if (m_time >= 1.0) {
        QVector3D vec = m_last_plane.normal();
        vec.normalize();
        vec *= layer_height();
        m_last_plane.point(last_point + vec);
    }

    // If this was the first plane, then override its normal
    if (start_time == 0.0) {
        // The first layer must be perpendicular to the bed
        m_last_plane.normal(QVector3D(0, 0, 1));
    }

    return m_last_plane;
}
} // namespace ORNL
