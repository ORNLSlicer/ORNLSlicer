#include "widgets/part_widget/model/part_meta_item.h"

#include <GL/gl.h>

#include <tuple>

#include <qlist.h>
#include <qobject.h>
#include <qquaternion.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {

PartMetaItem::PartMetaItem(QSharedPointer<Part> p) {
    m_part = p;

    m_selected = false;
    m_transparency = 255;

    m_type = m_part->rootMesh()->type();

    auto unit = m_part->rootMesh()->unit().toString();
    if (unit == Constants::Units::kMm)
        m_scale_unit_index = 0;
    else if (unit == Constants::Units::kCm)
        m_scale_unit_index = 1;
    else if (unit == Constants::Units::kM)
        m_scale_unit_index = 2;
    else if (unit == Constants::Units::kInch)
        m_scale_unit_index = 3;
    else if (unit == Constants::Units::kFeet)
        m_scale_unit_index = 4;
    else // If it is any other unit default to mm
        m_scale_unit_index = 0;

    m_transformation = m_part->rootMesh()->transformation();

    QVector3D orginal_translation, original_scale;
    QQuaternion orginal_rotation;
    std::tie(orginal_translation, orginal_rotation, original_scale) =
        MathUtils::decomposeTransformMatrix(m_transformation);
    orginal_translation *= Constants::OpenGL::kObjectToView;
    m_original_transformation =
        MathUtils::composeTransformMatrix(orginal_translation, orginal_rotation, original_scale);
    m_aligned_transformation = m_original_transformation;

    std::tie(m_translation, m_rotation, m_scale) = MathUtils::decomposeTransformMatrix(m_transformation);

    emit modified(PartMetaUpdateType::kAddUpdate);
}

void PartMetaItem::replaceInModel(QString filename) { m_model->replaceItem(this->sharedFromThis(), filename); }

void PartMetaItem::reloadInModel() { m_model->reloadItem(this->sharedFromThis()); }

void PartMetaItem::removeFromModel() { m_model->removeItem(this->sharedFromThis()); }

void PartMetaItem::setSelected(bool toggle) {
    m_selected = toggle;
    emit modified(PartMetaUpdateType::kSelectionUpdate);
}

bool PartMetaItem::isSelected() { return m_selected; }

void PartMetaItem::setMeshType(MeshType mt) {
    m_part->setMeshType(mt);
    m_type = mt;

    emit modified(PartMetaUpdateType::kVisualUpdate);
}

MeshType PartMetaItem::meshType() { return m_type; }

void PartMetaItem::setTransparency(uint val) {
    m_transparency = val;
    emit modified(PartMetaUpdateType::kVisualUpdate);
}

uint PartMetaItem::transparency() { return m_transparency; }

void PartMetaItem::setWireframe(bool show) {
    m_render_mode = (show) ? GL_LINES : GL_TRIANGLES;
    m_wireframe_mode = show;
    emit modified(PartMetaUpdateType::kVisualUpdate);
}

void PartMetaItem::setSolidWireframe(bool show) {
    m_solid_wireframe_mode = show;

    emit modified(PartMetaUpdateType::kVisualUpdate);
}

bool PartMetaItem::wireframeMode() { return m_wireframe_mode; }

ushort PartMetaItem::renderMode() { return m_render_mode; }

bool PartMetaItem::solidWireframeMode() { return m_solid_wireframe_mode; }

void PartMetaItem::setTranslation(QVector3D t) {
    m_translation = t;
    m_transformation = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);

    emit modified(PartMetaUpdateType::kTransformUpdate);
}

QVector3D PartMetaItem::translation() { return m_translation; }

void PartMetaItem::setRotation(QQuaternion r, bool current_rotation) {
    if (m_rotation == r)
        return;

    if (current_rotation) {
        m_rotation = r;
        m_transformation = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);
        emit modified(PartMetaUpdateType::kTransformUpdate);
        return;
    }

    QVector3D translation = m_translation;
    setTranslation(QVector3D(0, 0, 0));

    m_rotation = r;
    m_transformation = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);
    emit modified(PartMetaUpdateType::kTransformUpdate);

    m_scale = QVector3D(1, 1, 1);
    m_rotation = QQuaternion(1, 0, 0, 0);
    m_translation = translation;
    m_transformation = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);
    m_part->rootMesh()->alignAxis(m_transformation);

    m_graphics_part->setTransformation(m_transformation);
    emit modified(PartMetaUpdateType::kReloadUpdate);
}

QQuaternion PartMetaItem::rotation() { return m_rotation; }

void PartMetaItem::setScale(QVector3D s) {
    m_scale = s;
    m_transformation = MathUtils::composeTransformMatrix(m_translation, m_rotation, m_scale);

    emit modified(PartMetaUpdateType::kTransformUpdate);
}

QVector3D PartMetaItem::scale() { return m_scale; }

void PartMetaItem::setTransformation(QMatrix4x4 m) {
    m_transformation = m;
    std::tie(m_translation, m_rotation, m_scale) = MathUtils::decomposeTransformMatrix(m_transformation);

    emit modified(PartMetaUpdateType::kTransformUpdate);
}

QMatrix4x4 PartMetaItem::transformation() { return m_transformation; }

void PartMetaItem::resetTransformation() {
    m_transformation = m_original_transformation;
    std::tie(m_translation, m_rotation, m_scale) = MathUtils::decomposeTransformMatrix(m_transformation);

    m_part->rootMesh()->resetAlignedAxis(m_transformation);
    m_graphics_part->setTransformation(m_transformation);

    emit modified(PartMetaUpdateType::kReloadUpdate);
}

void PartMetaItem::setOriginalTransformation(QMatrix4x4 m) { m_aligned_transformation = m; }

void PartMetaItem::adoptChild(QSharedPointer<PartMetaItem> c) {
    m_children.append(c);
    c->setParent(this->sharedFromThis());

    emit modified(PartMetaUpdateType::kParentingUpdate);
}

QList<QSharedPointer<PartMetaItem>> PartMetaItem::children() { return m_children; }

void PartMetaItem::orphanChild(QSharedPointer<PartMetaItem> c) {
    m_children.removeOne(c);
    c->setParent(nullptr);

    emit modified(PartMetaUpdateType::kParentingUpdate);
}

void PartMetaItem::setParent(QSharedPointer<PartMetaItem> p) {
    m_parent = p;

    emit modified(PartMetaUpdateType::kParentingUpdate);
}

QSharedPointer<PartMetaItem> PartMetaItem::parent() { return m_parent; }

void PartMetaItem::setGraphicsPart(QSharedPointer<PartObject> gop) { m_graphics_part = gop; }

QSharedPointer<PartObject> PartMetaItem::graphicsPart() { return m_graphics_part; }

void PartMetaItem::setScaleUnitIndex(uint idx) {
    m_scale_unit_index = idx;

    emit modified(PartMetaUpdateType::kTransformUpdate);
}

uint PartMetaItem::scaleUnitIndex() { return m_scale_unit_index; }

QSharedPointer<Part> PartMetaItem::part() { return m_part; }

void PartMetaItem::setModel(QSharedPointer<PartMetaModel> m) { m_model = m; }
} // namespace ORNL
