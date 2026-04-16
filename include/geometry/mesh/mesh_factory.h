#pragma once

#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/open_mesh.h"
#include "units/unit.h"

namespace ORNL {
/*!
 * \brief Static class the provides various functions to generate meshes
 */
class MeshFactory {
  public:
    //! \brief Constructor
    MeshFactory();

    //! \brief Builds and returns a mesh that is a rectangular prism.
    //! \param length: the length of the prism
    //! \param width: the width of the prism
    //! \param height: the height of the prism
    //! \return a new mesh
    static ClosedMesh CreateBoxMesh(const Distance& length, const Distance& width, const Distance& height);

    //! \brief Builds and returns a mesh that is a rectangular prism. Does not have top/bottom faces.
    //! \param length: the length of the prism
    //! \param width: the width of the prism
    //! \param height: the height of the prism
    //! \return a new mesh
    static OpenMesh CreateOpenTopBoxMesh(const Distance& length, const Distance& width, const Distance& height);

    //! \brief Builds and returns a mesh that is a triangle pyramid.
    //! \param length the side length
    //! \returna new mesh
    static ClosedMesh CreateTriaglePyramidMesh(const Distance& length);

    //! \brief Builds and returns a mesh that is a cylinder
    //! \param radius the raduis of the cylinder
    //! \param height the height of the cylinder
    //! \param resolution the resolution (defaults to 50 segments)
    //! \return a new mesh
    static ClosedMesh CreateCylinderMesh(const Distance& radius, const Distance& height, const int resolution = 50);

    //! \brief Builds and returns a mesh that is a cone
    //! \param radius the raduis of the cone
    //! \param height the height of the cone
    //! \param resolution the resolution (defaults to 50 segments)
    //! \return a new mesh
    static ClosedMesh CreateConeMesh(const Distance& radius, const Distance& height, const int resolution = 50);
};
} // namespace ORNL
