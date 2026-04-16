#include "graphics/graphics_object.h"

#include "QOpenGLShaderProgram"
#include "QStack"
#include "graphics/base_view.h"
#include "graphics/objects/part_object.h"
#include "utilities/mathutils.h"

namespace ORNL {
GraphicsObject::GraphicsObject(BaseView* view, const std::vector<float>& vertices, const std::vector<float>& normals,
                               const std::vector<float>& colors, const ushort render_mode, const std::vector<float>& uv,
                               const QImage texture) {
    this->populateGL(view, vertices, normals, colors, render_mode, uv, texture);
}

GraphicsObject::~GraphicsObject() {
    if (m_view.isNull() || m_view->context() == nullptr)
        return;

    m_view->makeCurrent();

    m_vao->destroy();
    m_vbo.destroy();
    m_nbo.destroy();
    m_cbo.destroy();
    m_tbo.destroy();
    m_texture.reset();
}

void GraphicsObject::render() {

    if (m_state.hidden)
        return;
    m_view->shaderProgram()->setUniformValue(m_shader_locs.usingSolidWireframeMode, this->getSolidWireFrameMode());

    // Draw to front of screen if on top or underneath.
    if (m_state.ontop)
        m_view->glDepthRange(0.00, 0.01);
    else if (m_state.underneath)
        m_view->glDepthRange(0.99, 1.00);

    // Adjust rotation / scale if billboarding.
    if (m_state.billboard) {
        QVector3D view_translation;
        QQuaternion view_rotation;
        std::tie(view_translation, view_rotation, std::ignore) =
            MathUtils::decomposeTransformMatrix(m_view->viewMatrix());

        QVector3D scaling = QVector3D(1, 1, 1);
        scaling *= (view_translation.z() / Constants::OpenGL::kZoomDefault);

        this->rotateAbsolute(view_rotation.inverted(), false);
        this->scaleAbsolute(scaling, false);

        // Ambient strength is 100% for a billboard.
        m_view->shaderProgram()->setUniformValue(m_shader_locs.ambient, 1.0f);
    }

    // Note: if rendered outside of BaseView::paintGL(), the view must be made current.
    m_view->makeCurrent();

    // Actual render.
    m_view->shaderProgram()->setUniformValue(m_shader_locs.model, m_transform);
    this->configureUniforms();

    m_texture->bind();
    m_vao->bind();
    this->draw();
    m_vao->release();
    m_texture->release();
    // qDebug() << m_view->glGetError();

    // Restore global rendering values.
    if (m_state.ontop || m_state.underneath)
        m_view->glDepthRange(0.01, 0.99);
    if (m_state.billboard)
        m_view->shaderProgram()->setUniformValue(m_shader_locs.ambient, 0.4f);
}

void GraphicsObject::configureUniforms() {
    m_view->shaderProgram()->setUniformValue(m_shader_locs.renderingPartObject, false);
}

BaseView* GraphicsObject::view() { return m_view.data(); }

QVector<QVector3D> GraphicsObject::minimumBoundingBox() { return m_transformed_mbb; }

bool GraphicsObject::doesMBBIntersect(QSharedPointer<GraphicsObject> go) {
    QVector3D min = this->minimum();
    QVector3D max = this->maximum();

    QVector3D other_min = go->minimum();
    QVector3D other_max = go->maximum();

    if (max.x() >= other_min.x() && other_max.x() >= min.x() && max.y() >= other_min.y() && other_max.y() >= min.y() &&
        max.z() >= other_min.z() && other_max.z() >= min.z()) {
        return true;
    }

    return false;
}

QVector3D GraphicsObject::center() { return (m_transformed_mbb[MBB_MIN] + m_transformed_mbb[MBB_MAX]) / 2; }

QVector3D GraphicsObject::minimum() {
    QVector3D min;
    min.setX(Constants::Limits::Maximums::kMaxFloat);
    min.setY(Constants::Limits::Maximums::kMaxFloat);
    min.setZ(Constants::Limits::Maximums::kMaxFloat);

    for (QVector3D& v : m_transformed_mbb) {
        min.setX(std::fmin(min.x(), v.x()));
        min.setY(std::fmin(min.y(), v.y()));
        min.setZ(std::fmin(min.z(), v.z()));
    }

    return min;
}

QVector3D GraphicsObject::maximum() {
    QVector3D max;
    max.setX(Constants::Limits::Minimums::kMinFloat);
    max.setY(Constants::Limits::Minimums::kMinFloat);
    max.setZ(Constants::Limits::Minimums::kMinFloat);

    for (QVector3D& v : m_transformed_mbb) {
        max.setX(std::fmax(max.x(), v.x()));
        max.setY(std::fmax(max.y(), v.y()));
        max.setZ(std::fmax(max.z(), v.z()));
    }

    return max;
}

void GraphicsObject::setTransformation(QMatrix4x4 mtrx, bool propagate) {
    this->setTransformationInternal(mtrx, propagate);

    std::tie(m_translation, m_rotation, m_scale) = MathUtils::decomposeTransformMatrix(mtrx);
}

void GraphicsObject::setTransformationInternal(QMatrix4x4 mtrx, bool propagate) {
    if (propagate) {
        for (auto& c : this->children()) {
            // Get new child position and propagate.
            QMatrix4x4 child_tfm = c->transformation();
            QMatrix4x4 relative_tfm = m_transform.inverted() * child_tfm;
            QMatrix4x4 new_tfm = mtrx * relative_tfm;

            // Using external function to decompose here.
            c->setTransformation(new_tfm, true);
        }
    }

    m_transform = mtrx;

    // Update bounding cube.
    for (int i = 0; i < m_mbb.size(); i++) {
        m_transformed_mbb[i] = mtrx * m_mbb[i];
    }

    if (!m_state.in_callback) {
        m_state.in_callback = true;
        this->transformationCallback();
        m_state.in_callback = false;
    }
}

void GraphicsObject::translateAbsolute(QVector3D t, bool propagate) {
    m_translation = t;

    QMatrix4x4 new_tfm = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);
    this->setTransformationInternal(new_tfm, propagate);
}

