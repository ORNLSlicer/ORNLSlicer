#include "graphics/objects/printer/toroidal_printer_object.h"

#include <GL/gl.h>

#include <cstdlib>
#include <vector>

#include <qlist.h>
#include <qmath.h>
#include <qsharedpointer.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/segment_base.h"
#include "graphics/objects/axes_object.h"
#include "graphics/objects/cylinder/cylinder_plane_object.h"
#include "graphics/objects/part_object.h"
#include "graphics/objects/printer/printer_object.h"
#include "graphics/support/shape_factory.h"
#include "utilities/constants.h"
#include "utilities/mathutils.h"

namespace ORNL {

ToroidalPrinterObject::ToroidalPrinterObject(BaseView* view, QSharedPointer<SettingsBase> sb, bool is_true_volume)
    : PrinterObject(is_true_volume) {
    this->setSettings(sb);
    this->updateMembers();

    std::vector<float> vertices;
    std::vector<float> colors;

    ShapeFactory::createBuildVolumeToroidal(m_outer_radius, m_inner_radius, m_x_grid, m_y_grid, m_height,
                                            Constants::Colors::kBlack, vertices, colors);

    std::vector<float> tmp_norm;
    this->populateGL(view, vertices, tmp_norm, colors, GL_LINES);

    // Axes
    auto goax = QSharedPointer<AxesObject>::create(view, m_outer_radius * 0.4);
    goax->translate(this->minimum());

    // Floor plane
    //! \todo When shape factory supports inner radius, use the appropriate constructor for plane object.
    auto gopl =
        QSharedPointer<CylinderPlaneObject>::create(view, m_outer_radius, Constants::Colors::kBlue.lighter(160));
    gopl->translate(m_floor_center);
    gopl->setUnderneath(true);

    m_axes = goax;
    m_floor_plane = gopl;

    this->adoptChild(goax);
    this->adoptChild(gopl);

    // Seams
    this->createSeams();
    this->updateSeams();
}

QVector3D ToroidalPrinterObject::printerCenter() { return m_floor_center; }

QList<QSharedPointer<PartObject>> ToroidalPrinterObject::externalParts() {
    QList<QSharedPointer<PartObject>> ret;

    float printerMinZ = this->minimum().z();
    float printerMaxZ = this->maximum().z();

    float outerRadiusSquared = m_outer_radius * m_outer_radius;
    float innerRadiusSquared = m_inner_radius * m_inner_radius;
    QVector3D origin = this->printerCenter();

    for (auto& child : this->allChildren()) {
        auto gop = child.dynamicCast<PartObject>();
        if (gop.isNull())
            continue;

        for (auto& pt : gop->minimumBoundingBox()) {
            float dist = qPow(pt.x() - origin.x(), 2) + qPow(pt.y() - origin.y(), 2);
            float z = pt.z();

            if (dist > outerRadiusSquared || dist < innerRadiusSquared ||
                (!MathUtils::glEquals(z, printerMinZ) && z < printerMinZ) ||
                (!MathUtils::glEquals(z, printerMaxZ) && z > printerMaxZ)) {

                ret.append(gop);
                break;
            }
        }
    }

    return ret;
}

void ToroidalPrinterObject::updateMembers() {
    QSharedPointer<SettingsBase> sb = this->getSettings();

    m_x_grid = 0;
    m_y_grid = 0;

    if (sb->setting<bool>(PRS::Dimensions::kEnableGridX))
        m_x_grid = sb->setting<float>(PRS::Dimensions::kGridXDistance);

    if (sb->setting<bool>(PRS::Dimensions::kEnableGridY))
        m_y_grid = sb->setting<float>(PRS::Dimensions::kGridYDistance);

    m_outer_radius = sb->setting<float>(PRS::Dimensions::kOuterRadius);
    m_inner_radius = sb->setting<float>(PRS::Dimensions::kInnerRadius);

    QVector3D min(0.0, 0.0, sb->setting<float>(PRS::Dimensions::kZMin));
    QVector3D max(0.0, 0.0, sb->setting<float>(PRS::Dimensions::kZMax));

    // If this is not a true volume (i.e. in the part view) then we draw a box starting at (x_min, y_min, z_min) where
    // z_min > 0
    if (!isTrueVolume()) {
        // If z-min < 0 then we add the extra space to the top of the box
        if (min.z() < 0.0) {
            max.setZ(max.z() + qFabs(min.z()));
            min.setZ(0);
        }

        max.setZ(max.z() - min.z());

        min.setZ(0);
    }

    if (sb->setting<bool>(PRS::Dimensions::kEnableW)) {
        max.setZ(max.z() +
                 std::abs(sb->setting<float>(PRS::Dimensions::kWMax) - sb->setting<float>(PRS::Dimensions::kWMin)));
    }

    m_height = max.z() - min.z();

    m_outer_radius *= Constants::OpenGL::kObjectToView;
    m_inner_radius *= Constants::OpenGL::kObjectToView;
    m_height *= Constants::OpenGL::kObjectToView;
    m_x_grid *= Constants::OpenGL::kObjectToView;
    m_y_grid *= Constants::OpenGL::kObjectToView;

    m_floor_center = QVector3D(0.0f, 0.0f, 0.0f);

    m_printer_max_dims.setX(m_outer_radius);
    m_printer_max_dims.setY(m_outer_radius);
    m_printer_max_dims.setZ(m_height);
}

void ToroidalPrinterObject::updateGeometry() {
    std::vector<float> vertices;
    std::vector<float> colors;

    ShapeFactory::createBuildVolumeToroidal(m_outer_radius, m_inner_radius, m_x_grid, m_y_grid, m_height,
                                            Constants::Colors::kBlack, vertices, colors);

    this->replaceVertices(vertices);
    this->replaceColors(colors);

    m_axes->updateDimensions(m_outer_radius * 0.4);
    m_floor_plane->updateDimensions(m_outer_radius, m_inner_radius);

    m_axes->translateAbsolute(this->minimum());
    m_floor_plane->translateAbsolute(m_floor_center);
}
} // namespace ORNL
