#include "graphics/view/gcode_view.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

#include <qcontainerfwd.h>
#include <qcursor.h>
#include <qlist.h>
#include <qmatrix4x4.h>
#include <qpoint.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvectornd.h>

#include "configs/settings_base.h"
#include "geometry/segment_base.h"
#include "graphics/base_view.h"
#include "graphics/objects/axes_object.h"
#include "graphics/objects/gcode_object.h"
#include "graphics/objects/part_object.h"
#include "graphics/objects/printer/cartesian_printer_object.h"
#include "graphics/objects/printer/cylindrical_printer_object.h"
#include "graphics/objects/sphere/seam_object.h"
#include "graphics/support/part_picker.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"
#include "widgets/gcode_info_control.h"
#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {
namespace {
constexpr float kLinePickToleranceSquared = 0.000225f;
constexpr uint kHoverTrackingSegmentLimit = 10000;

struct LinePickResult {
    float distance_squared;
    float depth;
};

LinePickResult projectLineHit(const QPointF& point, const QPointF& start, const QPointF& end, float start_depth,
                              float end_depth) {
    const float dx = end.x() - start.x();
    const float dy = end.y() - start.y();
    const float length_squared = (dx * dx) + (dy * dy);

    if (length_squared <= std::numeric_limits<float>::epsilon()) {
        const float point_dx = point.x() - start.x();
        const float point_dy = point.y() - start.y();
        return {(point_dx * point_dx) + (point_dy * point_dy), start_depth};
    }

    float t = ((point.x() - start.x()) * dx + (point.y() - start.y()) * dy) / length_squared;
    t = MathUtils::clamp(0.0f, t, 1.0f);

    const float projected_x = start.x() + (t * dx);
    const float projected_y = start.y() + (t * dy);
    const float point_dx = point.x() - projected_x;
    const float point_dy = point.y() - projected_y;
    const float depth = start_depth + (t * (end_depth - start_depth));
    return {(point_dx * point_dx) + (point_dy * point_dy), depth};
}

bool projectToNdc(const QMatrix4x4& projection, const QMatrix4x4& view, const QVector3D& point, QPointF& ndc,
                  float& depth) {
    const QVector4D clip = projection * view * QVector4D(point, 1.0f);
    if (qFuzzyIsNull(clip.w())) {
        return false;
    }

    const QVector3D projected = clip.toVector3DAffine();
    ndc = QPointF(projected.x(), projected.y());
    depth = projected.z();
    return true;
}
} // namespace

GCodeView::GCodeView(QSharedPointer<SettingsBase> sb, QSharedPointer<GCodeInfoControl> segmentInfoControl) {
    m_sb = sb;
    m_segment_info_control = segmentInfoControl;
    m_use_true_segment_widths = PreferencesManager::getInstance()->getUseTrueWidthsPreference();
}

void GCodeView::zoomIn() {
    this->BaseView::zoomIn();
    updateSegmentInfoViewMatrix();
}

void GCodeView::zoomOut() {
    this->BaseView::zoomOut();
    updateSegmentInfoViewMatrix();
}

void GCodeView::resetZoom() {
    this->BaseView::resetZoom();
    updateSegmentInfoViewMatrix();
}

void GCodeView::setTopView() {
    this->BaseView::setTopView();
    updateSegmentInfoViewMatrix();
}

void GCodeView::setSideView() {
    this->BaseView::setSideView();
    updateSegmentInfoViewMatrix();
}

void GCodeView::setFrontView() {
    this->BaseView::setFrontView();
    updateSegmentInfoViewMatrix();
}

void GCodeView::setForwardView() {
    this->BaseView::setForwardView();
    updateSegmentInfoViewMatrix();
}

void GCodeView::setIsoView() {
    this->BaseView::setIsoView();
    updateSegmentInfoViewMatrix();
}

void GCodeView::useOrthographic(bool ortho) {
    m_state.ortho = ortho;

    this->resizeGL(this->width(), this->height());

    if (!m_state.ortho)
        this->setForwardView();
    else
        this->setTopView();

    updateSegmentInfoViewMatrix();
}

