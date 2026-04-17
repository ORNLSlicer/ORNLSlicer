#include "graphics/view/gcode_view.h"

#include <cmath>

#include <qcontainerfwd.h>
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
#include "graphics/objects/printer/toroidal_printer_object.h"
#include "graphics/support/part_picker.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "utilities/mathutils.h"
#include "widgets/gcode_info_control.h"
#include "widgets/part_widget/model/part_meta_item.h"
#include "widgets/part_widget/model/part_meta_model.h"

namespace ORNL {
GCodeView::GCodeView(QSharedPointer<SettingsBase> sb, QSharedPointer<GCodeInfoControl> segmentInfoControl) {
    m_sb = sb;
    m_segment_info_control = segmentInfoControl;
    m_use_true_segment_widths = PreferencesManager::getInstance()->getUseTrueWidthsPreference();
}

void GCodeView::useOrthographic(bool ortho) {
    m_state.ortho = ortho;

    this->resizeGL(this->width(), this->height());

    if (!m_state.ortho)
        this->setForwardView();
    else
        this->setTopView();
}

void GCodeView::addGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode) {
    // Adjust segment widths down if needed
    if (!m_use_true_segment_widths) {
        for (auto& layer : gcode) {
            for (auto& segment : layer) {
                segment->setDisplayWidth(segment->displayWidth() * 0.2f);
                segment->setDisplayHeight(segment->displayHeight() * 0.2f);
            }
        }
    }

    if (m_state.ortho) {
        m_state.zoom_factor = 1.0f;
        this->resizeGL(this->width(), this->height());
    }

    if (gcode.isEmpty()) {
        m_printer->orphanChild(m_gcode_object);
        m_gcode_object = nullptr;
    }
    else {
        if (m_state.high_layer >= gcode.size()) {
            m_state.high_layer = gcode.size() - 1;
        }

        m_gcode_object = QSharedPointer<GCodeObject>::create(this, gcode, m_segment_info_control);
        m_gcode_object->showLayers(m_state.low_layer, m_state.high_layer);
        m_gcode_object->hideSegmentType(m_state.hidden_type, true);

        m_printer->adoptChild(m_gcode_object);
    }

    // Clear out old ghosted parts
    for (auto ghost : m_ghosted_parts) {
        m_printer->orphanChild(ghost);
    }
    m_ghosted_parts.clear();

    // Add ghosted parts
    for (auto item : m_meta_model->items()) {
        auto gop = QSharedPointer<PartObject>::create(this, item->part());
        gop->setTransparency(item->transparency());

        if (!m_state.showing_ghosts) {
            gop->hide();
        }

        gop->setTransformation(item->transformation());
        gop->translateAbsolute(QVector3D(item->translation().x(), item->translation().y(),
                                         item->translation().z() + m_printer->minimum().z()));
        m_printer->adoptChild(gop);
        m_ghosted_parts[item] = gop;
    }

    this->update();
    m_gcode = gcode;
}

void GCodeView::hideSegmentType(SegmentDisplayType type, bool hidden) {
    m_state.hidden_type = (hidden) ? (m_state.hidden_type | type) : (m_state.hidden_type & ~type);

    if (m_gcode_object.isNull())
        return;

    m_gcode_object->hideSegmentType(type, hidden);

    this->update();
}

void GCodeView::updateSegmentWidths(bool use_true_width) {
    clear();
    m_use_true_segment_widths = use_true_width;

    // Adjust existing G-code
    if (!m_gcode.isEmpty()) {
        // Adjust segment widths back up if needed
        if (m_use_true_segment_widths) {
            for (auto& layer : m_gcode) {
                for (auto& segment : layer) {
                    segment->setDisplayWidth(segment->displayWidth() * 5.0f);
                    segment->setDisplayHeight(segment->displayHeight() * 5.0f);
                }
            }
        }
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
        case ORNL::BuildVolumeType::kToroidal:
            m_printer = QSharedPointer<ToroidalPrinterObject>::create(this, m_sb, true);
            break;
    }

    m_camera->setDefaultZoom(m_printer->getDefaultZoom());

    this->addObject(m_printer);

    this->resetCamera();
}

void GCodeView::handleLeftClick(QPointF mouse_ndc_pos) {
    if (m_gcode_object.isNull())
        return;

    uint picked_line_num = this->pickSegment(mouse_ndc_pos, m_gcode_object);

    if (picked_line_num == 0)
        emit updateSelectedSegments(QList<int>(), m_gcode_object->deselectAll());
    else {
        if (m_gcode_object->isCurrentlySelected(picked_line_num)) {
            m_gcode_object->deselectSegment(picked_line_num);
            emit updateSelectedSegments(QList<int>(), QList<int> {(int)picked_line_num - 1});
        }
        else {
            m_gcode_object->selectSegment(picked_line_num);
            emit updateSelectedSegments(QList<int> {(int)picked_line_num - 1}, QList<int> {});
        }
    }
    this->update();
}

