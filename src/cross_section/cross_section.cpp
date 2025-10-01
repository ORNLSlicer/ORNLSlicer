#include "cross_section/cross_section.h"

#include "cross_section/cross_section_object.h"
#include "cross_section/cross_section_segment.h"
#include "utilities/mathutils.h"

namespace ORNL {
PolygonList CrossSection::doCrossSection(QSharedPointer<MeshBase> mesh, Plane& slicing_plane, Point& shift,
                                         QVector3D& averageNormal, QSharedPointer<SettingsBase> sb) {
    //! \brief Helper for doCrossSection(). Exists as lamda to simplfy external interface (i.e. not show helpers).
    // given a point, a rotation, and a shift_amount: does **negative** shift of point and then rotation
    auto rotatePoint = [](Point point_to_rotate, QQuaternion rotation, Point& shift_amount) {
        // shift to rotate around origin
        point_to_rotate = point_to_rotate - shift_amount;

        // rotate
        QVector3D point_vec = point_to_rotate.toQVector3D();
        QVector3D result = rotation.rotatedVector(point_vec);

        // reapply translate to account for origin offset
        result.setX(result.x() + shift_amount.x());
        result.setY(result.y() + shift_amount.y());

        return Point(result); // will un-rotate and un-shift later, just before gcode writing
    };

    QVector<MeshVertex> vertices = mesh->vertices();
    QVector<MeshFace> faces = mesh->faces();

    /*
     * Check every face (triangle) to see if it is cut by the slicing plane
     * If so, generate a segment at the intersection
     */
    CrossSectionObject cs(sb);
    int num_intersections = 0;
    QVector3D normal_accumulator(0, 0, 0);
    for (int m = 0, end = faces.size(); m < end; ++m) {
        const MeshFace& face = faces[m];

        // Skip ignored faces
        if (face.ignore)
            continue;

        const MeshVertex& v0 = vertices[face.vertex_index[0]];
        const MeshVertex& v1 = vertices[face.vertex_index[1]];
        const MeshVertex& v2 = vertices[face.vertex_index[2]];

        // the points of the triangle
        Point p0 = v0.location;
        Point p1 = v1.location;
        Point p2 = v2.location;

        CrossSectionSegment segment;
        segment.end_vertex = nullptr;
        int end_edge_idx = -1;

        /*
           Each point is evaluated in the plane equation to
           determine which side of the plane the point is on.
           If pt_eval > 0, it is above the plane
                      < 0, below the plane
                      = 0, on the plane
        */
        float p0_eval = slicing_plane.evaluatePoint(p0);
        float p1_eval = slicing_plane.evaluatePoint(p1);
        float p2_eval = slicing_plane.evaluatePoint(p2);

        bool intersection = false;
        // Check each orientation of points & plane that intersect
        if (p0_eval < 0 && p1_eval >= 0 && p2_eval >= 0) {
            // p2   p1
            // --------
            //   p0
            end_edge_idx = 0;
            segment.start = findIntersection(p0, p2, slicing_plane);
            segment.end = findIntersection(p0, p1, slicing_plane);
            if (p1_eval == 0) {
                segment.end_vertex = &v1;
            }
            intersection = true;
        }
        else if (p0_eval > 0 && p1_eval < 0 && p2_eval < 0) {
            //   p0
            // --------
            // p1  p2

            end_edge_idx = 2;
            segment.start = findIntersection(p1, p0, slicing_plane);
            segment.end = findIntersection(p2, p0, slicing_plane);
            intersection = true;
        }
        else if (p1_eval < 0 && p0_eval >= 0 && p2_eval >= 0) {
            // p0   p2
            // --------
            //   p1
            end_edge_idx = 1;
            segment.start = findIntersection(p1, p0, slicing_plane);
            segment.end = findIntersection(p1, p2, slicing_plane);
            if (p2_eval == 0) {
                segment.end_vertex = &v2;
            }
            intersection = true;
        }
        else if (p1_eval > 0 && p0_eval < 0 && p2_eval < 0) {
            //   p1
            // --------
            // p2  p0
            end_edge_idx = 0;
            segment.start = findIntersection(p2, p1, slicing_plane);
            segment.end = findIntersection(p0, p1, slicing_plane);
            intersection = true;
        }
        else if (p2_eval < 0 && p1_eval >= 0 && p0_eval >= 0) {
            // p1   p0
            // --------
            //   p2
            end_edge_idx = 2;
            segment.start = findIntersection(p2, p1, slicing_plane);
            segment.end = findIntersection(p2, p0, slicing_plane);
            if (p0_eval == 0) {
                segment.end_vertex = &v0;
            }
            intersection = true;
        }
        else if (p2_eval > 0 && p1_eval < 0 && p0_eval < 0) {
            //   p2
            // --------
            // p0  p1
            end_edge_idx = 1;
            segment.start = findIntersection(p0, p2, slicing_plane);
            segment.end = findIntersection(p1, p2, slicing_plane);
            intersection = true;
        }
        else {
            // Not all cases create a segment, because a point of a face
            // could create just a dot, and two touching faces
            //  on the slice would create two segments
            continue;
        }

        if (intersection) {
            ++num_intersections;
            normal_accumulator += face.normal;
        }
        /*
         * Polygon operations that follow cross sectioning are done in 2 dimensions.
         * To allow that, shift and rotate each segment so that the cross-section will be flat in 2D
         * Polymer_slicer save shift and rotation amount to layer so that this is un-done before gcode writing
         */

        // modify shift parameter so that it can be saved to layer outside of this function
        shift = findSlicingPlaneMidPoint(mesh, slicing_plane);

        // define rotation using the direction of the slicing plane
        QQuaternion rotation = MathUtils::CreateQuaternion(slicing_plane.normal(), QVector3D(0, 0, 1));
        segment.start = rotatePoint(segment.start, rotation, shift);
        segment.end = rotatePoint(segment.end, rotation, shift);

        // add segment to cross-section
        cs.insertFaceToSegment(m, cs.segments().size());
        segment.face_index = m;
        segment.end_other_face_idx = face.connected_face_index[end_edge_idx];
        segment.added_to_polygon = false;
        segment.normal = face.normal;
        cs.addSegment(segment);
    }

    // Only set average normal if there was at least one intersection
    if (num_intersections > 0) {
        averageNormal = normal_accumulator / static_cast<float>(num_intersections);
        averageNormal.normalize();
    }

    return cs.makePolygons();
}

Point CrossSection::findSlicingPlaneMidPoint(QSharedPointer<MeshBase> mesh, Plane& slicing_plane) {
    Point center = (mesh->min() + mesh->max()) / 2; // center of the bounding box
    Distance max_dimension = mesh->min().distance(mesh->max());
    QVector3D plane_normal = slicing_plane.normal().normalized() * max_dimension();

    // calculate a 'center_line' that is parallel to the slicing plane normal and runs through the center
    // end points are arbitrary as long as they are outside the bounding box
    Point center_line_min = center - plane_normal;
    Point center_line_max = center + plane_normal;

    // the plane midpoint is the intersection of the center_normal line and the plane
    center = findIntersection(center_line_min, center_line_max, slicing_plane);
    return center;
}

Point CrossSection::findIntersection(Point& vertex0, Point& vertex1, Plane& slicing_plane) {
    // slope of the line
    float dx = vertex1.x() - vertex0.x();
    float dy = vertex1.y() - vertex0.y();
    float dz = vertex1.z() - vertex0.z();

    // diff between plane and vertex0
    float plane_dx = slicing_plane.point().x() - vertex0.x();
    float plane_dy = slicing_plane.point().y() - vertex0.y();
    float plane_dz = slicing_plane.point().z() - vertex0.z();

    float numerator = (slicing_plane.normal().x() * plane_dx) + (slicing_plane.normal().y() * plane_dy) +
                      (slicing_plane.normal().z() * plane_dz);

    float denominator =
        (slicing_plane.normal().x() * dx) + (slicing_plane.normal().y() * dy) + (slicing_plane.normal().z() * dz);

    float t = numerator / denominator;

    Point intersection_point;
    intersection_point.x(dx * t + vertex0.x());
    intersection_point.y(dy * t + vertex0.y());
    intersection_point.z(dz * t + vertex0.z());

    return intersection_point;
}

} // namespace ORNL
