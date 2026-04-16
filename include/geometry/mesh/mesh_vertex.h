#pragma once

#include <QVector3D>
#include <QVector>
#include <qcontainerfwd.h>

#include "geometry/mesh/advanced/mesh_types.h"

namespace ORNL {
/*!
 * \class MeshVertex
 *
 * \brief MeshVertex type to be used in a Mesh.
 *
 * Keeps track of which faces connect to it.
 */
class MeshVertex {
  public:
    /*!
     * \brief Constructor
     *
     * \param location The location of this vertex
     * \param normal The normal vector for this vertex
     */
    MeshVertex(const QVector3D& location = QVector3D(), const QVector3D& normal = QVector3D());

    // Transform this vertex by applying the passed matrix.
    void transform(const QMatrix4x4& transform);

    //! \brief Converts the mesh vertex to a CGAL cartesian point
    MeshTypes::Point_3 toPoint3() const;

    QVector3D location; //!< Location of the vertex
    QVector3D normal;
    QVector<int> connected_faces; //!< list of the indices of connected faces
};
} // namespace ORNL