void GCodeView::handleLeftDoubleClick(QPointF mouse_ndc_pos) {
    // NOP
}

void GCodeView::handleMouseMove(QPointF mouse_ndc_pos) {
    if (m_gcode_object.isNull())
        return;

    uint picked_line_num = this->pickSegment(mouse_ndc_pos, m_gcode_object);

    m_gcode_object->highlightSegment(picked_line_num);

    this->update();
}

void GCodeView::handleRightMove(QPointF mouse_ndc_pos) {
    if (!m_state.ortho)
        this->BaseView::handleRightMove(mouse_ndc_pos);
}

void GCodeView::handleWheelForward(QPointF mouse_ndc_pos, float delta) {
    if (!m_state.ortho) {
        this->BaseView::handleWheelForward(mouse_ndc_pos, delta);
        return;
    }

    m_state.zoom_factor += 0.1;
    m_state.zoom_factor = MathUtils::clamp(0.0f, m_state.zoom_factor, 2.0f);

    this->resizeGL(this->width(), this->height());
}

void GCodeView::handleWheelBackward(QPointF mouse_ndc_pos, float delta) {
    if (!m_state.ortho) {
        this->BaseView::handleWheelBackward(mouse_ndc_pos, delta);
        return;
    }

    m_state.zoom_factor -= 0.1;
    m_state.zoom_factor = MathUtils::clamp(0.0f, m_state.zoom_factor, 2.0f);

    this->resizeGL(this->width(), this->height());
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
}

uint GCodeView::pickSegment(const QPointF& mouse_ndc_pos, QSharedPointer<GCodeObject> gog) {
    float min_dist = Constants::Limits::Maximums::kMaxFloat;
    uint picked_seg = 0;

    auto tris = gog->segmentTriangles();

    for (auto& tri : tris) {
        float dist = PartPicker::pickDistance(this->projectionMatrix(), this->viewMatrix(), mouse_ndc_pos, tri.second,
                                              m_state.ortho);

        if (dist < min_dist) {
            min_dist = dist;
            picked_seg = tri.first;
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
        case ORNL::BuildVolumeType::kToroidal:
            if (m_printer.dynamicCast<ToroidalPrinterObject>().isNull()) {
                new_printer = QSharedPointer<ToroidalPrinterObject>::create(this, m_sb, false);
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

        m_printer = new_printer;
    }
    else {
        m_printer->updateFromSettings(m_sb);
    }

    m_camera->setDefaultZoom(m_printer->getDefaultZoom());
    this->resetCamera();
}

void GCodeView::setLowLayer(uint low_layer) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showLow(low_layer);
    m_state.low_layer = low_layer;

    // When there are more than 10000 segments being shown at once,
    // disable the mouse tracking (highlighting when mouse on top of
    // segment). This helps performance.
    uint segment_count = m_gcode_object->visibleSegmentCount();
    if (segment_count > 10000)
        this->setMouseTracking(false);
    else
        this->setMouseTracking(true);

    emit maxSegmentChanged(segment_count - 1);

    this->update();
}

void GCodeView::setHighLayer(uint high_layer) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showHigh(high_layer);
    m_state.high_layer = high_layer;

    uint segment_count = m_gcode_object->visibleSegmentCount();
    if (segment_count > 10000)
        this->setMouseTracking(false);
    else
        this->setMouseTracking(true);

    emit maxSegmentChanged(segment_count - 1);

    this->update();
}

void GCodeView::setLowSegment(uint low_segment) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showLowSegment(low_segment);

    if (m_gcode_object->visibleSegmentCount() > 10000)
        this->setMouseTracking(false);
    else
        this->setMouseTracking(true);

    this->update();
}

void GCodeView::setHighSegment(uint high_segment) {
    if (m_gcode_object.isNull())
        return;

    m_gcode_object->showHighSegment(high_segment);

    if (m_gcode_object->visibleSegmentCount() > 10000)
        this->setMouseTracking(false);
    else
        this->setMouseTracking(true);

    this->update();
}

void GCodeView::updateSegments(QList<int> linesToAdd, QList<int> linesToRemove) {
    if (m_gcode_object.isNull())
        return;

    for (int line_num : linesToAdd)
        m_gcode_object->selectSegment(line_num + 1);

    for (int line_num : linesToRemove)
        m_gcode_object->deselectSegment(line_num + 1);

    this->update();
}

void GCodeView::clear() {
    if (m_gcode_object.isNull())
        return;

    m_printer->orphanChild(m_gcode_object);
    m_gcode_object.reset();

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
    for (auto ghost : m_ghosted_parts)
        if (show) {
            ghost->show();
        }
        else
            ghost->hide();
}

void GCodeView::resetCamera() {
    // Reset rotation and zoom
    m_camera->reset();

    this->translateCamera(
        QVector3D(m_printer->printerCenter().x(), m_printer->printerCenter().y(), m_printer->minimum().z()), true);

    this->update(); // Need to repaint with new model matrices
}
} // namespace ORNL