void GraphicsObject::rotateAbsolute(QQuaternion r, bool propagate) {
    m_rotation = r;

    QMatrix4x4 new_tfm = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);
    this->setTransformationInternal(new_tfm, propagate);
}

void GraphicsObject::scaleAbsolute(QVector3D s, bool propagate) {
    m_scale = s;

    QMatrix4x4 new_tfm = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);
    this->setTransformationInternal(new_tfm, propagate);
}

void GraphicsObject::translate(QVector3D t, bool propagate) {
    m_translation += t;

    QMatrix4x4 new_tfm = m_transform;
    new_tfm.translate(t);

    this->setTransformationInternal(new_tfm, propagate);
}

void GraphicsObject::rotate(QQuaternion r, bool propagate) {
    // QVector3D er = r.toEulerAngles();
    // QVector3D mer = m_rotation.toEulerAngles();
    //
    // m_rotation = QQuaternion::fromEulerAngles(er + mer);

    m_rotation *= r;

    QMatrix4x4 new_tfm = m_transform;
    new_tfm.rotate(r);

    this->setTransformationInternal(new_tfm, propagate);
}

void GraphicsObject::scale(QVector3D s, bool propagate) {
    m_scale += s;

    QMatrix4x4 new_tfm = m_transform;
    new_tfm.scale(s);

    this->setTransformationInternal(new_tfm, propagate);
}

QMatrix4x4 GraphicsObject::transformation() { return m_transform; }

QVector3D GraphicsObject::translation() { return m_translation; }

QQuaternion GraphicsObject::rotation() { return m_rotation; }

QVector3D GraphicsObject::scaling() { return m_scale; }

std::vector<Triangle> GraphicsObject::triangles() {
    // 3 vertices specify a triangle, but each vertex refers to 3 floats
    std::vector<Triangle> triangles;
    Triangle current_triangle;
    for (int i = 0; i < m_vertices.size(); i += 9) {
        current_triangle.a = m_transform * QVector3D(m_vertices[i + 0], m_vertices[i + 1], m_vertices[i + 2]);

        current_triangle.b = m_transform * QVector3D(m_vertices[i + 3], m_vertices[i + 4], m_vertices[i + 5]);

        current_triangle.c = m_transform * QVector3D(m_vertices[i + 6], m_vertices[i + 7], m_vertices[i + 8]);

        triangles.push_back(current_triangle);
    }

    return triangles;
}

const std::vector<float>& GraphicsObject::vertices() { return m_vertices; }

const std::vector<float>& GraphicsObject::normals() { return m_normals; }

