#include "widgets/gcode_info_control.h"

#include <QtMath>
#include <algorithm>
#include <cmath>
#include <limits>

#include <qboxlayout.h>
#include <qcombobox.h>
#include <qcontainerfwd.h>
#include <qfiledevice.h>
#include <qframe.h>
#include <qgraphicseffect.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qlayoutitem.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qoverload.h>
#include <qsharedpointer.h>
#include <qsizepolicy.h>
#include <qtypes.h>
#include <qvectornd.h>
#include <qwidget.h>

#include "geometry/segment_base.h"
#include "geometry/segments/arc.h"
#include "geometry/segments/bezier.h"
#include "managers/preferences_manager.h"
#include "utilities/constants.h"

namespace ORNL {
namespace {
constexpr double kTwoPi                    = 6.28318530717958647692;
constexpr double kCenterlineArcSampleAngle = kTwoPi / 48.0;
constexpr int kCenterlineBezierSampleCount = 48;
constexpr double kDistanceEpsilon          = 1.0e-9;

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

QVector3D viewPointToMicrons(const Point& point) {
    return point.toQVector3D() * Constants::OpenGL::kViewToObject;
}

QVector<QVector3D> linearCenterlineSamples(const QSharedPointer<SegmentBase>& segment) {
    return {viewPointToMicrons(segment->start()), viewPointToMicrons(segment->end())};
}

QVector<QVector3D> arcCenterlineSamples(const ArcSegment& arc) {
    const Point start   = arc.start();
    const Point center  = arc.center();
    const Point end     = arc.end();
    const double radius = std::hypot(start.x() - center.x(), start.y() - center.y());
    const double sweep  = arc.angle()();

    if (!std::isfinite(radius) || !std::isfinite(sweep) || radius <= kDistanceEpsilon || sweep <= kDistanceEpsilon) {
        return {viewPointToMicrons(start), viewPointToMicrons(end)};
    }

    const int sample_count    = std::max(1, static_cast<int>(std::ceil(std::abs(sweep) / kCenterlineArcSampleAngle)));
    const double start_angle  = std::atan2(start.y() - center.y(), start.x() - center.x());
    const double signed_sweep = arc.counterclockwise() ? sweep : -sweep;
    const double z_delta      = end.z() - start.z();

    QVector<QVector3D> samples;
    samples.reserve(sample_count + 1);
    for (int i = 0; i <= sample_count; ++i) {
        if (i == 0) { samples.push_back(viewPointToMicrons(start)); }
        else if (i == sample_count) { samples.push_back(viewPointToMicrons(end)); }
        else {
            const double t     = static_cast<double>(i) / static_cast<double>(sample_count);
            const double angle = start_angle + (signed_sweep * t);
            samples.push_back(QVector3D(center.x() + (radius * std::cos(angle)),
                                        center.y() + (radius * std::sin(angle)), start.z() + (z_delta * t)) *
                              Constants::OpenGL::kViewToObject);
        }
    }

    return samples;
}

QVector<QVector3D> bezierCenterlineSamples(BezierSegment& bezier) {
    QVector<QVector3D> samples;
    samples.reserve(kCenterlineBezierSampleCount + 1);
    for (int i = 0; i <= kCenterlineBezierSampleCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kCenterlineBezierSampleCount);
        samples.push_back(viewPointToMicrons(bezier.getPointAlong(t)));
    }
    return samples;
}

QVector<QVector3D> centerlineSamples(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull()) { return {}; }

    if (const auto* arc = dynamic_cast<ArcSegment*>(segment.data())) { return arcCenterlineSamples(*arc); }
    if (auto* bezier = dynamic_cast<BezierSegment*>(segment.data())) { return bezierCenterlineSamples(*bezier); }

    return linearCenterlineSamples(segment);
}

