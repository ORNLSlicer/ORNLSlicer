#pragma once

#include <GL/gl.h>

#include <QColor>
#include <qhashfunctions.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <qtypes.h>

#include "graphics/graphics_object.h"
#include "units/unit.h"
#include "utilities/enums.h"

namespace ORNL {
// Forward
class Part;
class ArrowObject;
class AxesObject;
class PlaneObject;
class TextObject;

/*!
 * \brief Displays a part in an OpenGL view.
 *
 * This is one of the most complicated GraphicsObject classes due to the number of sub-objects
 * each part has. These include arrows between the parent/child, a text label, axes for rotation,
 * and a slicing plane. Coloring is also more complicated. This including overhangs, highlighting,
 * mesh types, and transparency.
 */
class PartObject : public GraphicsObject {
  public:
    //! \brief Constructor.
    //! \param view: View to render object to.
    //! \param p: Part to extract geometry from.
    //! \param render_mode: OpenGL render mode. Use GL_TRIANGLES, GL_LINES, etc.
    PartObject(BaseView* view, QSharedPointer<Part> p, ushort render_mode = GL_TRIANGLES);

    //! \brief Modfies uniforms to specific values for PartObjects
    void configureUniforms() override;
    //! \brief Selects this object and unselects any parents or children.
    //! \return The parts that have been unselected.
    QSet<QSharedPointer<PartObject>> select();
    void unselect();

    //! \brief Highlights the part (makes it lighter).
    void highlight();
    //! \brief Restores normal color after highlighting.
    void unhighlight();

    //! \brief Sets mesh color based on type.
    //! \param type: Mesh type.
    void setMeshTypeColor(MeshType type);

    //! \brief sets the render mode to use
    //! \param mode the mode
    void setRenderMode(ushort mode);

    //! \brief sets the shader to use (m_shader_program vs m_shader_program2)
    void setSolidWireFrameMode(bool state);

    //! \brief Gets the arrow object.
    QSharedPointer<ArrowObject> arrow();
    //! \brief Gets the label object.
    QSharedPointer<TextObject> label();
    //! \brief Gets the axes object.
    QSharedPointer<AxesObject> axes();
    //! \brief Gets the plane object.
    QSharedPointer<PlaneObject> plane();

    //! \brief Sets if overhangs are shown.
    void showOverhang(bool show);
    //! \brief Sets the overhang angle for calculations.
    void setOverhangAngle(Angle a);

    //! \brief Gets the part name.
    QString name();
    //! \brief Gets the mesh part pointer.
    QSharedPointer<Part> part();

  protected:
    void draw() override;
    //! \brief Updates the overhang coloring. Called after rotations/translations.
    void overhangUpdate();

    //! \brief Updates children objects upon transformation.
    void transformationCallback() override;

    //! \brief Handles arrow creation.
    void adoptChildCallback(QSharedPointer<GraphicsObject> child) override;
    //! \brief Handles arrow removal.
    void orphanChildCallback(QSharedPointer<GraphicsObject> child) override;

    //! \brief Paints a color while taking transparency/overhangs into account.
    void paint(QColor color) override;

  private:
    //! \brief Part pointer.
    QSharedPointer<Part> m_part;

    //! \brief Children graphic parts.
    QSet<QSharedPointer<PartObject>> m_part_children;

    //! \brief Sub-objects.
    QSharedPointer<ArrowObject> m_arrow_object;
    QSharedPointer<TextObject> m_label_object;
    QSharedPointer<AxesObject> m_axes_object;
    QSharedPointer<PlaneObject> m_plane_object;

    //! \brief Overhang angle to use for calculations.
    Angle m_overhang_angle;
    //! \brief If overhang angles are shown or not.
    bool m_overhang_shown = false;
};
} // Namespace ORNL