void GCodeView::addGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode) {
    clearGhosts();
    clearTrueWidthOverlay();
    if (!m_gcode_object.isNull()) {
        m_printer->orphanChild(m_gcode_object);
        m_gcode_object.reset();
    }

    m_true_width_overlay_key_valid = false;
    m_gcode = gcode;

    if (m_state.ortho) {
        m_state.zoom_factor = 1.0f;
        this->resizeGL(this->width(), this->height());
    }

    if (gcode.isEmpty()) {
        m_gcode_object = nullptr;
    }
    else {
        if (m_state.low_layer >= gcode.size()) {
            m_state.low_layer = gcode.size() - 1;
        }
        if (m_state.high_layer >= gcode.size()) {
            m_state.high_layer = gcode.size() - 1;
        }
        if (m_state.high_layer < m_state.low_layer) {
            m_state.high_layer = m_state.low_layer;
        }

        QSharedPointer<PreferencesManager> preferences = PreferencesManager::getInstance();
        m_gcode_object = QSharedPointer<GCodeObject>::create(this, gcode, m_segment_info_control,
                                                             m_use_true_segment_widths,
                                                             preferences->getGCodePreviewModePreference(),
                                                             preferences->getGCodePreviewVertexThresholdPreference());
        m_gcode_object->showLayers(m_state.low_layer, m_state.high_layer);
        if (m_state.high_segment != std::numeric_limits<uint>::max()) {
            const uint max_segment = m_gcode_object->visibleSegmentCount();
            if (max_segment > 0) {
                const uint high_segment = std::min(m_state.high_segment, max_segment - 1);
                m_gcode_object->showSegments(std::min(m_state.low_segment, high_segment), high_segment);
            }
        }
        m_gcode_object->hideSegmentType(m_state.hidden_type, true);

        m_printer->adoptChild(m_gcode_object);
        refreshTrueWidthOverlay();
    }
    updateHoverTracking();

    if (m_state.showing_ghosts) {
        rebuildGhosts();
    }

    this->update();
    updateSegmentInfoViewMatrix();
}

bool GCodeView::beginOptimizationPointDrag(QPointF mouse_ndc_pos) {
    auto picked =
        m_printer->pickOptimizationPoint(this->projectionMatrix(), this->viewMatrix(), mouse_ndc_pos, m_state.ortho);
    if (!picked.isValid()) {
        return false;
    }

    QVector3D bed_intersection;
    if (!m_printer->bedIntersection(this->projectionMatrix(), this->viewMatrix(), mouse_ndc_pos, bed_intersection,
                                    m_state.ortho)) {
        return false;
    }

    m_state.dragging_seam = true;
    m_state.dragged_seam = picked.object;
    m_state.dragged_seam_x_setting = picked.x_setting;
    m_state.dragged_seam_y_setting = picked.y_setting;
    m_state.dragged_seam_offset = picked.object->translation() - bed_intersection;

    emit optimizationPointDragStarted(picked.x_setting, picked.y_setting);

    this->setCursor(QCursor(Qt::ClosedHandCursor));
    return true;
}

bool GCodeView::updateOptimizationPointDrag(QPointF mouse_ndc_pos, bool finish) {
    if (!m_state.dragging_seam || m_state.dragged_seam.isNull()) {
        return false;
    }

    QVector3D bed_intersection;
    if (!m_printer->bedIntersection(this->projectionMatrix(), this->viewMatrix(), mouse_ndc_pos, bed_intersection,
                                    m_state.ortho)) {
        return false;
    }

    QVector3D translation = bed_intersection + m_state.dragged_seam_offset;
    translation.setZ(m_printer->printerCenter().z());
    m_state.dragged_seam->translateAbsolute(translation);

    const double x = translation.x() * Constants::OpenGL::kViewToObject;
    const double y = translation.y() * Constants::OpenGL::kViewToObject;
    if (finish) {
        emit optimizationPointDragFinished(m_state.dragged_seam_x_setting, x, m_state.dragged_seam_y_setting, y);
    }
    else {
        emit optimizationPointDragged(m_state.dragged_seam_x_setting, x, m_state.dragged_seam_y_setting, y);
    }

    this->update();
    return true;
}

