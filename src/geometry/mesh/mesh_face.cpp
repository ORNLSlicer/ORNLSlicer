#include "geometry/mesh/mesh_face.h"

#include <qvectornd.h>

namespace ORNL {
MeshFace::MeshFace() {
    for (int i = 0; i < 3; i++) {
        vertex_index[i] = -1;
        connected_face_index[i] = -1;
    }
}

MeshFace::MeshFace(int* vertices, int* connected_faces, QVector3D _normal, bool _ignore) {
    vertex_index[0] = vertices[0];
    vertex_index[1] = vertices[1];
    vertex_index[2] = vertices[2];

    connected_face_index[0] = connected_faces[0];
    connected_face_index[1] = connected_faces[1];
    connected_face_index[2] = connected_faces[2];

    normal = _normal;

    ignore = _ignore;
}
} // namespace ORNL
