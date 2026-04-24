#pragma once

#include <qvectornd.h>

#include "geometry/point.h"

namespace ORNL {
class MeshVertex;

/*!
 * \class CrossSectionSegment
 * \brief Basic struct that holds information about vertices and face indicies for cross sectioning
 *
 * Based on the Cura Engine by Ultimaker
 */
class CrossSectionSegment {
  public:
    CrossSectionSegment();

    //! \brief Start and end points
    Point start, end;

    //! \brief Index of face
    int face_index;

    //! \brief Index of other face connected via the edge that created end
    int end_other_face_idx;

    //! \brief End vertex
    const MeshVertex* end_vertex;

    //! \brief Whether or not segment has been added to polygon
    bool added_to_polygon;

    //! \brief Face normal
    QVector3D normal;
};
} // namespace ORNL