const std::vector<float>& GraphicsObject::colors() { return m_colors; }

QSharedPointer<GraphicsObject> GraphicsObject::parent() { return m_parent; }

QSet<QSharedPointer<GraphicsObject>> GraphicsObject::children() { return m_children; }

QSet<QSharedPointer<GraphicsObject>> GraphicsObject::allChildren() {
    QSet<QSharedPointer<GraphicsObject>> ret;

    QStack<QSharedPointer<GraphicsObject>> queue;
    for (auto& child : m_children) {
        queue.push(child);
    }

    while (!queue.empty()) {
        QSharedPointer<GraphicsObject> go = queue.pop();

        for (auto& go_child : go->children()) {
            queue.push(go_child);
        }

        ret.insert(go);
    }

    return ret;
}

void GraphicsObject::adoptChild(QSharedPointer<GraphicsObject> child) {
    child->m_parent = this->sharedFromThis();
    m_children.insert(child);

    this->adoptChildCallback(child);
}

void GraphicsObject::orphanChild(QSharedPointer<GraphicsObject> child) {
    if (child.isNull())
        return;
    child->m_parent = nullptr;
    m_children.remove(child);

    this->orphanChildCallback(child);
}

void GraphicsObject::lock() { m_state.locked = true; }

void GraphicsObject::unlock() { m_state.locked = false; }

void GraphicsObject::setLocked(bool lock) { m_state.locked = lock; }

bool GraphicsObject::locked() { return m_state.locked; }

void GraphicsObject::hide() { m_state.hidden = true; }

void GraphicsObject::show() { m_state.hidden = false; }

void GraphicsObject::setHidden(bool hide) { m_state.hidden = hide; }

bool GraphicsObject::hidden() { return m_state.hidden; }

void GraphicsObject::setBillboarding(bool state) { m_state.billboard = state; }

void GraphicsObject::setOnTop(bool state) {
    m_state.underneath = false;
    m_state.ontop = state;
}

void GraphicsObject::setUnderneath(bool state) {
    m_state.ontop = false;
    m_state.underneath = state;
}

GraphicsObject::GraphicsObject() {
    // NOP
}

