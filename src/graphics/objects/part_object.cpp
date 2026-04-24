#include "graphics/objects/part_object.h"

#include <GL/gl.h>

#include <cmath>
#include <tuple>
#include <vector>

#include <qcolor.h>
#include <qhashfunctions.h>
#include <qquaternion.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "geometry/mesh/mesh_base.h"
#include "graphics/base_view.h"
#include "graphics/graphics_object.h"
#include "graphics/objects/arrow_object.h"
#include "graphics/objects/axes_object.h"
#include "graphics/objects/cube/plane_object.h"
#include "graphics/objects/text_object.h"
#include "part/part.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"

namespace ORNL {
PartObject::PartObject(BaseView* view, QSharedPointer<Part> p, ushort render_mode) {
    m_part = p;
    QSharedPointer<MeshBase> mesh = p->rootMesh();

    // Vertices
    float* vert_float;
    unsigned int vert_size;

    std::tie(vert_float, vert_size) = mesh->glVertexArray();
    std::vector<float> vertices = std::vector<float>(vert_float, vert_float + (vert_size / sizeof(float)));

    // Scale to OpenGL space
    for (int i = 0; i < vert_size / sizeof(float); i += 3) {
        vertices[i] = (vertices[i]) * Constants::OpenGL::kObjectToView;
        vertices[i + 1] = (vertices[i + 1]) * Constants::OpenGL::kObjectToView;
        vertices[i + 2] = (vertices[i + 2]) * Constants::OpenGL::kObjectToView;
    }

    // Normals
    float* normals_float;
    unsigned int normal_size;

    std::tie(normals_float, normal_size) = mesh->glNormalArray();
    std::vector<float> normals = std::vector<float>(normals_float, normals_float + (normal_size / sizeof(float)));

    // Colors
    // Setup colors based on mesh type
    switch (mesh->type()) {
        case (MeshType::kSupport):
        case (MeshType::kBuild):
            m_base_color = Constants::Colors::kLightBlue;
            m_selected_color = Constants::Colors::kGreen;
            break;
        case (MeshType::kClipping):
            m_base_color = Constants::Colors::kRed;
            m_selected_color = Constants::Colors::kOrange;
            break;
        case (MeshType::kSettings):
            m_base_color = Constants::Colors::kBlue;
            m_selected_color = Constants::Colors::kPurple;
            break;
    }

    m_color = m_base_color;

    int totalColors = 4 * (vertices.size() / 3);
    std::vector<float> colors;
    colors.resize(totalColors);
    for (int i = 0; i < totalColors; i += 4) {
        colors[i] = m_base_color.redF();
        colors[i + 1] = m_base_color.greenF();
        colors[i + 2] = m_base_color.blueF();
        colors[i + 3] = m_base_color.alphaF();
    }

    // Float buffers have been copied into std::vector by now.
    delete[] vert_float;
    delete[] normals_float;

    this->populateGL(view, vertices, normals, colors, render_mode);

    // Make a label.
    auto got = QSharedPointer<TextObject>::create(this->view(), m_part->name());
    got->setOnTop(true);
    got->hide();

    this->adoptChild(got);
    m_label_object = got;

    // Make axis.
    float length = this->maximum().x() - this->minimum().x();
    float width = this->maximum().y() - this->minimum().y();
    float depth = this->maximum().z() - this->minimum().z();
    auto goax = QSharedPointer<AxesObject>::create(this->view(), std::fmax(std::fmax(length, width), depth));
    goax->setOnTop(true);
    goax->hide();

    this->adoptChild(goax);
    m_axes_object = goax;
    m_axes_object->translateAbsolute(this->minimum());

    // Make slicing plane.
    float max_dim = std::fmax(length, std::fmax(width, depth));
    max_dim += max_dim * 0.20;
    auto gos = QSharedPointer<PlaneObject>::create(this->view(), max_dim, max_dim);
    gos->setLockedRotation(true);
    gos->hide();

    this->adoptChild(gos);
    m_plane_object = gos;

    // Object translation happens last since it will callback (see translationCallback()) this object and everything
    // needs to be setup first.
    QVector3D trans;
    QQuaternion rot;
    QVector3D scale;

    std::tie(trans, rot, scale) = MathUtils::decomposeTransformMatrix(mesh->transformation());

    trans *= Constants::OpenGL::kObjectToView;

    this->setTransformation(MathUtils::composeTransformMatrix(trans, rot, scale), true);
}

void PartObject::draw() {
    m_view->makeCurrent();
    if (getWireFrameMode()) {
        m_view->glDisable(GL_CULL_FACE);
        m_view->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        m_view->glDrawArrays(GL_TRIANGLES, 0, m_vertices.size() / 3);
        m_view->glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        m_view->glEnable(GL_CULL_FACE);
    }
    else {
        m_view->glDrawArrays(renderMode(), 0, m_vertices.size() / 3);
    }
}
void PartObject::configureUniforms() {
    view()->shaderProgram()->setUniformValue(m_shader_locs.renderingPartObject, true);
}

QSet<QSharedPointer<PartObject>> PartObject::select() {
    QSet<QSharedPointer<PartObject>> ret;

    // Cannot have both parent and this selected at the same time, so unselect it.
    QSharedPointer<PartObject> p = this->parent().dynamicCast<PartObject>();
    if (!p.isNull()) {
        while (!p.isNull()) {
            if (p->m_selected) {
                ret.insert(p);
                p->unselect();
            }

            p = p->parent().dynamicCast<PartObject>();
        }

        this->arrow()->show();

        m_axes_object->show();
    }

    m_color = m_selected_color;
    this->paint(m_color);

    // Select children but give them a different highlighting.
    for (auto& cp : m_part_children) {
        ret.unite(cp->select());
        cp->m_color = Constants::Colors::kYellow;
        cp->m_color.setAlpha(cp->transparency());
        cp->paint(cp->m_color);
        cp->m_selected = false;

        cp->arrow()->show();

        ret.insert(cp);
    }

    m_selected = true;

    return ret;
}

void PartObject::unselect() {
    m_color = m_base_color;
    this->paint(m_color);

    for (auto& cp : m_part_children) {
        cp->arrow()->hide();
        cp->unselect();
    }

    QSharedPointer<PartObject> p = this->parent().dynamicCast<PartObject>();
    if (!p.isNull()) {
        this->arrow()->hide();
        m_axes_object->show();
    }

    m_selected = false;
}

void PartObject::highlight() { this->paint(m_color.lighter()); }

void PartObject::unhighlight() { this->paint(m_color); }

void PartObject::setMeshTypeColor(MeshType type) {
    switch (type) {
        case (MeshType::kSupport):
        case (MeshType::kBuild):
            m_base_color = Constants::Colors::kLightBlue;
            m_selected_color = Constants::Colors::kGreen;
            break;
        case (MeshType::kClipping):
            m_base_color = Constants::Colors::kRed;
            m_selected_color = Constants::Colors::kOrange;
            break;
        case (MeshType::kSettings):
            m_base_color = Constants::Colors::kBlue;
            m_selected_color = Constants::Colors::kPurple;
            break;
    }

    m_color = (m_selected) ? m_selected_color : m_base_color;
    this->paint(m_color);
}

void PartObject::setRenderMode(ushort mode) {
    renderMode() = mode;
    this->setWireFrameMode(mode == GL_LINES);
    render();
}

void PartObject::setSolidWireFrameMode(bool state) { solidWireFrameMode() = state; }

QSharedPointer<ArrowObject> PartObject::arrow() { return m_arrow_object; }

QSharedPointer<TextObject> PartObject::label() { return m_label_object; }

QSharedPointer<AxesObject> PartObject::axes() { return m_axes_object; }

QSharedPointer<PlaneObject> PartObject::plane() { return m_plane_object; }

void PartObject::showOverhang(bool show) {
    m_overhang_shown = show;
    if (show)
        this->overhangUpdate();
    else
        this->paint(m_color);
}

void PartObject::setOverhangAngle(Angle a) {
    m_overhang_angle = a;
    if (m_overhang_shown)
        this->overhangUpdate();
    else
        this->paint(m_color);
}

QString PartObject::name() { return m_part->name(); }

QSharedPointer<Part> PartObject::part() { return m_part; }

void PartObject::overhangUpdate() {
    this->view()->shaderProgram()->bind();
    float overhang_angle = m_overhang_angle();
    // Pass the information angle and stacking axis to the shader so that it can determine what faces
    // need to be colored with the overhang color.
    this->view()->shaderProgram()->setUniformValue(m_shader_locs.overhangAngle, overhang_angle);
    this->view()->shaderProgram()->setUniformValue(m_shader_locs.stackingAxis, QVector3D(0, 0, 1));
    this->view()->shaderProgram()->setUniformValue(m_shader_locs.overhangMode, true);
    this->view()->shaderProgram()->release();
}

void PartObject::transformationCallback() {
    if (!m_arrow_object.isNull())
        m_arrow_object->updateEndpoints();
    for (auto& goc : m_part_children) {
        if (!goc->arrow().isNull())
            goc->arrow()->updateEndpoints();
    }

    QVector3D label_pos = this->center();
    label_pos.setZ(this->maximum().z() + 0.5);
    m_label_object->translateAbsolute(label_pos, false);

    m_plane_object->scaleAbsolute(QVector3D(this->scaling().x(), this->scaling().y(), 1));

    float length = this->minimumBoundingBox()[MBB_FLB].distanceToPoint(this->minimumBoundingBox()[MBB_FRB]);
    float width = this->minimumBoundingBox()[MBB_FLB].distanceToPoint(this->minimumBoundingBox()[MBB_BLB]);
    float depth = this->minimumBoundingBox()[MBB_FLT].distanceToPoint(this->minimumBoundingBox()[MBB_FLB]);
    m_axes_object->updateDimensions(std::fmax(std::fmax(length, width), depth));

    if (m_overhang_shown)
        this->overhangUpdate();
}

void PartObject::adoptChildCallback(QSharedPointer<GraphicsObject> child) {
    // We only care about new part objects.
    QSharedPointer<PartObject> goc = child.dynamicCast<PartObject>();
    if (goc.isNull())
        return;

    m_part_children.insert(goc);

    // Make an arrow. Currently, arrows are only between parts. If we want arrows between other
    // objects, it might be worth the time to move this to the graphics object for the general case.
    auto goa = QSharedPointer<ArrowObject>::create(this->view(), goc, this->sharedFromThis());
    goa->setOnTop(true);
    goa->hide();

    goc->m_arrow_object = goa;
    goc->adoptChild(goa);
}

void PartObject::orphanChildCallback(QSharedPointer<GraphicsObject> child) {
    // We only care about removed part objects.
    QSharedPointer<PartObject> goc = child.dynamicCast<PartObject>();
    if (goc.isNull())
        return;

    m_part_children.remove(goc);

    goc->m_arrow_object.reset();
}

void PartObject::paint(QColor color) {
    // Disable the overhang setting on the shader
    this->view()->shaderProgram()->bind();
    this->view()->shaderProgram()->setUniformValue(m_shader_locs.overhangMode, false);
    this->view()->shaderProgram()->release();
    color.setAlpha(m_transparency);

    this->GraphicsObject::paint(color);
    if (m_overhang_shown)
        this->overhangUpdate();
}
} // namespace ORNL
