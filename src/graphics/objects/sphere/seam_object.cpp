#include "graphics/objects/sphere/seam_object.h"

#include <QtGui/qopengl.h>
#include <qcolor.h>

#include "graphics/graphics_object.h"
#include "graphics/objects/sphere_object.h"

namespace ORNL {
SeamObject::SeamObject(BaseView* view, QColor color) : SphereObject(view, .25, color, GL_TRIANGLES) {
    this->setOnTop(true);
}
}  // namespace ORNL
