#include "geometry/mesh/mesh_vertex.h"

#include <QMatrix4x4>
#include <qvectornd.h>

#include "geometry/mesh/advanced/mesh_types.h"

namespace ORNL {
MeshVertex::MeshVertex(const QVector3D& location, const QVector3D& normal) : location(location), normal(normal) {
    connected_faces.reserve(8);
}

void MeshVertex::transform(const QMatrix4x4& transform) {
    location = transform * location; // QVector3D(transform * QVector4D(location));
}

MeshTypes::Point_3 MeshVertex::toPoint3() const { return MeshTypes::Point_3(location.x(), location.y(), location.z()); }

bool operator<(const MeshVertex& lhs, const MeshVertex& rhs) {
    if (lhs.location.x() < rhs.location.x()) {
        return true;
    }
    else if (lhs.location.x() > rhs.location.x()) {
        return false;
    }
    // lhs.location.x() == rhs.location.x()
    else if (lhs.location.y() < rhs.location.y()) {
        return true;
    }
    else if (lhs.location.y() > rhs.location.y()) {
        return false;
    }
    // lhs.location.y() == rhs.location.y()
    else if (lhs.location.z() < rhs.location.z()) {
        return true;
    }
    else if (lhs.location.z() > rhs.location.z()) {
        return false;
    }
    // lhs.location.z() == rhs.location.z()
    else {
        return false;
    }
}
} // namespace ORNL
