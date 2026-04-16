#pragma once

#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/mesh/mesh_base.h"
#include "geometry/plane.h"
#include "geometry/polygon_list.h"

namespace ORNL {

class CrossSectionSegment;

//! \brief provides access to functions that cross section meshes
namespace CrossSection {

//! @brief Computes the cross-section for a mesh and a plane.
//! @param mesh the mesh to cross-section
//! @param slicing_plane the plane to cross-section with
//! @param shift the shift applied to the mesh before cross-sectioning (modified by function)
//! @param average_normal the average normal of all cross-sectioned faces (output)
//! @param sb the settings to use for cross-sectioning
//! @param preserve_input_shift if true, the incoming shift is used (not recomputed) so multiple meshes share the same
//! flattening transform when slicing with an angled plane.
//! @return the resulting cross-section polygons
PolygonList doCrossSection(QSharedPointer<MeshBase> mesh, Plane& slicing_plane, Point& shift, QVector3D& average_normal,
                           QSharedPointer<SettingsBase> sb, bool preserve_input_shift = false);

//! \brief finds the center of the slicing plane within the bounding box
//! \param mesh the mesh
//! \param slicing_plane the plane
//! \return the center
Point findSlicingPlaneMidPoint(QSharedPointer<MeshBase> mesh, Plane& slicing_plane);

//! \brief finds intersection of segment between two points and a plane
//! \param vertex0 the start point of the segment
//! \param vertex1 the end point of the segment
//! \param slicing_plane the plane
//! \return the intersection point
Point findIntersection(Point& vertex0, Point& vertex1, Plane& slicing_plane);

} // namespace CrossSection

} // namespace ORNL