double lineSegmentDistance(const QVector3D& first_start, const QVector3D& first_end, const QVector3D& second_start,
                           const QVector3D& second_end) {
    const QVector3D first_direction  = first_end - first_start;
    const QVector3D second_direction = second_end - second_start;
    const QVector3D start_delta      = first_start - second_start;

    const double first_length_squared  = QVector3D::dotProduct(first_direction, first_direction);
    const double second_length_squared = QVector3D::dotProduct(second_direction, second_direction);
    const double second_projection     = QVector3D::dotProduct(second_direction, start_delta);

    double first_t  = 0.0;
    double second_t = 0.0;

    if (first_length_squared <= kDistanceEpsilon && second_length_squared <= kDistanceEpsilon) {
        return first_start.distanceToPoint(second_start);
    }

    if (first_length_squared <= kDistanceEpsilon) { second_t = clamp01(second_projection / second_length_squared); }
    else {
        const double first_projection = QVector3D::dotProduct(first_direction, start_delta);
        if (second_length_squared <= kDistanceEpsilon) { first_t = clamp01(-first_projection / first_length_squared); }
        else {
            const double direction_projection = QVector3D::dotProduct(first_direction, second_direction);
            const double denominator =
                (first_length_squared * second_length_squared) - (direction_projection * direction_projection);

            if (std::abs(denominator) > kDistanceEpsilon) {
                first_t =
                    clamp01(((direction_projection * second_projection) - (first_projection * second_length_squared)) /
                            denominator);
            }

            second_t = (direction_projection * first_t + second_projection) / second_length_squared;
            if (second_t < 0.0) {
                second_t = 0.0;
                first_t  = clamp01(-first_projection / first_length_squared);
            }
            else if (second_t > 1.0) {
                second_t = 1.0;
                first_t  = clamp01((direction_projection - first_projection) / first_length_squared);
            }
        }
    }

    const QVector3D first_closest  = first_start + (first_direction * static_cast<float>(first_t));
    const QVector3D second_closest = second_start + (second_direction * static_cast<float>(second_t));
    return first_closest.distanceToPoint(second_closest);
}

int centerlineSegmentCount(const QVector<QVector3D>& samples) {
    if (samples.isEmpty()) { return 0; }
    return std::max(1, static_cast<int>(samples.size()) - 1);
}

QVector3D centerlineSegmentStart(const QVector<QVector3D>& samples, int index) {
    return samples.size() == 1 ? samples.first() : samples[index];
}

QVector3D centerlineSegmentEnd(const QVector<QVector3D>& samples, int index) {
    return samples.size() == 1 ? samples.first() : samples[index + 1];
}

double centerlineDistance(const QSharedPointer<SegmentBase>& first, const QSharedPointer<SegmentBase>& second) {
    const QVector<QVector3D> first_samples  = centerlineSamples(first);
    const QVector<QVector3D> second_samples = centerlineSamples(second);
    const int first_segment_count           = centerlineSegmentCount(first_samples);
    const int second_segment_count          = centerlineSegmentCount(second_samples);
    if (first_segment_count == 0 || second_segment_count == 0) { return std::numeric_limits<double>::infinity(); }

    double min_distance = std::numeric_limits<double>::infinity();
    for (int first_index = 0; first_index < first_segment_count; ++first_index) {
        for (int second_index = 0; second_index < second_segment_count; ++second_index) {
            min_distance =
                std::min(min_distance, lineSegmentDistance(centerlineSegmentStart(first_samples, first_index),
                                                           centerlineSegmentEnd(first_samples, first_index),
                                                           centerlineSegmentStart(second_samples, second_index),
                                                           centerlineSegmentEnd(second_samples, second_index)));
        }
    }

    return min_distance;
}
}  // namespace

GCodeInfoControl::GCodeInfoControl(QWidget* parent) : QWidget(parent) {
    setupWidget();
}

void GCodeInfoControl::setGCode(QVector<QVector<QSharedPointer<SegmentBase>>> gcode) {
    m_gcode = gcode;
    m_current_segment.clear();

    m_line_no_list.clear();
    fillSegmentInfo(0);

    m_headercb_lines->clear();
}

