#pragma once

#include <QMatrix4x4>
#include <qpoint.h>
#include <qvectornd.h>

#include "utilities/constants.h"

namespace ORNL {
/*!
 * \class CameraManager
 * \brief Manages the camera for an OpenGL widget
 */
class CameraManager {
  public:
    //! \brief Constructor
    CameraManager();

    //! \brief Resets camera zoom and rotation
    void reset();

    //! \brief Set the start of the drag.
    void setDragStart(QPointF ndc_pos);

    //! \brief Rotate the camera. Horizontal pitch and vertical yaw.
    void rotate(const QVector2D dr);
    //! \brief Set the rotation. Horizontal pitch and vertical yaw.
    void rotateAbsolute(const QVector2D r);

    //! \brief Update camera position.
    void zoom(float delta);

    /*! Rotate the camera between the point given
     * @param ndc_pos Position of mouse in NDC
     */
    void rotateFromPoint(QPointF ndc_pos);

    /*! Translate the camera from the point given
     * @param ndc_pos Position of mouse in NDC
     * @return Translation that just occured.
     */
    QVector3D translateFromPoint(QPointF ndc_pos);

    //! \brief The view matrix
    void setViewMatrix(QMatrix4x4 mtrx);

    //! \brief return the matrix of the world from the camera's perspective
    const QMatrix4x4& viewMatrix();

    //! \brief The camera's location in the world space relative to the center of the view.
    const QVector3D cameraTranslation();

    //! \brief pans the camera
    //! \param dt the distance to pan
    void pan(QVector3D dt);

    //! \brief pans the camera to a certain place
    //! \param pos the position of pan
    void panAbsolute(QVector3D pos);

    //! \brief gets the current pan
    //! \return the pan
    QVector3D getPan();

    //! \brief gets the current zoom level
    //! \return the zoom level
    float getZoom();

    //! \brief sets the default zoom level
    //! \param value the level
    void setDefaultZoom(double value);

    //! \brief gets the default zoom level
    //! \return the default zoom
    float getDefaultZoom();

  private:
    //! \brief Update the view matrix after new data is available.
    void updateViewMatrix();

    //! \brief Camera position.
    QVector3D m_camera_translation;

    QVector3D m_pan;

    float m_default_zoom = Constants::OpenGL::kZoomDefault;
    float m_max_zoom = Constants::OpenGL::kZoomMax;

    float m_pitch = 0;
    float m_yaw = 0;

    QPointF m_drag_start;

    double m_wheel_pos = 0.0;

    //! \brief Current camera zoom.
    float m_zoom = Constants::OpenGL::kZoomDefault;

    //! \brief View matrix that represents the translation and rotation of the camera
    QMatrix4x4 m_view;

}; // class CameraManager

} // namespace ORNL
