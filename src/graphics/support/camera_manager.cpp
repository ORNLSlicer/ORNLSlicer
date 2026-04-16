#include "graphics/support/camera_manager.h"

#include <cmath>

#include <QtMath>
#include <qmatrix4x4.h>
#include <qpoint.h>
#include <qvectornd.h>

#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "utilities/mathutils.h"

namespace ORNL {
CameraManager::CameraManager() { this->reset(); }

void CameraManager::reset() {
    this->rotateAbsolute(QVector2D(0, -70));
    m_pan = QVector3D(0, 0, 0);
    m_zoom = m_default_zoom;
    m_wheel_pos = 0.0;
    updateViewMatrix();
}

void CameraManager::setDragStart(QPointF ndc_pos) { m_drag_start = ndc_pos; }

void CameraManager::rotate(const QVector2D dr) {
    m_pitch += -90 + dr.x();
    m_yaw += dr.y();

    this->updateViewMatrix();
}

void CameraManager::rotateAbsolute(const QVector2D r) {
    m_pitch = -90 + r.x();
    m_yaw = r.y();

    this->updateViewMatrix();
}

void CameraManager::zoom(float delta) {
    constexpr float kZoomBase = 1.01;
    delta /= 8.75;
    m_wheel_pos += delta;
    double y_offset = log10(m_default_zoom) / log10(kZoomBase);
    m_zoom = powf(kZoomBase, (-(m_wheel_pos - y_offset)));

    this->updateViewMatrix();
}

void CameraManager::rotateFromPoint(QPointF ndc_pos) {
    QPointF delta = ndc_pos - m_drag_start;

    int invert = (PreferencesManager::getInstance()->invertCamera()) ? 1 : -1;

    m_pitch += delta.x() * Constants::OpenGL::kTrackball * invert;
    m_yaw += delta.y() * Constants::OpenGL::kTrackball * invert;

    this->updateViewMatrix();

    this->setDragStart(ndc_pos);
}

QVector3D CameraManager::translateFromPoint(QPointF ndc_pos) {
    // Use view matrix to get camera's up and right vectors, so that we can
    // translate in the direction of those vectors.
    QVector3D right = QVector3D(m_view(0, 0), m_view(0, 1), m_view(0, 2));
    QVector3D up = QVector3D(m_view(1, 0), m_view(1, 1), m_view(1, 2));

    // Project onto the XY plane and normalize.
    right.setZ(0);
    up.setZ(0);
    right.normalize();
    up.normalize();

    float delta_x = ndc_pos.x() - m_drag_start.x();
    float delta_y = ndc_pos.y() - m_drag_start.y();

    float translation_speed = 20 * (m_zoom / m_default_zoom);

    this->setDragStart(ndc_pos);
    QVector3D translation = (-translation_speed * delta_x * right) - (translation_speed * delta_y * up);

    return translation;
}

void CameraManager::setViewMatrix(QMatrix4x4 mtrx) { m_view = mtrx; }

const QMatrix4x4& CameraManager::viewMatrix() { return m_view; }

const QVector3D CameraManager::cameraTranslation() { return m_camera_translation + m_pan; }

void CameraManager::pan(QVector3D dt) {
    m_pan -= dt;
    updateViewMatrix();
}

void CameraManager::panAbsolute(QVector3D pos) {
    m_pan = pos;
    updateViewMatrix();
}

QVector3D CameraManager::getPan() { return m_pan; }

float CameraManager::getZoom() { return m_zoom; }

void CameraManager::setDefaultZoom(double value) {
    m_default_zoom = value;
    m_max_zoom = m_default_zoom * 1.5;
}

float CameraManager::getDefaultZoom() { return m_default_zoom; }

void CameraManager::updateViewMatrix() {
    // Limit as approaching -180, being collinear with up vector causes render failure.
    m_yaw = MathUtils::clamp(-179.9999, m_yaw, -5);

    m_camera_translation = MathUtils::sphericalToCartesian(m_zoom, m_pitch, -m_yaw);
    m_camera_translation += m_pan;
    m_view.setToIdentity();
    m_view.lookAt(m_camera_translation, m_pan, QVector3D(0, 0, 1));
}

} // namespace ORNL