void GCodeInfoControl::fillSegmentInfo(uint lineNo) {
    if (m_line_no_list.length() > 0 && lineNo > 0) {
        QSharedPointer<SegmentBase> seg = segmentForLine(lineNo);
        if (!seg.isNull()) {
            m_infolbl_type->setText(seg->m_segment_info_meta.type);
            m_infolbl_speed->setText(seg->m_segment_info_meta.speed);
            m_infolbl_extruder_speed->setText(seg->m_segment_info_meta.extruderSpeed);
            m_infolbl_length->setText(seg->m_segment_info_meta.length);
            m_infolbl_width->setText(seg->m_segment_info_meta.width);
            m_infolbl_layer_no->setText(QString::number(seg->layerNumber()));
            m_infolbl_line_no->setText(QString::number(lineNo));
            m_current_segment = seg;
            updateDirectionForSegment(m_current_segment);
            updateCenterDistanceInfo(lineNo);

            return;
        }
    }

    m_infolbl_type->setText("");
    m_infolbl_speed->setText("");
    m_infolbl_extruder_speed->setText("");
    m_infolbl_length->setText("");
    m_infolbl_width->setText("");
    m_infolbl_center_distance->setText("");
    m_infolbl_layer_no->setText("");
    m_infolbl_line_no->setText("");

    m_current_segment.clear();
    m_infolbl_direction->setVisible(false);
}

void GCodeInfoControl::addSegmentInfo(int selectedLineNumber) {
    QString textVal = QString::number(selectedLineNumber);

    int index = m_line_no_list.indexOf(selectedLineNumber);
    if (index < 0) {
        index = 0;
        for (; index < m_line_no_list.length(); ++index) {
            if (m_line_no_list[index] > selectedLineNumber) break;
        }

        m_line_no_list.insert(index, selectedLineNumber);
        m_headercb_lines->insertItem(index, textVal);
    }

    m_headercb_lines->setCurrentText(textVal);
}

void GCodeInfoControl::removeSegmentInfo(int selectedLineNumber) {
    int index = m_line_no_list.indexOf(selectedLineNumber);
    if (index < 0) {
        fillSegmentInfo(0);
        return;
    }

    m_line_no_list.removeAt(index);
    m_headercb_lines->removeItem(index);

    if (m_line_no_list.isEmpty() || m_headercb_lines->currentIndex() < 0) { fillSegmentInfo(0); }
    else { fillSegmentInfo(m_line_no_list[m_headercb_lines->currentIndex()]); }
}

QSharedPointer<SegmentBase> GCodeInfoControl::segmentForLine(uint lineNo) {
    for (auto& layer : m_gcode) {
        if (layer.isEmpty() || layer.back()->lineNumber() < lineNo) continue;

        for (auto& seg : layer) {
            if (seg->lineNumber() == lineNo) return seg;
        }
    }

    return nullptr;
}

void GCodeInfoControl::updateCenterDistanceInfo(uint lineNo) {
    m_infolbl_center_distance->setText("");
    if (m_line_no_list.size() != 2) return;

    const int first_line  = m_line_no_list.first();
    const int second_line = m_line_no_list.last();
    const int other_line  = first_line == static_cast<int>(lineNo) ? second_line : first_line;

    QSharedPointer<SegmentBase> current_segment = segmentForLine(lineNo);
    QSharedPointer<SegmentBase> other_segment   = segmentForLine(other_line);
    if (current_segment.isNull() || other_segment.isNull()) return;

    const double distance = centerlineDistance(current_segment, other_segment);
    if (!std::isfinite(distance)) return;

    m_infolbl_center_distance->setText(QString("Line %1: %2").arg(other_line).arg(formatDistance(distance)));
}

QString GCodeInfoControl::formatDistance(double microns) const {
    const Distance unit = PreferencesManager::getInstance()->getDistanceUnit();
    return QString("%1 %2").arg(QString::number(Distance(microns).to(unit), 'f', 3),
                                PreferencesManager::getInstance()->getDistanceUnitText());
}