void GraphicsObject::populateGL(BaseView* view, const std::vector<float>& vertices, const std::vector<float>& normals,
                                const std::vector<float>& colors, const ushort render_mode,
                                const std::vector<float>& uv, const QImage texture) {
    m_view = view;
    m_render_mode = render_mode;

    QSharedPointer<QOpenGLShaderProgram> program = m_view->shaderProgram();
    // Get shader locations.
    m_shader_locs.model = m_view->shaderProgram()->uniformLocation(Constants::OpenGL::Shader::kModelName);
    m_shader_locs.ambient = m_view->shaderProgram()->uniformLocation(Constants::OpenGL::Shader::kAmbientStrengthName);
    m_shader_locs.overhangAngle =
        m_view->shaderProgram()->uniformLocation(Constants::OpenGL::Shader::kOverhangAngleName);
    m_shader_locs.overhangMode = m_view->shaderProgram()->uniformLocation(Constants::OpenGL::Shader::kOverhangModeName);
    m_shader_locs.stackingAxis = m_view->shaderProgram()->uniformLocation(Constants::OpenGL::Shader::kStackingAxisName);
    m_shader_locs.renderingPartObject =
        m_view->shaderProgram()->uniformLocation(Constants::OpenGL::Shader::kRenderingPartObjectName);
    m_shader_locs.vertice = m_view->shaderProgram()->attributeLocation(Constants::OpenGL::Shader::kPositionName);
    m_shader_locs.normal = m_view->shaderProgram()->attributeLocation(Constants::OpenGL::Shader::kNormalName);
    m_shader_locs.color = m_view->shaderProgram()->attributeLocation(Constants::OpenGL::Shader::kColorName);
    m_shader_locs.uv = m_view->shaderProgram()->attributeLocation(Constants::OpenGL::Shader::kUVName);
    m_shader_locs.usingSolidWireframeMode =
        m_view->shaderProgram()->uniformLocation(Constants::OpenGL::Shader::kUsingSolidWireframeModeName);

    m_view->makeCurrent();

    m_view->shaderProgram()->bind();

    m_vertices = vertices;
    m_normals = normals;
    m_colors = colors;
    m_uv = uv;

    m_texture = QSharedPointer<QOpenGLTexture>::create(texture);
    m_texture->setMinificationFilter(QOpenGLTexture::Nearest);
    m_texture->setMagnificationFilter(QOpenGLTexture::Nearest);
    m_texture->setWrapMode(QOpenGLTexture::Repeat);

    m_vao.reset(new QOpenGLVertexArrayObject());
    m_vao->create();
    m_vao->bind();

    // Vertices
    m_vbo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_vbo.bind();
    m_vbo.allocate(m_vertices.data(), m_vertices.size() * sizeof(float));

    m_view->shaderProgram()->enableAttributeArray(m_shader_locs.vertice);
    m_view->shaderProgram()->setAttributeBuffer(m_shader_locs.vertice, GL_FLOAT, 0, 3);

    // Normals
    if (!m_normals.size()) {
        m_normals.resize(m_vertices.size(), 0);
    }
    m_nbo.create();
    m_nbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_nbo.bind();
    m_nbo.allocate(m_normals.data(), m_normals.size() * sizeof(float));
    m_view->shaderProgram()->enableAttributeArray(m_shader_locs.normal);
    m_view->shaderProgram()->setAttributeBuffer(m_shader_locs.normal, GL_FLOAT, 0, 3);

    // Colors
    if (!m_colors.size()) {
        m_normals.resize((m_vertices.size() / 3) * 4, 0);
    }
    m_cbo.create();
    m_cbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_cbo.bind();
    m_cbo.allocate(m_colors.data(), m_colors.size() * sizeof(float));

    m_view->shaderProgram()->enableAttributeArray(m_shader_locs.color);
    m_view->shaderProgram()->setAttributeBuffer(m_shader_locs.color, GL_FLOAT, 0, 4);

    // Texture
    if ((m_uv.size() / 2) < (m_vertices.size() / 3)) {
        // If mapping has less vertices specified than the passed vertex array,
        // map to 0,0 for the remaining vertices.
        int uv_float_count = (m_vertices.size() / 3) * 2;
        m_uv.resize(uv_float_count);
        for (int i = m_uv.size(); i < uv_float_count; i++) {
            m_uv[i] = 0.0f;
        }
    }

    m_tbo.create();
    m_tbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_tbo.bind();
    m_tbo.allocate(m_uv.data(), m_uv.size() * sizeof(float));

    m_view->shaderProgram()->enableAttributeArray(m_shader_locs.uv);
    m_view->shaderProgram()->setAttributeBuffer(m_shader_locs.uv, GL_FLOAT, 0, 2);

    // Release it
    m_vao->release();
    m_vbo.release();
    m_nbo.release();
    m_cbo.release();
    m_tbo.release();

    m_view->shaderProgram()->release();

    // Create bounding box.
    QVector3D min, max;

    min.setX(Constants::Limits::Maximums::kMaxFloat);
    min.setY(Constants::Limits::Maximums::kMaxFloat);
    min.setZ(Constants::Limits::Maximums::kMaxFloat);
    max.setX(Constants::Limits::Minimums::kMinFloat);
    max.setY(Constants::Limits::Minimums::kMinFloat);
    max.setZ(Constants::Limits::Minimums::kMinFloat);

    for (int i = 0; i < vertices.size(); i += 3) {
        min.setX(std::min(vertices[i + 0], min.x()));
        min.setY(std::min(vertices[i + 1], min.y()));
        min.setZ(std::min(vertices[i + 2], min.z()));
        max.setX(std::max(vertices[i + 0], max.x()));
        max.setY(std::max(vertices[i + 1], max.y()));
        max.setZ(std::max(vertices[i + 2], max.z()));
    }

    m_mbb.reserve(8);
    m_mbb.append(QVector3D(min.x(), min.y(), min.z()));
    m_mbb.append(QVector3D(min.x(), max.y(), min.z()));
    m_mbb.append(QVector3D(max.x(), max.y(), min.z()));
    m_mbb.append(QVector3D(max.x(), min.y(), min.z()));
    m_mbb.append(QVector3D(min.x(), min.y(), max.z()));
    m_mbb.append(QVector3D(min.x(), max.y(), max.z()));
    m_mbb.append(QVector3D(max.x(), max.y(), max.z()));
    m_mbb.append(QVector3D(max.x(), min.y(), max.z()));

    m_transformed_mbb = m_mbb;

    std::tie(m_translation, m_rotation, m_scale) = MathUtils::decomposeTransformMatrix(m_transform);
}

