#pragma once

#include "graphics/graphics_object.h"

namespace ORNL {
/*!
 * \brief Simple cube object. Supports textures.
 *
 * This class is a base object type that can be used as primative for other objects.
 *
 * \note
 *  QVector<QImage> text = {
 *      QImage(":/icons/ornlslicer_logo.png"),
 *      QImage(":/icons/3d_chart.png"),
 *      QImage(":/icons/3d_graph.png"),
 *      QImage(":/icons/arrow.png"),
 *      QImage(":/icons/box.png"),
 *      QImage(":/icons/brush.png")
 *  };
 *
 *  auto goc = QSharedPointer<CubeObject>::create(this, 20, 10, 7, Constants::Colors::kWhite, GL_TRIANGLES, text);
 */
class CubeObject : public GraphicsObject {
  public:
    /*!
     * \brief Constructor for object.
     * \param view: View this object is drawn on.
     * \param length: Length of the cube.
     * \param width: Width of the cube.
     * \param height: Height of the cube.
     * \param color: Color of the cube.
     * \param render_mode: How OpenGL should render this cube. Use GL_TRIANGLES, GL_LINES, etc.
     * \param textures: Vector of images to use for faces. If the size of the vector is less than
     *        the number of faces (6 faces), the remaining faces will be the color of the object.
     *        The order of faces is: Bottom, Top, Left, Front, Right, Back
     */
    CubeObject(BaseView* view, float length, float width, float height, QColor color, ushort render_mode = GL_TRIANGLES,
               const QVector<QImage>& textures = QVector<QImage>());

  protected:
    //! \brief Dims
    float m_length;
    float m_width;
    float m_height;

    //! \brief Cube texture.
    QVector<QImage> m_textures;
};
} // namespace ORNL