void GCodeInfoControl::updateDirection(double angle) {
    m_infolbl_direction->setVisible(true);
    m_infolbl_direction->setPixmap(m_infopm_direction->transformed(QTransform().rotate(angle)));
}

void GCodeInfoControl::updateZDirection(double angle) {
    m_infolbl_direction->setVisible(true);
    m_infolbl_direction->setPixmap(m_infopm_direction_z->transformed(QTransform().rotate(angle)));
}

void GCodeInfoControl::updateDirectionForSegment(const QSharedPointer<SegmentBase>& segment) {
    if (segment.isNull()) {
        m_infolbl_direction->setVisible(false);
        return;
    }

    auto& meta = segment->m_segment_info_meta;
    QVector3D direction(meta.end.x() - meta.start.x(), meta.end.y() - meta.start.y(), meta.end.z() - meta.start.z());

    if (meta.isXYmove()) { updateDirection(viewAngleForDirection(direction, 360 - meta.getCCWXAngle())); }
    else {
        float diff = meta.getZChange();
        if (diff == 0) { m_infolbl_direction->setVisible(false); }
        else { updateZDirection(viewAngleForDirection(direction, diff > 0 ? 180 : 0)); }
    }
}

double GCodeInfoControl::viewAngleForDirection(const QVector3D& direction, double fallbackAngle) const {
    constexpr float kMinimumVisibleDirection = 1.0e-6f;

    const QVector3D view_direction = m_view_matrix.mapVector(direction);
    if (std::abs(view_direction.x()) < kMinimumVisibleDirection &&
        std::abs(view_direction.y()) < kMinimumVisibleDirection) {
        return fallbackAngle;
    }

    double angle = qRadiansToDegrees(std::atan2(view_direction.y(), view_direction.x()));
    if (angle < 0.0) angle += 360.0;

    return 360.0 - angle;
}

void GCodeInfoControl::setViewMatrix(const QMatrix4x4& viewMatrix) {
    m_view_matrix = viewMatrix;
    updateDirectionForSegment(m_current_segment);
}

void GCodeInfoControl::setupWidget() {
    QLabel* lbl2DAxis = new QLabel;

    m_infolbl_type            = new QLabel;
    m_infolbl_speed           = new QLabel;
    m_infolbl_extruder_speed  = new QLabel;
    m_infolbl_length          = new QLabel;
    m_infolbl_width           = new QLabel;
    m_infolbl_center_distance = new QLabel;
    m_infolbl_layer_no        = new QLabel;
    m_infolbl_line_no         = new QLabel;
    m_infolbl_direction       = new QLabel;
    m_infopm_direction        = new QPixmap(":/icons/right_flat.png");
    m_infopm_direction_z      = new QPixmap(":/icons/down_black.png");
    lbl2DAxis->setPixmap(QPixmap(":/icons/2d_axis.png"));

    m_info_display           = new QFrame;
    m_info_display_indicator = new QLabel;
    m_info_display_indicator->setPixmap(QPixmap(":/icons/down_black.png"));

    lbl2DAxis->setToolTip("Print Orientation, 2D (X Y)");
    m_infolbl_direction->setToolTip("Print Direction\nMatches the current G-Code view");

    m_info_grid = new QGridLayout(m_info_display);
    m_info_grid->setRowMinimumHeight(0, 75);
    m_info_grid->setRowMinimumHeight(1, 15);
    m_info_grid->setColumnStretch(1, 1);
    m_info_grid->setVerticalSpacing(0);

    setupHeaderWidget();

    m_info_grid->addWidget(lbl2DAxis, 0, 0);
    m_info_grid->addWidget(m_infolbl_direction, 0, 1);
    m_info_grid->addWidget(new QLabel("Type"), 2, 0);
    m_info_grid->addWidget(m_infolbl_type, 2, 1);
    m_info_grid->addWidget(new QLabel("Print Speed"), 3, 0);
    m_info_grid->addWidget(m_infolbl_speed, 3, 1);
    m_info_grid->addWidget(new QLabel("Extruder Speed"), 4, 0);
    m_info_grid->addWidget(m_infolbl_extruder_speed, 4, 1);
    m_info_grid->addWidget(new QLabel("Length"), 5, 0);
    m_info_grid->addWidget(m_infolbl_length, 5, 1);
    m_info_grid->addWidget(new QLabel("Bead Width"), 6, 0);
    m_info_grid->addWidget(m_infolbl_width, 6, 1);
    m_info_grid->addWidget(new QLabel("Centerline Distance"), 7, 0);
    m_info_grid->addWidget(m_infolbl_center_distance, 7, 1);
    m_info_grid->addWidget(new QLabel("Layer #"), 8, 0);
    m_info_grid->addWidget(m_infolbl_layer_no, 8, 1);
    m_info_grid->addWidget(new QLabel("G-Code Line #"), 9, 0);
    m_info_grid->addWidget(m_infolbl_line_no, 9, 1);

    fillSegmentInfo(0);
}

