#include "graphics/objects/cylinder/cylinder_plane_object.h"

#include <qcolor.h>
#include <qvectornd.h>

#include "graphics/graphics_object.h"
#include "graphics/objects/cylinder_object.h"

namespace ORNL {
CylinderPlaneObject::CylinderPlaneObject(BaseView* view, float radius, QColor color)
    : CylinderPlaneObject(view, radius, 0, color) {
    m_starting_outer_radius = radius;
    m_color = color;
}

CylinderPlaneObject::CylinderPlaneObject(ORNL::BaseView* view, float outer_radius, float inner_radius, QColor color)
    : CylinderObject(view, outer_radius, inner_radius, 0.1, color) {
    m_starting_outer_radius = outer_radius;
    m_color = color;
}

void CylinderPlaneObject::updateDimensions(float outer_radius, float inner_radius) {
    this->scaleAbsolute(QVector3D(outer_radius / m_starting_outer_radius, outer_radius / m_starting_outer_radius, 1));
}
} // namespace ORNL
