#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <qlist.h>
#include <qmatrix4x4.h>
#include <qobject.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "graphics/graphics_object.h"
#include "graphics/support/camera_manager.h"

namespace ORNL {

class AxesObject;
class GCodeObject;

/*! \brief Base class for all OpenGL views in ORNLSlicer.
 *
 *  This base class is responsible only for the rendering of objects. Derived classes are responsible for the
 *  management and manipulation of parts and other objects. See PartView for an example of this.
 *
 *  Must inherit from both QOpenGLWidget and QOpenGLFunctions to have widget functionality and use the
 *  Qt OpenGL API.
 */
class BaseView : public QOpenGLWidget, public QOpenGLFunctions_3_3_Core {
    Q_OBJECT
  public:
    //! \brief Initialize printer OpenGL buffers, projection and view matrices, part picker, and shader program
    //! \param parent QWidget that has to be passed to superclass constructor
    BaseView(QWidget* parent = nullptr);

    //! \return The view's shader program
    QSharedPointer<QOpenGLShaderProgram> shaderProgram();
    //! \brief Makes this the current OpenGL context and deletes members as required.
    ~BaseView() override;

    /*!
     * \title Qt Overrides
     * \brief These functions in QOpenGLWidget have been overridden.
     */

    //! \brief Responds to mouse press events on the widget.
    //! \param e Qt event that has data about mouse press.
    void mousePressEvent(QMouseEvent* e) override;
    //! \brief Responds to mouse double click events on the widget.
    //! \param e Qt event that has data about mouse press.
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    //! \brief Responds to mouse release events on the widget.
    //! \param e Qt event that has data about mouse press.
    void mouseReleaseEvent(QMouseEvent* e) override;
    //! \brief Responds to mouse move events on the widget.
    //! \param e Qt event that has data about mouse move.
    void mouseMoveEvent(QMouseEvent* e) override;
    //! \brief Responds to mouse wheel events.
    //! \param e Qt event that has data about wheel movement.
    void wheelEvent(QWheelEvent* e) override;

    /*!
     * \title Interactivity - Camera
     * \brief The following functions relate to the manipulation of the camera.
     */

    //! \brief Moves the camera toward the print volume.
    void zoomIn();
    //! \brief Moves the camera away from the print volume.
    void zoomOut();
    //! \brief Moves the camera to its default zoom.
    virtual void resetZoom();
    //! \brief Moves the camera to its default zoom and orientation.
    virtual void resetCamera();

    //! \brief Set camera to view from top.
    void setTopView();
    //! \brief Set camera to view from side.
    void setSideView();
    //! \brief Set camera to view from front.
    void setFrontView();
    //! \brief Set camera to view from the forward direction.
    void setForwardView();
    //! \brief Set camera to view from an isometric direction.
    void setIsoView();

    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief Indicator to color certain objects based on current theme.
    QString m_theme;

    //! \return The camera object.
    QSharedPointer<CameraManager> camera();

  protected:
    /*!
     * \title Object Methods
     * \brief The following functions relate to the addition and remove of objects.
     */

    //! \brief Adds an object to the set of objects to render.
    //! \param object New object to add to render loop.
    //! \param layer Layer this object will be drawn on. Higher layers have higher priority.
    void addObject(QSharedPointer<GraphicsObject> object);

    //! \brief Removes an object from the render set.
    void removeObject(QSharedPointer<GraphicsObject> object);

    /*!
     * \title Child Accessor Methods
     * \brief Provides access to internals for sub classes.
     */

    //! \brief Set the projection matrix for the view. This is called in sub classes
    //!        where a different projection is required.
    void setProjectionMatrix(QMatrix4x4 projection);

    //! \return The view's projection matrix.
    QMatrix4x4 projectionMatrix();

    //! \return The view's view matrix.
    QMatrix4x4 viewMatrix();

    /*!
     * \title Virtual Methods - Render
     * \brief These methods are used by derived classes alter the render loop.
     */

