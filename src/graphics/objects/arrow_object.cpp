#include "graphics/objects/arrow_object.h"

#include <GL/gl.h>

#include <vector>

#include <qcolor.h>
#include <qmatrix4x4.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "graphics/graphics_object.h"
#include "graphics/support/shape_factory.h"

namespace ORNL {
ArrowObject::ArrowObject(BaseView* view, QVector3D begin, QVector3D end, QColor color) {
    m_begin = begin;
    m_end = end;
    m_color = color;

    this->initArrow(view);
}

ArrowObject::ArrowObject(BaseView* view, QSharedPointer<GraphicsObject> begin, QSharedPointer<GraphicsObject> end,
                         QColor color) {
    m_begin = begin->center();
    m_end = end->center();
    m_color = color;

    m_head_tracking = end;
    m_tail_tracking = begin;

    this->initArrow(view);
}

void ArrowObject::setBegin(QVector3D begin) {
    m_begin = begin;

    float len = m_begin.distanceToPoint(m_end);

    m_tail_vertices = {// Origin to len.
                       0, 0, 0, -len, 0, 0};

    this->updateVertices(m_tail_vertices);

    this->setTransformation(this->findTransform(m_begin, m_end));
}

void ArrowObject::setEnd(QVector3D end) {
    m_end = end;

    float len = m_begin.distanceToPoint(m_end);

    m_tail_vertices = {// Origin to len.
                       0, 0, 0, -len, 0, 0};

    this->updateVertices(m_tail_vertices);

    this->setTransformation(this->findTransform(m_begin, m_end));
}

void ArrowObject::updateEndpoints() {
    if (m_head_tracking.isNull() || m_tail_tracking.isNull())
        return;

    m_begin = m_tail_tracking->center();
    m_end = m_head_tracking->center();

    float len = m_begin.distanceToPoint(m_end);

    m_tail_vertices = {// Origin to len.
                       0, 0, 0, -len, 0, 0};

    this->updateVertices(m_tail_vertices);

    this->setTransformation(this->findTransform(m_begin, m_end));
}

void ArrowObject::initArrow(BaseView* view) {
    std::vector<float> vertices;
    std::vector<float> colors;

    float len = m_begin.distanceToPoint(m_end);

    m_tail_vertices = {// Origin to len.
                       0, 0, 0, -len, 0, 0};

    // Translate to end and rotate to match direction.
    /*
    QMatrix4x4 cone_tfm;
    cone_tfm.translate(m_end);

    QVector3D dir_vec = m_end - m_begin;
    QVector3D up_vec = QVector3D(0, 0, 1);
    QQuaternion rotation = QQuaternion::fromDirection(dir_vec, up_vec);
    cone_tfm.rotate(rotation);
    */

    QMatrix4x4 cone_tfm;
    cone_tfm.translate(-1, 0, 0);
    cone_tfm.rotate(90, 0, 1, 0);

    std::vector<float> tmp_norm;
    ShapeFactory::createCone(.3, 1, cone_tfm, QColor(), m_head_vertices, colors, tmp_norm);

    // Concat the two vectors and populate
    vertices = m_tail_vertices;
    vertices.insert(vertices.end(), m_head_vertices.begin(), m_head_vertices.end());

    int colorSize = vertices.size() / 3 * 4;
    colors.reserve(colorSize);
    for (int i = 0; i < colorSize; i += 4) {
        colors.push_back(m_color.redF());
        colors.push_back(m_color.greenF());
        colors.push_back(m_color.blueF());
        colors.push_back(m_color.alphaF());
    }

    tmp_norm.clear();
    this->populateGL(view, vertices, tmp_norm, colors, GL_LINES);

    this->setTransformation(this->findTransform(m_begin, m_end));
}

QMatrix4x4 ArrowObject::findTransform(QVector3D begin, QVector3D end) {
    QVector3D dir_vec = end - begin;
    QVector3D start_vec = QVector3D(begin.distanceToPoint(end), 0, 0);

    QQuaternion rotation = QQuaternion::rotationTo(start_vec, dir_vec);

    QMatrix4x4 tfm;
    tfm.translate(end);
    tfm.rotate(rotation);

    return tfm;
}

} // namespace ORNL
