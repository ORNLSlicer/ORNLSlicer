#pragma once

#include "QMatrix4x4"
#include "QOpenGLBuffer"
#include "QOpenGLTexture"
#include "QOpenGLVertexArrayObject"
#include "QPointer"
#include "QQueue"

// Forward
class QOpenGLShaderProgram;

// Two definitions - index of the bounding box min and max.
#define MBB_MIN 0
#define MBB_MAX 6

// Indexs of the rest of the bounding box.
// Names follow the format: [Front, Back][Right, Left][Bottom, Top]
#define MBB_FRB 0
#define MBB_BRB 1
#define MBB_FLB 2
#define MBB_BLB 3
#define MBB_FRT 4
#define MBB_BRT 5
#define MBB_FLT 6
#define MBB_BLT 7

namespace ORNL {
// Forward
class BaseView;

// Pod struct for graphics.
struct Triangle {
    QVector3D a;
    QVector3D b;
    QVector3D c;

    // Allows access to struct members with index.
    inline QVector3D& operator[](uint index) { return *((QVector3D*)(this) + index); }
};

/*!
 * \brief Underlying representation of any drawn 3D object. Base class for a number of
 *        different graphics types.
 *
 * All objects that are rendered are contained within a GraphicsObject. These objects
 * maintain a list of verticies and normals, a transformation, and the various OpenGL
 * buffers. Information reguarding the object's significance must be kept externally
 * (for instance, in a view or a derived object).
 *
 * Note that all graphics objects have their own set of buffers. In OpenGL, switching
 * buffers is one of the more expensive operations. While this is fine
 * for most cases in ORNLSlicer, having more than a few hundred objects shown at once
 * can start to incur slowdown. In these cases, it is recommended to use the GraphicsObject
 * as a sort of container object. See GCodeObject for an example of this.
 *
 * Additionally, there is a parenting scheme between objects. And transformations to a
 * parent object will by default be propagated to the child object relative to the parent.
 * As an example, consider the rotation of a parent with a child. The child must be rotated
 * and translated to allow for this motion.
 */
class GraphicsObject : public QEnableSharedFromThis<GraphicsObject> {
  public:
    //! \brief Construct object from GL data.
    GraphicsObject(BaseView* view, const std::vector<float>& vertices, const std::vector<float>& normals,
                   const std::vector<float>& colors, const ushort render_mode = GL_TRIANGLES,
                   const std::vector<float>& uv = std::vector<float>(),
                   const QImage texture = QImage(":/textures/blank_texture.png"));

    //! \brief Destructor.
    ~GraphicsObject();

    //! \brief Perform render. Renders to view passed to constructor.
    void render();

    //! \brief Allows GraphicsObject and objects that inherit from GraphicsObject to modify
    //! uniforms in their own way
    virtual void configureUniforms();

    //! \brief Get the view this object renders to.
    BaseView* view();

    //! \brief The bounding box for this object.
    QVector<QVector3D> minimumBoundingBox();
    //! \brief Checks if another object's bounding box intersects with this one.
    bool doesMBBIntersect(QSharedPointer<GraphicsObject> go);

    //! \brief Returns the center of this object.
    QVector3D center();
    //! \brief Returns the minimum point on the bounding box.
    QVector3D minimum();
    //! \brief Returns the maximum point on the bounding box.
    QVector3D maximum();

    //! \brief Set the global transformation.
    void setTransformation(QMatrix4x4 mtrx, bool propagate = true);

    //! \brief Translate the object to a location.
    void translateAbsolute(QVector3D t, bool propagate = true);
    //! \brief Rotate the object to an absolute rotation.
    void rotateAbsolute(QQuaternion r, bool propagate = true);
    //! \brief Scale the object from to an absolute size.
    void scaleAbsolute(QVector3D s, bool propagate = true);

    //! \brief Translate the object from the current position.
    void translate(QVector3D t, bool propagate = true);
    //! \brief Rotate the object from the current position.
    void rotate(QQuaternion r, bool propagate = true);
    //! \brief Scale the object from the current size.
    void scale(QVector3D s, bool propagate = true);

    //! \brief Get the global transformation.
    QMatrix4x4 transformation();