void GCodeView::finishOptimizationPointDrag(QPointF mouse_ndc_pos) {
    if (!m_state.dragging_seam) {
        return;
    }

    if (!updateOptimizationPointDrag(mouse_ndc_pos, true) && !m_state.dragged_seam.isNull()) {
        const QVector3D translation = m_state.dragged_seam->translation();
        emit optimizationPointDragFinished(m_state.dragged_seam_x_setting,
                                           translation.x() * Constants::OpenGL::kViewToObject,
                                           m_state.dragged_seam_y_setting,
                                           translation.y() * Constants::OpenGL::kViewToObject);
    }

    m_state.dragging_seam = false;
    m_state.dragged_seam.reset();
    m_state.dragged_seam_x_setting.clear();
    m_state.dragged_seam_y_setting.clear();
    m_state.dragged_seam_offset = QVector3D();
    this->setCursor(QCursor(Qt::ArrowCursor));
    this->update();
}

void GCodeView::hideSegmentType(SegmentDisplayType type, bool hidden) {
    m_state.hidden_type = (hidden) ? (m_state.hidden_type | type) : (m_state.hidden_type & ~type);

    if (m_gcode_object.isNull())
        return;

    m_gcode_object->hideSegmentType(type, hidden);
    if (!m_true_width_overlay_object.isNull()) {
        m_true_width_overlay_object->hideSegmentType(type, hidden);
    }

    this->update();
}

void GCodeView::updateHoverTracking() {
    if (m_gcode_object.isNull()) {
        this->setMouseTracking(true);
        return;
    }

    this->setMouseTracking(m_gcode_object->visibleSegmentCount() <= kHoverTrackingSegmentLimit);
}

void GCodeView::clearGhosts() {
    if (m_printer.isNull()) {
        m_ghosted_parts.clear();
        return;
    }

    for (auto ghost : m_ghosted_parts) {
        m_printer->orphanChild(ghost);
    }
    m_ghosted_parts.clear();
}

void GCodeView::rebuildGhosts() {
    if (m_meta_model.isNull() || m_printer.isNull()) {
        return;
    }

    clearGhosts();

    for (auto item : m_meta_model->items()) {
        auto gop = QSharedPointer<PartObject>::create(this, item->part());
        gop->setTransparency(item->transparency());
        gop->setTransformation(item->transformation());
        gop->translateAbsolute(QVector3D(item->translation().x(), item->translation().y(),
                                         item->translation().z() + m_printer->minimum().z()));
        m_printer->adoptChild(gop);
        m_ghosted_parts[item] = gop;
    }
}

void GCodeView::clearTrueWidthOverlay() {
    if (!m_true_width_overlay_object.isNull()) {
        if (!m_gcode_object.isNull()) {
            m_gcode_object->orphanChild(m_true_width_overlay_object);
        }
        m_true_width_overlay_object.reset();
    }
}

QVector<QVector<QSharedPointer<SegmentBase>>> GCodeView::visiblePrintableGCodeSubset() const {
    QVector<QVector<QSharedPointer<SegmentBase>>> subset;
    if (m_gcode.isEmpty()) {
        return subset;
    }

    const uint low_layer = std::min(m_state.low_layer, static_cast<uint>(m_gcode.size() - 1));
    const uint high_layer = std::min(m_state.high_layer, static_cast<uint>(m_gcode.size() - 1));
    if (low_layer > high_layer) {
        return subset;
    }

    const uint high_segment = m_state.high_segment == std::numeric_limits<uint>::max()
                                  ? std::numeric_limits<uint>::max()
                                  : std::max(m_state.low_segment, m_state.high_segment);
    uint visible_segment_index = 0;

    for (uint layer_index = low_layer; layer_index <= high_layer; ++layer_index) {
        QVector<QSharedPointer<SegmentBase>> layer_subset;
        const QVector<QSharedPointer<SegmentBase>>& layer = m_gcode[layer_index];
        layer_subset.reserve(layer.size());

        for (const QSharedPointer<SegmentBase>& segment : layer) {
            const bool in_segment_range =
                visible_segment_index >= m_state.low_segment && visible_segment_index <= high_segment;
            if (in_segment_range && !static_cast<bool>(segment->displayType() & SegmentDisplayType::kTravel)) {
                layer_subset.push_back(segment);
            }

            ++visible_segment_index;
        }

        if (!layer_subset.isEmpty()) {
            subset.push_back(layer_subset);
        }
    }

    return subset;
}