void GraphicsObject::draw() { m_view->glDrawArrays(renderMode(), 0, m_vertices.size() / 3); }

bool GraphicsObject::getSolidWireFrameMode() { return m_state.is_solid_wireframe; }

void GraphicsObject::setSolidWireFrameMode(bool state) { m_state.is_solid_wireframe = state; }

bool GraphicsObject::getWireFrameMode() { return m_state.is_wireframe; }

void GraphicsObject::setWireFrameMode(bool state) { m_state.is_wireframe = state; }

void GraphicsObject::setTransparency(uint trans) {
    m_transparency = trans;
    m_base_color.setAlpha(trans);
    m_selected_color.setAlpha(trans);

    m_color = (m_selected) ? m_selected_color : m_base_color;

    this->paint(m_color);
}

uint GraphicsObject::transparency() { return m_transparency; }

void GraphicsObject::replaceVertices(std::vector<float>& vertices) {
    m_vertices = vertices;

    m_vbo.bind();
    m_vbo.allocate(m_vertices.data(), m_vertices.size() * sizeof(float));
    m_vbo.release();

    // Update bounding box.
    QVector3D min, max;

    min.setX(Constants::Limits::Maximums::kMaxFloat);
    min.setY(Constants::Limits::Maximums::kMaxFloat);
    min.setZ(Constants::Limits::Maximums::kMaxFloat);
    max.setX(Constants::Limits::Minimums::kMinFloat);
    max.setY(Constants::Limits::Minimums::kMinFloat);
    max.setZ(Constants::Limits::Minimums::kMinFloat);

    for (int i = 0; i < vertices.size(); i += 3) {
        min.setX(std::min(vertices[i + 0], min.x()));
        min.setY(std::min(vertices[i + 1], min.y()));
        min.setZ(std::min(vertices[i + 2], min.z()));
        max.setX(std::max(vertices[i + 0], max.x()));
        max.setY(std::max(vertices[i + 1], max.y()));
        max.setZ(std::max(vertices[i + 2], max.z()));
    }

    m_mbb.clear();
    m_mbb.reserve(8);
    m_mbb.append(QVector3D(min.x(), min.y(), min.z()));
    m_mbb.append(QVector3D(min.x(), max.y(), min.z()));
    m_mbb.append(QVector3D(max.x(), max.y(), min.z()));
    m_mbb.append(QVector3D(max.x(), min.y(), min.z()));
    m_mbb.append(QVector3D(min.x(), min.y(), max.z()));
    m_mbb.append(QVector3D(min.x(), max.y(), max.z()));
    m_mbb.append(QVector3D(max.x(), max.y(), max.z()));
    m_mbb.append(QVector3D(max.x(), min.y(), max.z()));

    m_transformed_mbb = m_mbb;
}

void GraphicsObject::replaceNormals(std::vector<float>& normals) {
    m_normals = normals;

    m_nbo.bind();
    m_nbo.allocate(m_normals.data(), m_normals.size() * sizeof(float));
    m_nbo.release();
}

void GraphicsObject::replaceColors(std::vector<float>& colors) {
    m_colors = colors;

    m_cbo.bind();
    m_cbo.allocate(m_colors.data(), m_colors.size() * sizeof(float));
    m_cbo.release();
}

void GraphicsObject::replaceUV(std::vector<float>& uv) {
    m_uv = uv;

    if ((m_uv.size() / 2) < (m_vertices.size() / 3)) {
        // If mapping has less vertices specified than the passed vertex array,
        // map to 0,0 for the remaining vertices.
        int uv_float_count = (m_vertices.size() / 3) * 2;
        m_uv.reserve(uv_float_count);
        for (int i = m_uv.size(); i < uv_float_count; i++) {
            m_uv.push_back(0.0f);
            m_uv.push_back(0.0f);
        }
    }

    m_tbo.bind();
    m_tbo.allocate(m_uv.data(), m_uv.size() * sizeof(float));
    m_tbo.release();
}

void GraphicsObject::replaceTexture(QImage texture) {
    m_texture.reset();
    m_texture = QSharedPointer<QOpenGLTexture>::create(texture);
    m_texture->setMinificationFilter(QOpenGLTexture::Nearest);
    m_texture->setMagnificationFilter(QOpenGLTexture::Nearest);
    m_texture->setWrapMode(QOpenGLTexture::Repeat);
}