    //! \brief Any one-time initialization that needs to be done for a subclass that isn't universal, otherwise
    //!        the initialization could be done in this class
    virtual void initView() = 0;

    /*!
     * \title Virtual Methods - Mouse
     * \brief Alter the behavior of the mouse in sub classes.
     */

    //! \brief Respond to a left click
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleLeftClick(QPointF mouse_ndc_pos) = 0;
    //! \brief Respond to a click of the scroll wheel
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleMidClick(QPointF mouse_ndc_pos);
    //! \brief Respond to a right click
    //!  \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    //!  \param local_pos The local mouse pos on the widget
    virtual void handleRightClick(QPointF mouse_ndc_pos, QPointF global_pos);
    //! \brief Respond to a left release
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleLeftRelease(QPointF mouse_ndc_pos);
    //! \brief Respond to a mid release
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleMidRelease(QPointF mouse_ndc_pos);
    //! \brief Respond to a right release
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleRightRelease(QPointF mouse_ndc_pos, QPointF global_pos);
    //! \brief Respond to a move of the mouse with the left button held down
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleLeftMove(QPointF mouse_ndc_pos);
    //! \brief Respond to a move of the mouse with the mid button held down
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleMidMove(QPointF mouse_ndc_pos);
    //! \brief Respond to a move of the mouse with the right button held down
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleRightMove(QPointF mouse_ndc_pos);
    //! \brief Respond to a move of the mouse with the right and left buttons held down
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleRightLeftMove(QPointF mouse_nds_pos);
    //! \brief Respond to a move of the mouse with the ctrl button and the right mouse button held down
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleControlModifiedRightMove(QPointF mouse_ndc_pos);
    //! \brief Responds to movement of the mouse with no buttons held down.
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleMouseMove(QPointF mouse_ndc_pos);
    //! \brief Responds to a double click with the left mouse button.
    //! \param mouse_ndc_pos Mouse position in Normalized Device Coordinates of window
    virtual void handleLeftDoubleClick(QPointF mouse_ndc_pos);
    //! \brief Respond to a mouse wheel forward movement
    virtual void handleWheelForward(QPointF mouse_ndc_pos, float delta);
    //! \brief Respond to a mouse wheel backward movement
    virtual void handleWheelBackward(QPointF mouse_ndc_pos, float delta);
    //! \brief Translates camera (and objects in sub views)
    virtual void translateCamera(QVector3D v, bool absolute);

    //! \brief Helper: Convert coordinates of point in widget to normalized device coordinates, i.e. x in [-1,1] and y
    //! in [-1,1] \param widget_pos Local widget position of an event
    QPointF normalizeWidgetPos(QPointF widget_pos);

    //! \brief Adjust projection matrix when widget/window is resized
    //! \param width New width of widget
    //! \param height New height of widget
    void resizeGL(int width, int height) override;

    //! \brief Object to handle camera manipulation.
    QSharedPointer<CameraManager> m_camera;

    //! \brief Shows camera focus.
    QSharedPointer<AxesObject> m_focus;

  private:
    //! \brief OpenGL setup.
    void initializeGL() override;

    //! \brief Draw everything to the screen. Recall this function with update().
    void paintGL() override;

    //! \brief GraphicsObject needs information from view to initalize/render.
    friend class GraphicsObject;
    friend class GCodeObject;

    //! \brief Shader file location for the first shader program.
    struct {
        int view;
        int camera_pos;
        int projection;
        int ambient_strength;
        int lighting_color;
        int lighting_pos;
        int using_solid_wireframe_mode;
        int overhang_angle;
        int using_overhang_mode;
        int rendering_part_object;
        int stack_axis;
    } m_shader_locs;

    //! \brief Projection matrix (world to clip coordinates/NDC coordinates)
    QMatrix4x4 m_projection;

    //! \brief OpenGL shader program to hold shader files and set up VAO.
    QSharedPointer<QOpenGLShaderProgram> m_shader_program;

    //! \brief Set of objects to render.
    QList<QSharedPointer<GraphicsObject>> m_render_objects;
};

} // namespace ORNL