void GCodeView::refreshTrueWidthOverlay() {
    if (m_gcode_object.isNull() || !m_gcode_object->isLightweight() || !m_use_true_segment_widths) {
        clearTrueWidthOverlay();
        m_true_width_overlay_key_valid = false;
        return;
    }

    QSharedPointer<PreferencesManager> preferences = PreferencesManager::getInstance();
    const GCodePreviewMode preview_mode = preferences->getGCodePreviewModePreference();
    const int vertex_threshold = preferences->getGCodePreviewVertexThresholdPreference();

    TrueWidthOverlayKey key;
    key.low_layer = m_state.low_layer;
    key.high_layer = m_state.high_layer;
    key.low_segment = m_state.low_segment;
    key.high_segment = m_state.high_segment;
    key.preview_mode = preview_mode;
    key.vertex_threshold = vertex_threshold;
    key.use_true_widths = m_use_true_segment_widths;

    if (m_true_width_overlay_key_valid && key == m_true_width_overlay_key) {
        return;
    }

    clearTrueWidthOverlay();
    m_true_width_overlay_key = key;
    m_true_width_overlay_key_valid = true;

    if (preview_mode == GCodePreviewMode::kThinLines) {
        return;
    }

    QVector<QVector<QSharedPointer<SegmentBase>>> visible_gcode = visiblePrintableGCodeSubset();
    if (visible_gcode.isEmpty()) {
        return;
    }

    const qsizetype estimated_vertices =
        GCodeObject::estimateTrueWidthVertexCount(visible_gcode, static_cast<qsizetype>(vertex_threshold));
    if (estimated_vertices > vertex_threshold) {
        return;
    }

    m_true_width_overlay_object = QSharedPointer<GCodeObject>::create(
        this, visible_gcode, m_segment_info_control, true, GCodePreviewMode::kTrueWidths, vertex_threshold, false);
    if (m_state.hidden_type != SegmentDisplayType::kNone) {
        m_true_width_overlay_object->hideSegmentType(m_state.hidden_type, true);
    }
    m_true_width_overlay_object->setOnTop(true);
    m_gcode_object->adoptChild(m_true_width_overlay_object);
}

uint GCodeView::pickVisibleSegment(const QPointF& mouse_ndc_pos) {
    if (!m_true_width_overlay_object.isNull()) {
        const uint overlay_line_num = this->pickSegment(mouse_ndc_pos, m_true_width_overlay_object);
        if (overlay_line_num != 0) {
            return overlay_line_num;
        }
    }

    if (m_gcode_object.isNull()) {
        return 0;
    }

    return this->pickSegment(mouse_ndc_pos, m_gcode_object);
}

void GCodeView::updateSegmentWidths(bool use_true_width) {
    clear();
    m_use_true_segment_widths = use_true_width;

    if (!m_gcode.isEmpty()) {
        addGCode(m_gcode);
    }
}

void GCodeView::initView() {
    BuildVolumeType buildVolume = static_cast<BuildVolumeType>(m_sb->setting<int>(PRS::Dimensions::kBuildVolumeType));

    switch (buildVolume) {
        case ORNL::BuildVolumeType::kRectangular:
            m_printer = QSharedPointer<CartesianPrinterObject>::create(this, m_sb, true);
            break;
        case ORNL::BuildVolumeType::kCylindrical:
            m_printer = QSharedPointer<CylindricalPrinterObject>::create(this, m_sb, true);
            break;
        default:
            m_printer = QSharedPointer<CartesianPrinterObject>::create(this, m_sb, true);
            break;
    }

    m_camera->setDefaultZoom(m_printer->getDefaultZoom());

    this->addObject(m_printer);

    this->resetCamera();
}

