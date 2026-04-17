#include "widgets/gcode_widget.h"

#include <QResizeEvent>
#include <qboxlayout.h>
#include <qcontainerfwd.h>
#include <qobject.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <qsurfaceformat.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "geometry/segment_base.h"
#include "graphics/view/gcode_view.h"
#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "widgets/gcode_info_control.h"
#include "widgets/part_widget/model/part_meta_model.h"
#include "widgets/view_controls_toolbar.h"

namespace ORNL {
GCodeWidget::GCodeWidget(QWidget* parent) : QWidget(parent) { this->setupWidget(); }

GCodeView* GCodeWidget::view() {
    m_view_controls->raise(); // If the gcode view is changed, the controls need to be raised as well
    return m_gcode_view;
}

void GCodeWidget::setPartMeta(QSharedPointer<PartMetaModel> meta) { m_gcode_view->setMeta(meta); }

void GCodeWidget::addGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode) { m_gcode_view->addGCode(gcode); }

void GCodeWidget::clear() { m_gcode_view->clear(); }

void GCodeWidget::setOrthoView(bool status) {
    m_view_controls->setEnabled(!status);
    m_gcode_view->useOrthographic(status);
}

void GCodeWidget::showSegmentInfo(bool show) { m_segment_info_control->setVisible(show); }

void GCodeWidget::showGhosts(bool status) { m_gcode_view->showGhosts(status); }

void GCodeWidget::handleModifiedSetting(QString key) {
    static const auto printer_settings = QSet<QString> {PRS::Dimensions::kXMin,
                                                        PRS::Dimensions::kXMax,
                                                        PRS::Dimensions::kYMin,
                                                        PRS::Dimensions::kYMax,
                                                        PRS::Dimensions::kZMin,
                                                        PRS::Dimensions::kZMax,
                                                        PRS::Dimensions::kXOffset,
                                                        PRS::Dimensions::kYOffset,
                                                        PRS::Dimensions::kWMin,
                                                        PRS::Dimensions::kWMax,
                                                        PRS::Dimensions::kBuildVolumeType,
                                                        PRS::Dimensions::kInnerRadius,
                                                        PRS::Dimensions::kOuterRadius,
                                                        PRS::Dimensions::kEnableW,
                                                        PRS::Dimensions::kEnableGridX,
                                                        PRS::Dimensions::kGridXDistance,
                                                        PRS::Dimensions::kGridXOffset,
                                                        PRS::Dimensions::kEnableGridY,
                                                        PRS::Dimensions::kGridYDistance,
                                                        PRS::Dimensions::kGridYOffset};

    if (printer_settings.contains(key)) {
        m_gcode_view->updatePrinterSettings(GSM->getGlobal());
    }
}

void GCodeWidget::zoomIn() { m_gcode_view->zoomIn(); }

void GCodeWidget::zoomOut() { m_gcode_view->zoomOut(); }

void GCodeWidget::resetZoom() { m_gcode_view->resetZoom(); }

void GCodeWidget::resetCamera() { m_gcode_view->resetCamera(); }

void GCodeWidget::setupWidget() {
    this->setupSubWidgets();
    this->setupLayouts();
    this->setupPosition();
    this->setupEvents();
}

void GCodeWidget::setupSubWidgets() {
    // Segment info Control
    m_segment_info_control = QSharedPointer<GCodeInfoControl>(new GCodeInfoControl(this));

    // Set OpenGL Version information
    // Note: This format must be set before show() is called.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);
    format.setVersion(3, 3);

    // OpenGL View
    m_gcode_view = new GCodeView(GSM->getGlobal(), m_segment_info_control);
    m_gcode_view->resize(this->width(), this->height());
    m_gcode_view->setFormat(format);
    SegmentDisplayType types = SegmentDisplayType::kNone;
    if (PreferencesManager::getInstance()->getHideTravelPreference()) {
        types |= SegmentDisplayType::kTravel;
    }
    if (PreferencesManager::getInstance()->getHideSupportPreference()) {
        types |= SegmentDisplayType::kSupport;
    }
    m_gcode_view->hideSegmentType(types, true);

    // View Controls
    m_view_controls = new ViewControlsToolbar(this);
    m_view_controls->raise();
}

void GCodeWidget::setupStyle() {
    m_gcode_view->setupStyle();
    m_view_controls->setupStyle();
}

void GCodeWidget::setupLayouts() {
    m_layout = new QVBoxLayout(this);
    m_layout->addWidget(m_gcode_view);
}

void GCodeWidget::setupPosition() {}

void GCodeWidget::setupEvents() {
    connect(this, &GCodeWidget::resized, m_view_controls, &ViewControlsToolbar::resize);

    // Buttons -> PartWidget: Connect view buttons
    connect(m_view_controls, &ViewControlsToolbar::setIsoView, m_gcode_view, &GCodeView::setForwardView);
    connect(m_view_controls, &ViewControlsToolbar::setFrontView, m_gcode_view, &GCodeView::setFrontView);
    connect(m_view_controls, &ViewControlsToolbar::setSideView, m_gcode_view, &GCodeView::setSideView);
    connect(m_view_controls, &ViewControlsToolbar::setTopView, m_gcode_view, &GCodeView::setTopView);
}

void GCodeWidget::resizeEvent(QResizeEvent* event) {
    m_segment_info_control->move(10, event->size().height() - (m_segment_info_control->height() + 10));
    m_segment_info_control->raise();

    emit resized(event->size());
}
} // namespace ORNL
