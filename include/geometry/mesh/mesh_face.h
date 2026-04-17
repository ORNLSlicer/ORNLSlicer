#pragma once

#include <QVector3D>

namespace ORNL {
/*!
 * \class MeshFace
 * \brief A MeshFace is a 3 dimensional model triangle with 3 points. These
 * points are already converted to integers
 *
 * A face has 3 connected faces, corresponding to its 3 edges.
 * Note that a correct model may have more than 2 faces connected via a
 * single edge! In such a case the face_index stored in connected_face_index
 * is the one connected via the outside; see ASCII art below:
 *
 * : horizontal slice through vertical edge connected to four faces :
 *
 * \verbatim
 * [inside] x|
 *          x| <--+--- faces which contain each other in their
 * connected_face_index fiels
 *    xxxxxxx|   \|/
 *    -------+-------
 *       ^   |xxxxxxx
 *       +-->|x
 *       |   |x [inside]
 *       |
 *     faces which contain each other in their connected_face_index fields
 * \endverbatim
 */
class MeshFace {
  public:
    //! \brief Constructor
    MeshFace();

    //! \brief Constructor
    //! \param vertices: a pointer to 3 vertices
    //! \param connected_faces: a pointer to 3 connected faces
    //! \param _normal: the normal vector of this face
    //! \param _ignore: if this face should be ignored when cross-sectioning (defaults to false)
    MeshFace(int* vertices, int* connected_faces, QVector3D _normal, bool _ignore = false);

    //! \brief vertices in counter-clockwise ordering
    int vertex_index[3];

    //! \brief faces that are connected to this one
    int connected_face_index[3]; //!< same ordering as vertex_index
                                 //!(connected_face is connected via vertex
                                 //! 0 and
                                 //! 1, etc)

    //! \brief the normal of this face
    QVector3D normal;

    //! \brief a flag to ignore this face when cross-sectioning
    bool ignore = false;
};
} // namespace ORNL