void GCodeView::handleLeftClick(QPointF mouse_ndc_pos) {
    if (beginOptimizationPointDrag(mouse_ndc_pos)) {
        return;
    }

    if (m_gcode_object.isNull())
        return;

    uint picked_line_num = this->pickVisibleSegment(mouse_ndc_pos);

    if (picked_line_num == 0) {
        if (!m_true_width_overlay_object.isNull()) {
            m_true_width_overlay_object->deselectAll();
        }
        emit updateSelectedSegments(QList<int>(), m_gcode_object->deselectAll());
    }
    else {
        if (m_gcode_object->isCurrentlySelected(picked_line_num)) {
            m_gcode_object->deselectSegment(picked_line_num);
            if (!m_true_width_overlay_object.isNull()) {
                m_true_width_overlay_object->deselectSegment(picked_line_num);
            }
            emit updateSelectedSegments(QList<int>(), QList<int> {(int)picked_line_num - 1});
        }
        else {
            m_gcode_object->selectSegment(picked_line_num);
            if (!m_true_width_overlay_object.isNull()) {
                m_true_width_overlay_object->selectSegment(picked_line_num);
            }
            emit updateSelectedSegments(QList<int> {(int)picked_line_num - 1}, QList<int> {});
        }
    }
    this->update();
}

void GCodeView::handleLeftMove(QPointF mouse_ndc_pos) {
    if (m_state.dragging_seam) {
        updateOptimizationPointDrag(mouse_ndc_pos, false);
    }
}

void GCodeView::handleLeftRelease(QPointF mouse_ndc_pos) {
    if (m_state.dragging_seam) {
        finishOptimizationPointDrag(mouse_ndc_pos);
    }
}

void GCodeView::handleLeftDoubleClick(QPointF mouse_ndc_pos) {
    // NOP
}

void GCodeView::handleMouseMove(QPointF mouse_ndc_pos) {
    auto picked_seam =
        m_printer->pickOptimizationPoint(this->projectionMatrix(), this->viewMatrix(), mouse_ndc_pos, m_state.ortho);
    if (picked_seam.isValid()) {
        if (!m_gcode_object.isNull()) {
            m_gcode_object->highlightSegment(0);
        }
        if (!m_true_width_overlay_object.isNull()) {
            m_true_width_overlay_object->highlightSegment(0);
        }

        this->setCursor(QCursor(Qt::OpenHandCursor));
        this->update();
        return;
    }

    this->setCursor(QCursor(Qt::ArrowCursor));

    if (m_gcode_object.isNull())
        return;

    uint picked_line_num = this->pickVisibleSegment(mouse_ndc_pos);

    m_gcode_object->highlightSegment(picked_line_num);
    if (!m_true_width_overlay_object.isNull()) {
        m_true_width_overlay_object->highlightSegment(picked_line_num);
    }

    this->update();
}

void GCodeView::handleRightMove(QPointF mouse_ndc_pos) {
    if (!m_state.ortho) {
        this->BaseView::handleRightMove(mouse_ndc_pos);
        updateSegmentInfoViewMatrix();
    }
}

void GCodeView::handleWheelForward(QPointF mouse_ndc_pos, float delta) {
    if (!m_state.ortho) {
        this->BaseView::handleWheelForward(mouse_ndc_pos, delta);
        updateSegmentInfoViewMatrix();
        return;
    }

    m_state.zoom_factor += 0.1;
    m_state.zoom_factor = MathUtils::clamp(0.0f, m_state.zoom_factor, 2.0f);

    this->resizeGL(this->width(), this->height());
    updateSegmentInfoViewMatrix();
}

void GCodeView::handleWheelBackward(QPointF mouse_ndc_pos, float delta) {
    if (!m_state.ortho) {
        this->BaseView::handleWheelBackward(mouse_ndc_pos, delta);
        updateSegmentInfoViewMatrix();
        return;
    }

    m_state.zoom_factor -= 0.1;
    m_state.zoom_factor = MathUtils::clamp(0.0f, m_state.zoom_factor, 2.0f);

    this->resizeGL(this->width(), this->height());
    updateSegmentInfoViewMatrix();
}

void GCodeView::translateCamera(QVector3D v, bool absolute) {
    if (absolute) {
        m_camera->panAbsolute(v);
        m_focus->translateAbsolute(m_camera->getPan());
    }
    else {
        m_camera->pan(v);
        m_focus->translateAbsolute(m_camera->getPan());
    }

    updateSegmentInfoViewMatrix();
}

