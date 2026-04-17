
#include "graphics/base_view.h"

#include <GL/gl.h>

#include <QFileDialog>
#include <QMouseEvent>
#include <qmatrix4x4.h>
#include <qnamespace.h>
#include <qopenglshaderprogram.h>
#include <qopenglwidget.h>
#include <qpoint.h>
#include <qqueue.h>
#include <qsharedpointer.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "graphics/graphics_object.h"
#include "graphics/objects/axes_object.h"
#include "graphics/support/camera_manager.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"

namespace ORNL {
BaseView::BaseView(QWidget* parent) : QOpenGLWidget(parent) {
    // Initalize camera projection.
    float aspect;

    m_projection.setToIdentity();
    aspect = float(this->width()) / float(this->height());
    m_projection.perspective(Constants::OpenGL::kFov, aspect, Constants::OpenGL::kNearPlane,
                             Constants::OpenGL::kFarPlane);

    m_camera = QSharedPointer<CameraManager>::create(); // Manager for camera/view matrix
    m_shader_program.reset(new QOpenGLShaderProgram());
    this->setMouseTracking(true);
}

BaseView::~BaseView() {
    // Destructors need to call makeCurrent() according to
    // Qt5 QOpenGLWidget documentation page, section
    //"Resource Initialization and Cleanup"
    this->makeCurrent();

    // Release render-owned GL objects while the widget and its context are still valid.
    m_render_objects.clear();
    m_focus.reset();

    this->doneCurrent();
}

void BaseView::mousePressEvent(QMouseEvent* e) {
    this->setFocus(Qt::MouseFocusReason);

    QPointF ndc_mouse_pos = this->normalizeWidgetPos(e->localPos());
    switch (e->button()) {
        // We let derived classes handle clicks as they want this way
        case Qt::LeftButton:
            this->handleLeftClick(ndc_mouse_pos);
            break;
        case Qt::MiddleButton:
            this->handleMidClick(ndc_mouse_pos);
            break;
        case Qt::RightButton:
            this->handleRightClick(ndc_mouse_pos, e->globalPos());
            break;
        default:
            break;
    }
}

void BaseView::mouseDoubleClickEvent(QMouseEvent* e) {
    this->setFocus(Qt::MouseFocusReason);

    QPointF ndc_mouse_pos = this->normalizeWidgetPos(e->localPos());
    switch (e->button()) {
        case Qt::LeftButton:
            this->handleLeftDoubleClick(ndc_mouse_pos);
            break;
        default:
            break;
    }
}

void BaseView::mouseReleaseEvent(QMouseEvent* e) {
    this->setFocus(Qt::MouseFocusReason);

    QPointF ndc_mouse_pos = this->normalizeWidgetPos(e->localPos());
    switch (e->button()) {
        case Qt::LeftButton:
            this->handleLeftRelease(ndc_mouse_pos);
            break;
        case Qt::MiddleButton:
            this->handleMidRelease(ndc_mouse_pos);
            break;
        case Qt::RightButton:
            this->handleRightRelease(ndc_mouse_pos, e->globalPos());
            break;
        default:
            break;
    }
}

void BaseView::mouseMoveEvent(QMouseEvent* e) {
    this->setFocus(Qt::MouseFocusReason);

    QPointF ndc_mouse_pos = this->normalizeWidgetPos(e->localPos());
    switch (e->buttons()) {
        case (Qt::LeftButton): {
            this->handleLeftMove(ndc_mouse_pos);
            break;
        }

        case (Qt::RightButton): {
            if (e->modifiers() == Qt::ControlModifier) {
                this->handleControlModifiedRightMove(ndc_mouse_pos);
            }
            else {
                this->handleRightMove(ndc_mouse_pos);
            }
            break;
        }

        case (Qt::MiddleButton): {
            this->handleMidMove(ndc_mouse_pos);
            this->update();
            break;
        }

        case (Qt::LeftButton | Qt::RightButton): {
            this->handleRightLeftMove(ndc_mouse_pos);
            break;
        }

        default: {
            this->handleMouseMove(ndc_mouse_pos);
            break;
        }
    }
}

void BaseView::wheelEvent(QWheelEvent* e) {
    this->setFocus(Qt::MouseFocusReason);

    QPointF ndc_mouse_pos = this->normalizeWidgetPos(e->position());

    // Shouldn't be possible that delta is zero if wheel event triggered (macOS phases may cause issues?)
    if (e->angleDelta().y() > 0)
        this->handleWheelForward(ndc_mouse_pos, (float)e->angleDelta().y());
    else
        this->handleWheelBackward(ndc_mouse_pos, (float)e->angleDelta().y());
}

void BaseView::zoomIn() {
    m_camera->zoom(10);
    this->update(); // Need to repaint with new view matrix
}

void BaseView::zoomOut() {
    m_camera->zoom(-10);
    this->update(); // Need to repaint with new view matrix
}

void BaseView::resetZoom() {
    m_camera->zoom(200);
    m_camera->zoom(Constants::OpenGL::kZoomDefault + 1);
}

void BaseView::resetCamera() {
    // Reset rotation and zoom
    m_camera->reset();

    m_camera->panAbsolute(QVector3D(0, 0, 0));
    m_focus->scaleAbsolute(QVector3D(1, 1, 1));

    this->update(); // Need to repaint with new model matrices
}

void BaseView::setTopView() {
    this->resetCamera();
    m_camera->rotateAbsolute(QVector2D(0, 0));
    this->update();
}

void BaseView::setSideView() {
    this->resetCamera();
    m_camera->rotateAbsolute(QVector2D(90, -90));
    this->update();
}

void BaseView::setFrontView() {
    this->resetCamera();
    m_camera->rotateAbsolute(QVector2D(0, -90));
    this->update();
}

void BaseView::setForwardView() {
    this->resetCamera();
    m_camera->rotateAbsolute(QVector2D(0, -70));
    this->update();
}

void BaseView::setIsoView() {
    this->resetCamera();
    m_camera->rotateAbsolute(QVector2D(45, 45));
    this->handleWheelForward(QPointF(), 120 * 20);
}

void BaseView::addObject(QSharedPointer<GraphicsObject> object) { m_render_objects.append(object); }

void BaseView::removeObject(QSharedPointer<GraphicsObject> object) { m_render_objects.removeOne(object); }

void BaseView::setProjectionMatrix(QMatrix4x4 projection) { m_projection = projection; }

QSharedPointer<CameraManager> BaseView::camera() { return m_camera; }

QMatrix4x4 BaseView::projectionMatrix() { return m_projection; }

QMatrix4x4 BaseView::viewMatrix() { return m_camera->viewMatrix(); }

QSharedPointer<QOpenGLShaderProgram> BaseView::shaderProgram() { return m_shader_program; }

void BaseView::handleMidClick(QPointF mouse_ndc_pos) {
    m_camera->setDragStart(mouse_ndc_pos);
    m_focus->show();
    this->update();
}

void BaseView::handleRightClick(QPointF mouse_ndc_pos, QPointF global_pos) {
    m_camera->setDragStart(mouse_ndc_pos);
    m_focus->show();
    this->update();
}

void BaseView::handleLeftRelease(QPointF mouse_ndc_pos) {
    // Nothing by default.
}

void BaseView::handleMidRelease(QPointF mouse_ndc_pos) {
    m_focus->hide();
    this->update();
}

void BaseView::handleRightRelease(QPointF mouse_ndc_pos, QPointF global_pos) {
    m_focus->hide();
    this->update();
}

void BaseView::handleLeftMove(QPointF mouse_ndc_pos) {
    // Default to doing nothing
}

void BaseView::handleMidMove(QPointF mouse_ndc_pos) {
    QVector3D camera_trans = -1 * m_camera->translateFromPoint(mouse_ndc_pos);
    this->translateCamera(camera_trans, false);
}

void BaseView::handleRightMove(QPointF mouse_ndc_pos) {
    m_camera->rotateFromPoint(mouse_ndc_pos);

    this->update(); // Repaint because view matrix has changed
}

void BaseView::handleRightLeftMove(QPointF mouse_nds_pos) {
    // Nothing default.
}

void BaseView::handleControlModifiedRightMove(QPointF mouse_ndc_pos) {
    // Nothing by default.
}

void BaseView::handleMouseMove(QPointF mouse_ndc_pos) {
    // Nothing by default.
}

void BaseView::handleLeftDoubleClick(QPointF mouse_ndc_pos) {
    // Nothing by default.
}

void BaseView::handleWheelForward(QPointF mouse_ndc_pos, float delta) {
    m_camera->zoom(delta);
    float focus_scale = m_camera->getZoom() / m_camera->getDefaultZoom();
    m_focus->scaleAbsolute(QVector3D(focus_scale, focus_scale, focus_scale));
    this->update();
}

void BaseView::handleWheelBackward(QPointF mouse_ndc_pos, float delta) {
    m_camera->zoom(delta);
    float focus_scale = m_camera->getZoom() / m_camera->getDefaultZoom();
    m_focus->scaleAbsolute(QVector3D(focus_scale, focus_scale, focus_scale));
    this->update();
}

void BaseView::translateCamera(QVector3D v, bool absolute) {
    if (absolute) {
        m_camera->panAbsolute(v);
        m_focus->translateAbsolute(m_camera->getPan());
    }
    else {
        m_camera->pan(v);
        m_focus->translateAbsolute(m_camera->getPan());
    }
}

void BaseView::initializeGL() {
    // Required for OpenGL to work
    this->initializeOpenGLFunctions();

    this->setupStyle(); // color background

    this->glEnable(GL_CULL_FACE);  // Cull polygons based on winding
    this->glEnable(GL_DEPTH_TEST); // Depth comparisons so stuff behind other polygons not shown

    this->glEnable(GL_BLEND);
    this->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Compile shaders for the first shader program that is used for objects not in wireframe mode
    m_shader_program->addShaderFromSourceFile(QOpenGLShader::Vertex, Constants::OpenGL::Shader::kVertShaderFile);
    m_shader_program->addShaderFromSourceFile(QOpenGLShader::Fragment, Constants::OpenGL::Shader::kFragShaderFile);

    m_shader_program->link();
    m_shader_program->bind();

    m_shader_locs.projection = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kProjectionName);
    m_shader_locs.view = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kViewName);
    m_shader_locs.lighting_color = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kLightingColorName);
    m_shader_locs.lighting_pos = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kLightingPositionName);
    m_shader_locs.camera_pos = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kCameraPositionName);
    m_shader_locs.ambient_strength = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kAmbientStrengthName);
    m_shader_locs.using_solid_wireframe_mode =
        m_shader_program->uniformLocation(Constants::OpenGL::Shader::kUsingSolidWireframeModeName);
    m_shader_locs.stack_axis = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kStackingAxisName);
    m_shader_locs.overhang_angle = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kOverhangAngleName);
    m_shader_locs.using_overhang_mode = m_shader_program->uniformLocation(Constants::OpenGL::Shader::kOverhangModeName);
    m_shader_locs.rendering_part_object =
        m_shader_program->uniformLocation(Constants::OpenGL::Shader::kRenderingPartObjectName);
    m_shader_program->release();

    m_focus = QSharedPointer<AxesObject>::create(this, 5);
    m_focus->hide();
    m_focus->setOnTop(true);
    this->addObject(m_focus);

    // Hook here to init view-specific stuff (axes, view cube, etc)
    this->initView();
}