void GCodeInfoControl::setupHeaderWidget() {
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(Constants::UI::Common::DropShadow::kBlurRadius);
    effect->setXOffset(Constants::UI::Common::DropShadow::kXOffset);
    effect->setYOffset(Constants::UI::Common::DropShadow::kYOffset);
    effect->setColor(Constants::UI::Common::DropShadow::kColor);
    this->setGraphicsEffect(effect);
    this->setVisible(false);
    this->resize(QSize(520, 350));

    QSharedPointer<QFile> style = QSharedPointer<QFile>(
        new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "gcode_info_control.qss"));
    style->open(QIODevice::ReadOnly);
    auto gcodeInfoControlStyle = style->readAll();
    style->close();

    QClickableFrame* infoHeader = new QClickableFrame;
    infoHeader->setFixedHeight(55);
    infoHeader->setFrameStyle(QFrame::Panel | QFrame::Raised);
    infoHeader->setStyleSheet(gcodeInfoControlStyle);
    connect(infoHeader, &QClickableFrame::mouseLeftButtonClicked, this, [this]() {
        if (m_info_display->isVisible()) {
            m_info_display_indicator->setPixmap(QPixmap(":/icons/up_black.png"));
            m_info_display->hide();
        }
        else {
            m_info_display_indicator->setPixmap(QPixmap(":/icons/down_black.png"));
            m_info_display->show();
        }

        this->update();
    });

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_info_display, 0, Qt::AlignmentFlag::AlignBottom);
    layout->addWidget(infoHeader, 0, Qt::AlignmentFlag::AlignBottom);

    QHBoxLayout* hlayout = new QHBoxLayout(infoHeader);
    hlayout->setContentsMargins(0, 0, 0, 0);

    QLabel* picture = new QLabel;
    picture->setPixmap((new QIcon(":/icons/ornlslicer_logo.png"))->pixmap(QSize(28, 28), QIcon::Normal, QIcon::On));

    QFrame* line = new QFrame;
    line->setFrameShape(QFrame::VLine);
    line->setFixedSize(1, 32);
    line->setFrameShadow(QFrame::Sunken);

    m_headercb_lines = new QComboBox;
    m_headercb_lines->setFixedWidth(120);
    connect(m_headercb_lines, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int selIndex) {
        fillSegmentInfo(m_line_no_list.length() > 0 && selIndex >= 0 ? m_line_no_list[selIndex] : 0);
    });

    hlayout->addWidget(picture);
    hlayout->addWidget(new QLabel("Bead / Segment Info"));
    hlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));
    hlayout->addWidget(m_info_display_indicator);
    hlayout->addWidget(line);
    hlayout->addWidget(m_headercb_lines);
}
}  // namespace ORNL
