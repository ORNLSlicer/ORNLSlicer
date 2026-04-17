#include "graphics/objects/cube_object.h"

#include <vector>

#include <QPainter>
#include <qcolor.h>
#include <qcontainerfwd.h>
#include <qimage.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "graphics/graphics_object.h"
#include "graphics/support/shape_factory.h"

namespace ORNL {
CubeObject::CubeObject(BaseView* view, float length, float width, float height, QColor color, ushort render_mode,
                       const QVector<QImage>& textures) {
    m_length = length;
    m_width = width;
    m_height = height;

    m_textures = textures;

    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<float> uv;

    // Fill out rest of textures with blank textures.
    for (int i = m_textures.size(); i < 6; i++) {
        m_textures.append(QImage(":/textures/blank_texture.png"));
    }

    // Face pixel size - should be square.
    uint fps = 0;
    for (QImage& texture : m_textures) {
        if (texture.size().width() > fps)
            fps = texture.size().width();
        if (texture.size().height() > fps)
            fps = texture.size().height();
    }

    // Stitch together textures for a cube map.
    QVector<QRect> cubemap = {QRect(fps, 2 * fps, fps, fps), QRect(fps, 0, fps, fps),
                              QRect(0, fps, fps, fps),       QRect(fps, fps, fps, fps),
                              QRect(2 * fps, fps, fps, fps), QRect(3 * fps, fps, fps, fps)};
    QImage result(fps * 4, fps * 3, QImage::Format_ARGB32);
    result.fill(color);

    QPainter painter(&result);
    for (int i = 0; i < m_textures.size(); i++) {
        painter.drawImage(cubemap[i], m_textures[i].scaled(fps, fps).mirrored(true, true));
    }

    painter.end();

    // Map cube
    uv = {// Bottom face
          0.25, 1, 0.50, 1, 0.25, 0.666, 0.50, 1, 0.50, 0.666, 0.25, 0.666,

          // Top face
          0.25, 0.333, 0.50, 0.333, 0.25, 0, 0.50, 0.333, 0.50, 0, 0.25, 0,

          // Left
          0, 0.666, 0.25, 0.666, 0, 0.333, 0.25, 0.666, 0.25, 0.333, 0, 0.333,

          // Front
          0.25, 0.666, 0.50, 0.666, 0.25, 0.333, 0.50, 0.666, 0.50, 0.333, 0.25, 0.333,

          // Right
          0.50, 0.666, 0.75, 0.666, 0.50, 0.333, 0.75, 0.666, 0.75, 0.333, 0.50, 0.333,

          // Back
          0.75, 0.666, 1, 0.666, 0.75, 0.333, 1, 0.666, 1, 0.333, 0.75, 0.333};

    ShapeFactory::createRectangle(m_length, m_width, m_height, QMatrix4x4(), color, vertices, colors, normals);

    this->populateGL(view, vertices, normals, colors, render_mode, uv, result);
}

} // namespace ORNL