void GraphicsObject::updateVertices(std::vector<float>& vertices, uint whence) {
    m_vbo.bind();

    // If the new vertices run off the end of the vector, resize and update buffer.
    uint end_count = vertices.size() + whence;
    if (end_count > m_vertices.size()) {
        m_vertices.resize(end_count);

        m_vbo.allocate(end_count);
        m_vbo.write(0, m_vertices.data(), (whence + 1) * sizeof(float));
    }

    memcpy(m_vertices.data() + whence, vertices.data(), vertices.size() * sizeof(float));

    m_vbo.write(whence * sizeof(float), m_vertices.data() + whence, vertices.size() * sizeof(float));
    m_vbo.release();
}

void GraphicsObject::updateNormals(std::vector<float>& normals, uint whence) {
    m_nbo.bind();

    // If the new normals run off the end of the vector, resize and update buffer.
    uint end_count = normals.size() + whence;
    if (end_count > m_normals.size()) {
        m_normals.resize(end_count);

        m_nbo.allocate(end_count);
        m_nbo.write(0, m_normals.data(), (whence + 1) * sizeof(float));
    }

    memcpy(m_normals.data() + whence, normals.data(), normals.size() * sizeof(float));

    m_nbo.write(whence * sizeof(float), m_normals.data() + whence, normals.size() * sizeof(float));
    m_nbo.release();
}

void GraphicsObject::updateColors(std::vector<float>& colors, uint whence) {
    m_cbo.bind();

    // If the new colors run off the end of the vector, resize and update buffer.
    uint end_count = colors.size() + whence;
    if (end_count > m_colors.size()) {
        m_colors.resize(end_count);

        m_cbo.allocate(end_count);
        m_cbo.write(0, m_colors.data(), (whence + 1) * sizeof(float));
    }

    memcpy(m_colors.data() + whence, colors.data(), colors.size() * sizeof(float));

    m_cbo.write(whence * sizeof(float), m_colors.data() + whence, colors.size() * sizeof(float));
    m_cbo.release();
}

void GraphicsObject::transformationCallback() {
    // Does nothing by default.
}

void GraphicsObject::adoptChildCallback(QSharedPointer<GraphicsObject> child) {
    // Does nothing by default.
}

void GraphicsObject::orphanChildCallback(QSharedPointer<GraphicsObject> child) {
    // Does nothing by default.
}

QSharedPointer<QOpenGLTexture>& GraphicsObject::texture() { return m_texture; }

QSharedPointer<QOpenGLVertexArrayObject>& GraphicsObject::vao() { return m_vao; }

ushort& GraphicsObject::renderMode() { return m_render_mode; }

bool& GraphicsObject::solidWireFrameMode() { return m_state.is_solid_wireframe; }

bool& GraphicsObject::wireFrameMode() { return m_state.is_wireframe; }

void GraphicsObject::addToRenderQueue(QQueue<QSharedPointer<GraphicsObject>>& renderQueue) {

    // First add each of this objects children to the render queue
    for (auto& c : m_children) {
        c->addToRenderQueue(renderQueue);
    }

    QSharedPointer<PartObject> p = this->sharedFromThis().dynamicCast<PartObject>();
    // If this graphics object is a part and its transparent, put it to the back of the render queue,
    // otherwise, put it to the front.
    if (p && p->transparency() < 255) {
        renderQueue.append(this->sharedFromThis());
    }
    else {
        renderQueue.prepend(this->sharedFromThis());
    }
}

void GraphicsObject::paint(QColor color) { this->paint(color, 0); }

void GraphicsObject::paint(QColor color, uint whence, long count) {
    // TODO: move this to shaders.
    uint stop = (count == -1) ? m_colors.size() : whence + count;

    for (int i = whence; i < stop; i += 4) {
        m_colors[i] = color.redF();
        m_colors[i + 1] = color.greenF();
        m_colors[i + 2] = color.blueF();
        m_colors[i + 3] = color.alphaF();
    }

    // Update buffer.
    m_cbo.bind();
    m_cbo.write(0, m_colors.data(), m_colors.size() * sizeof(float));
    m_cbo.release();
}

} // Namespace ORNL