    //! \brief Get the translation of this object.
    QVector3D translation();
    //! \brief Get the rotation of this object.
    QQuaternion rotation();
    //! \brief Get the scaling of this object.
    QVector3D scaling();

    //! \brief Get the triangles that compose this part.
    //! \note Triangles are used for detecting collisions with rays. The function is
    //!       virtual so that sub classes can implement different collision meshes than
    //!       the visual (see GridObject for an example of this).
    virtual std::vector<Triangle> triangles();

    //! \brief Get the vertices of this object.
    const std::vector<float>& vertices();
    //! \brief Get the normals of this object.
    const std::vector<float>& normals();
    //! \brief Get the colors of this object.
    const std::vector<float>& colors();

    //! \brief Get the parent of this object.
    QSharedPointer<GraphicsObject> parent();
    //! \brief Get the children of this object.
    QSet<QSharedPointer<GraphicsObject>> children();

    //! \brief The set of all children for this object, including sub children.
    QSet<QSharedPointer<GraphicsObject>> allChildren();

    //! \brief Makes the calling object the parent of the parameter object.
    //! \param child Object to make into child.
    void adoptChild(QSharedPointer<GraphicsObject> child);
    //! \brief Remove child from this objects children.
    //! \param child Child to remove.
    void orphanChild(QSharedPointer<GraphicsObject> child);

    //! \brief Prevents the object from being selected.
    void lock();
    //! \brief Permits the object to be selected.
    void unlock();
    //! \brief Set the lock state.
    void setLocked(bool lock);
    //! \brief Get the lock state.
    bool locked();

    //! \brief Hides this object.
    void hide();
    //! \brief Shows this object.
    void show();
    //! \brief Sets the hidden state.
    void setHidden(bool hide);
    //! \brief Get if this object is hidden.
    bool hidden();

    //! \brief Set billboarding.
    void setBillboarding(bool state);
    //! \brief Set if this object should be rendered on top of the rest of the geometry.
    void setOnTop(bool state);
    //! \brief Set if this object should be rendered undeneath of the rest of the geometry.
    void setUnderneath(bool state);
    //! \brief Adds this graphic object to the render queue. Must be done here
    //! since graphics objects are accessed recursively.
    void addToRenderQueue(QQueue<QSharedPointer<GraphicsObject>>& goQueue);

    //! \brief Get the solid wireframe mode
    bool getSolidWireFrameMode();
    //! \brief Set the solid wireframe mode
    void setSolidWireFrameMode(bool state);

    //! \brief Get the wireframe mode
    bool getWireFrameMode();
    //! \brief Set the wireframe mode
    void setWireFrameMode(bool state);

    //! \brief Sets object transparency.
    //! \param trans: Value 0 - 255
    void setTransparency(uint trans);
    //! \brief Gets object transparency.
    uint transparency();

  protected:
    //! \brief Empty constructor. Only for derived classes.
    GraphicsObject();

    //! \brief Constructs OpenGL buffers from float and texture data.
    void populateGL(BaseView* view, const std::vector<float>& vertices, const std::vector<float>& normals,
                    const std::vector<float>& colors, const ushort render_mode = GL_TRIANGLES,
                    const std::vector<float>& uv = std::vector<float>(),
                    const QImage texture = QImage(":/textures/blank_texture.png"));

    //! \brief The actual call to OpenGL to draw the object.
    //! \note This function is virtual to permit objects to render as they see fit. The default proceedure
    //!       fits most use cases.
    virtual void draw();

    //! \brief Replace the vertices buffer with new float data.
    void replaceVertices(std::vector<float>& vertices);
    //! \brief Replace the normals buffer with new float data.
    void replaceNormals(std::vector<float>& normals);
    //! \brief Replace the colors buffer with new float data.
    void replaceColors(std::vector<float>& colors);
    //! \brief Replace the uv buffer with new float data.
    void replaceUV(std::vector<float>& uv);
    //! \brief Replace the texture buffer with new data.
    void replaceTexture(QImage texture);