void GCodeView::rotateCamera(QVector2D screen_delta) {
    if (!m_state.ortho)
        this->BaseView::rotateCamera(screen_delta);
}

void GCodeView::resizeGL(int width, int height) {
    if (!m_state.ortho) {
        this->BaseView::resizeGL(width, height);
        return;
    }

    // (Re)Initalize camera projection.
    QMatrix4x4 projection;

    float aspect = (float)width / (float)height;

    if (aspect >= 1)
        width *= (aspect);
    else
        height *= (1 / aspect);

    int quater_width = width / (std::pow(24, m_state.zoom_factor));
    int quater_height = height / (std::pow(24, m_state.zoom_factor));

    projection.setToIdentity();
    projection.ortho(-quater_width, quater_width, -quater_height, quater_height, -Constants::OpenGL::kFarPlane,
                     2 * Constants::OpenGL::kFarPlane);

    this->setProjectionMatrix(projection);

    this->update();
    updateSegmentInfoViewMatrix();
}

uint GCodeView::pickSegment(const QPointF& mouse_ndc_pos, QSharedPointer<GCodeObject> gog) {
    float min_triangle_dist = Constants::Limits::Maximums::kMaxFloat;
    float min_pick_depth = std::numeric_limits<float>::infinity();
    uint picked_seg = 0;

    auto tris = gog->segmentTriangles();

    for (auto& tri : tris) {
        const auto pick = PartPicker::pickDistanceAndIntersection(this->projectionMatrix(), this->viewMatrix(),
                                                                  mouse_ndc_pos, tri.second, m_state.ortho);
        const float dist = std::get<0>(pick);

        if (dist < min_triangle_dist) {
            QPointF ndc;
            float depth = min_pick_depth;
            projectToNdc(this->projectionMatrix(), this->viewMatrix(), std::get<1>(pick), ndc, depth);
            min_triangle_dist = dist;
            min_pick_depth = depth;
            picked_seg = tri.first;
        }
    }

    auto lines = gog->segmentLines();
    for (auto& line : lines) {
        QPointF start_ndc;
        QPointF end_ndc;
        float start_depth;
        float end_depth;
        if (!projectToNdc(this->projectionMatrix(), this->viewMatrix(), line.second.first, start_ndc, start_depth) ||
            !projectToNdc(this->projectionMatrix(), this->viewMatrix(), line.second.second, end_ndc, end_depth)) {
            continue;
        }

        const LinePickResult line_hit = projectLineHit(mouse_ndc_pos, start_ndc, end_ndc, start_depth, end_depth);
        if (line_hit.distance_squared <= kLinePickToleranceSquared && line_hit.depth < min_pick_depth) {
            min_pick_depth = line_hit.depth;
            picked_seg = line.first;
        }
    }

    return picked_seg;
}

void GCodeView::updatePrinterSettings(QSharedPointer<SettingsBase> sb) {
    m_sb = sb;

    BuildVolumeType buildVolume = static_cast<BuildVolumeType>(m_sb->setting<int>(PRS::Dimensions::kBuildVolumeType));

    QSharedPointer<PrinterObject> new_printer;

    switch (buildVolume) {
        case ORNL::BuildVolumeType::kRectangular:
            if (m_printer.dynamicCast<CartesianPrinterObject>().isNull()) {
                new_printer = QSharedPointer<CartesianPrinterObject>::create(this, m_sb, false);
            }
            break;
        case ORNL::BuildVolumeType::kCylindrical:
            if (m_printer.dynamicCast<CylindricalPrinterObject>().isNull()) {
                new_printer = QSharedPointer<CylindricalPrinterObject>::create(this, m_sb, false);
            }
            break;
        default:
            if (m_printer.dynamicCast<CartesianPrinterObject>().isNull()) {
                new_printer = QSharedPointer<CartesianPrinterObject>::create(this, m_sb, false);
            }
            break;
    }

    if (!new_printer.isNull()) {
        if (m_gcode_object != nullptr) {
            m_printer->orphanChild(m_gcode_object);
            new_printer->adoptChild(m_gcode_object);
        }

        this->removeObject(m_printer);
        this->addObject(new_printer);

        new_printer->setSeamsHidden(!m_state.seams_shown);

        m_printer = new_printer;
    }
    else {
        m_printer->updateFromSettings(m_sb);
    }

    m_camera->setDefaultZoom(m_printer->getDefaultZoom());
    this->resetCamera();
}

