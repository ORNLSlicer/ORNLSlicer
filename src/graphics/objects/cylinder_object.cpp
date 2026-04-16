#include "graphics/objects/cylinder_object.h"

#include <vector>

#include <qcolor.h>
#include <qmatrix4x4.h>

#include "graphics/graphics_object.h"
#include "graphics/support/shape_factory.h"

namespace ORNL {

CylinderObject::CylinderObject(BaseView* view, float radius, float height, QColor color)
    : CylinderObject(view, radius, 0, height, color) {
    // NOP
}

CylinderObject::CylinderObject(BaseView* view, float outer_radius, float inner_radius, float height, QColor color) {
    m_outer_radius = outer_radius;
    m_inner_radius = inner_radius;
    m_height = height;

    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> colors;

    ShapeFactory::createCylinder(m_outer_radius, m_height, QMatrix4x4(), color, vertices, colors, normals);

    this->populateGL(view, vertices, normals, colors);
}
} // namespace ORNL
