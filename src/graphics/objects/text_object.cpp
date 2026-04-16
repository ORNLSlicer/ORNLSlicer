#include "graphics/objects/text_object.h"

#include <GL/gl.h>

#include <vector>

#include <QPainter>
#include <qhashfunctions.h>
#include <qimage.h>
#include <qmatrix4x4.h>
#include <qstringview.h>
#include <qtypes.h>

#include "graphics/graphics_object.h"
#include "graphics/support/shape_factory.h"
#include "utilities/constants.h"

namespace ORNL {
TextObject::TextObject(ORNL::BaseView* view, QString str, float scale, bool billboarding) {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<float> uv;

    m_str = str;

    // Assemble texture.
    QByteArray s = str.toUtf8();

    // TODO: probably use better texture.
    QImage bitmap_font(":/textures/gl_font.png");
    QImage result(8 * str.length(), 8, QImage::Format_Mono);

    QPainter painter(&result);

    for (int i = 0; i < str.length(); i++) {
        uchar c = s[i];
        // Index into image. Font is in ASCII order currently. Might be reasonable to extend in the future.
        c -= ' ';
        QImage bitmap_char = bitmap_font.copy(c * 8, 0, 8, 8);

        painter.drawImage(i * 8, 0, bitmap_char);
    }

    painter.end();
    // result.invertPixels();

    ShapeFactory::createRectangle(scale * str.length(), scale, 0.1, QMatrix4x4(), Constants::Colors::kWhite, vertices,
                                  colors, normals);

    uv = {// Bottom face
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

          // Top face
          0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0};

    this->populateGL(view, vertices, normals, colors, GL_TRIANGLES, uv, result);

    this->setBillboarding(billboarding);
}

void TextObject::setText(QString str) {
    m_str = str;

    // Assemble texture.
    QByteArray s = str.toUtf8();

    // TODO: probably use better texture.
    QImage bitmap_font(":/textures/gl_font.png");
    QImage result(8 * str.length(), 8, QImage::Format_Mono);

    QPainter painter(&result);

    for (int i = 0; i < str.length(); i++) {
        uchar c = s[i];
        // Index into image. Font is in ASCII order currently. Might be reasonable to extend in the future.
        c -= ' ';
        QImage bitmap_char = bitmap_font.copy(c * 8, 0, 8, 8);

        painter.drawImage(i * 8, 0, bitmap_char);
    }

    painter.end();

    this->replaceTexture(result);
}
} // namespace ORNL