void GCodeView::showSeams(bool show) {
    m_printer->setSeamsHidden(!show);

    m_state.seams_shown = show;

    this->update();
}

void GCodeView::updateOptimizationSettings(QSharedPointer<SettingsBase> sb) {
    m_sb = sb;

    m_printer->updateFromSettings(m_sb);

    this->update();
}

void GCodeView::setLowLayer(uint low_layer) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showLow(low_layer);
    m_state.low_layer = low_layer;

    uint segment_count = m_gcode_object->visibleSegmentCount();
    updateHoverTracking();

    emit maxSegmentChanged(segment_count == 0 ? 0 : segment_count - 1);
    refreshTrueWidthOverlay();

    this->update();
}

void GCodeView::setHighLayer(uint high_layer) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showHigh(high_layer);
    m_state.high_layer = high_layer;

    uint segment_count = m_gcode_object->visibleSegmentCount();
    updateHoverTracking();

    emit maxSegmentChanged(segment_count == 0 ? 0 : segment_count - 1);
    refreshTrueWidthOverlay();

    this->update();
}

void GCodeView::setLowSegment(uint low_segment) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showLowSegment(low_segment);
    m_state.low_segment = low_segment;

    updateHoverTracking();
    refreshTrueWidthOverlay();

    this->update();
}

void GCodeView::setHighSegment(uint high_segment) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showHighSegment(high_segment);
    m_state.high_segment = high_segment;

    updateHoverTracking();
    refreshTrueWidthOverlay();

    this->update();
}

void GCodeView::updateSegments(QList<int> linesToAdd, QList<int> linesToRemove) {
    if (m_gcode_object.isNull())
        return;

    for (int line_num : linesToAdd) {
        m_gcode_object->selectSegment(line_num + 1);
        if (!m_true_width_overlay_object.isNull()) {
            m_true_width_overlay_object->selectSegment(line_num + 1);
        }
    }

    for (int line_num : linesToRemove) {
        m_gcode_object->deselectSegment(line_num + 1);
        if (!m_true_width_overlay_object.isNull()) {
            m_true_width_overlay_object->deselectSegment(line_num + 1);
        }
    }

    this->update();
}

void GCodeView::clear() {
    clearTrueWidthOverlay();
    m_true_width_overlay_key_valid = false;

    if (!m_gcode_object.isNull()) {
        m_printer->orphanChild(m_gcode_object);
        m_gcode_object.reset();
    }

    clearGhosts();

    this->update();
}

void GCodeView::setMeta(QSharedPointer<PartMetaModel> meta) {
    m_meta_model = meta;

    // Setup hook for transparency
    connect(m_meta_model.get(), &PartMetaModel::visualUpdate, this, [this](QSharedPointer<PartMetaItem> pm) {
        if (m_ghosted_parts.contains(pm)) {
            m_ghosted_parts[pm]->setTransparency(pm->transparency());
            update();
        }
    });
}

void GCodeView::showGhosts(bool show) {
    m_state.showing_ghosts = show;
    if (show && m_ghosted_parts.isEmpty()) {
        rebuildGhosts();
    }

    for (auto ghost : m_ghosted_parts) {
        if (show) {
            ghost->show();
        }
        else
            ghost->hide();
    }

    this->update();
}

void GCodeView::resetCamera() {
    // Reset rotation and zoom
    m_camera->reset();

    this->translateCamera(
        QVector3D(m_printer->printerCenter().x(), m_printer->printerCenter().y(), m_printer->minimum().z()), true);

    this->update(); // Need to repaint with new model matrices
    updateSegmentInfoViewMatrix();
}

void GCodeView::updateSegmentInfoViewMatrix() {
    if (!m_segment_info_control.isNull())
        m_segment_info_control->setViewMatrix(this->viewMatrix());
}
} // namespace ORNL