void BaseView::resizeGL(int width, int height) {
    // (Re)Initalize camera projection.
    float aspect;

    m_projection.setToIdentity();
    aspect = float(width) / float(height);
    m_projection.perspective(Constants::OpenGL::kFov, aspect, Constants::OpenGL::kNearPlane,
                             Constants::OpenGL::kFarPlane);

    this->update();
}

void BaseView::paintGL() {
    // Always clear color and depth buffer!
    this->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Always bind shader program
    m_shader_program->bind();

    // Projection and view matrices are same for every object we own, so set them now
    m_shader_program->setUniformValue(m_shader_locs.projection, m_projection);
    m_shader_program->setUniformValue(m_shader_locs.view, m_camera->viewMatrix());

    m_shader_program->setUniformValue(m_shader_locs.lighting_color, QVector3D(1.0f, 1.0f, 1.0f));
    m_shader_program->setUniformValue(m_shader_locs.lighting_pos, QVector3D(0, 0, 40));

    m_shader_program->setUniformValue(m_shader_locs.camera_pos, m_camera->cameraTranslation());
    m_shader_program->setUniformValue(m_shader_locs.ambient_strength, 0.4f);
    // We draw most objects in this depth range, leaving the front and the back for objects that need
    // to be drawn there.
    this->glDepthRange(0.01, 0.99);

    QQueue<QSharedPointer<GraphicsObject>> goQueue;
    // Add objects to the render queue first
    for (auto& go : m_render_objects) {
        go->addToRenderQueue(goQueue);
    }

    // Then render objects in queue
    while (!goQueue.isEmpty()) {
        QSharedPointer<GraphicsObject> go = goQueue.dequeue();
        go->render();
    }
    // Since we bound shader program, we must release
    m_shader_program->release();
}

void BaseView::setupStyle() {
    this->glClearColor(PreferencesManager::getInstance()->getTheme().getBgColor()[0],
                       PreferencesManager::getInstance()->getTheme().getBgColor()[1],
                       PreferencesManager::getInstance()->getTheme().getBgColor()[2],
                       PreferencesManager::getInstance()->getTheme().getBgColor()[3]);

    this->glClear(GL_COLOR_CLEAR_VALUE);
}

QPointF BaseView::normalizeWidgetPos(QPointF widget_pos) {
    QPointF normalized_device_pos;

    // Reverse sign needed for y because y is positive down from widget's point of view
    normalized_device_pos.setX(2.0 * widget_pos.x() / this->width() - 1.0);
    normalized_device_pos.setY(1.0 - 2.0 * widget_pos.y() / this->height());
    return normalized_device_pos;
}
} // namespace ORNL