    //! \brief Updates vertex buffer with new float data.
    void updateVertices(std::vector<float>& vertices, uint whence = 0);
    //! \brief Updates normals buffer with new float data.
    void updateNormals(std::vector<float>& normals, uint whence = 0);
    //! \brief Updates colors buffer with new float data.
    void updateColors(std::vector<float>& colors, uint whence = 0);

    //! \brief Paints entire object a single color.
    virtual void paint(QColor color);
    //! \brief Paints a subset of the object a single color.
    virtual void paint(QColor color, uint whence, long count = -1);

    //! \brief Callback for tranformation. Lets derived classes alter other objects when transformed.
    virtual void transformationCallback();
    //! \brief Callback for child addition.
    virtual void adoptChildCallback(QSharedPointer<GraphicsObject> child);
    //! \brief Callback for child removal.
    virtual void orphanChildCallback(QSharedPointer<GraphicsObject> child);

    //! \brief Get the texture buffer.
    QSharedPointer<QOpenGLTexture>& texture();
    //! \brief Get the vertex array object.
    QSharedPointer<QOpenGLVertexArrayObject>& vao();
    //! \brief Get the render mode.
    ushort& renderMode();
    //! \brief Get the solid wireframe mode
    bool& solidWireFrameMode();

    //! \brief Get the wireframe mode
    bool& wireFrameMode();

    //! \brief Locations in shader code.
    struct {
        int model;
        int ambient;
        int vertice;
        int normal;
        int color;
        int uv;
        int usingSolidWireframeMode;
        int overhangAngle;
        int overhangMode;
        int stackingAxis;
        int renderingPartObject;
    } m_shader_locs;

    //! \brief Current state for the object.
    struct {
        //! \brief If this object (and its children) hidden.
        bool hidden = false;
        //! \brief If this object can be selected.
        bool locked = false;
        //! \brief If this object will be rendered on top of other objects.
        bool ontop = false;
        //! \brief If this object will be rendered underneath of other objects.
        bool underneath = false;
        //! \brief If this object should billboard to the camera.
        bool billboard = false;

        //! \brief If the object is currently in a translation callback. Prevents endless recursion.
        bool in_callback = false;

        // If the object is currently rendering in wireFrame mode
        bool is_wireframe = false;

        // If the object is currently rendering in wireFrame mode
        bool is_solid_wireframe = false;

    } m_state;

    //! \brief View that we draw to. Guarded so shutdown can null this before lingering shared refs release.
    QPointer<BaseView> m_view;

    //! \brief GL mesh information.
    std::vector<float> m_vertices;
    std::vector<float> m_normals;
    std::vector<float> m_colors;
    std::vector<float> m_uv;

    //! \brief Colors.
    QColor m_selected_color;
    QColor m_base_color;
    QColor m_color;

    //! \brief Transparency.
    uint m_transparency = 255;

    //! \brief If this object is selected or not.
    bool m_selected = false;

  private:
    //! \brief Set the global transformation. This is an internal function to enforce updating of
    //!        transform components.
    void setTransformationInternal(QMatrix4x4 mtrx, bool propagate = true);

    //! \brief OpenGL buffers.
    QSharedPointer<QOpenGLVertexArrayObject> m_vao;
    QOpenGLBuffer m_vbo;
    QOpenGLBuffer m_cbo;
    QOpenGLBuffer m_nbo;
    QOpenGLBuffer m_tbo;

    //! \brief Texture object.
    QSharedPointer<QOpenGLTexture> m_texture;

    //! \brief Mode of render. Use values like GL_TRIANGLES, GL_LINES, etc.
    ushort m_render_mode;

    //! \brief Parent of this object.
    QSharedPointer<GraphicsObject> m_parent = nullptr;
    //! \brief List of children for this object.
    QSet<QSharedPointer<GraphicsObject>> m_children;

    //! \brief Components of transform. This is done to allow faster
    //! and easier matrix construction.
    QVector3D m_translation;
    QQuaternion m_rotation;
    QVector3D m_scale;

    //! \brief Transformation relative to world.
    QMatrix4x4 m_transform;

    //! \brief Bounding cube for object.
    QVector<QVector3D> m_mbb;
    //! \brief Bounding cube after transformation.
    QVector<QVector3D> m_transformed_mbb;
};

} // Namespace ORNL
