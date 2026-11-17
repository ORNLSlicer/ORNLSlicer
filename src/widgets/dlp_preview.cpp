#include "widgets/dlp_preview.h"

#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTransform>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

#include "dlp/dlp_build.h"
#include "units/unit.h"

namespace ORNL {
namespace {
class DlpImageView : public QGraphicsView {
   public:
    using QGraphicsView::QGraphicsView;
    bool inspecting = false;
    std::function<void(QPointF)> inspectAt;
    std::function<void(QPoint)> pan;
    std::function<void()> inspectionResized;

   protected:
    void resizeEvent(QResizeEvent* event) override {
        QGraphicsView::resizeEvent(event);
        if (inspecting) {
            if (inspectionResized) inspectionResized();
        }
        else
            fitInView(sceneRect(), Qt::KeepAspectRatio);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            m_drag_position = event->position().toPoint();
            setFocus();
            if (inspecting)
                setCursor(Qt::ClosedHandCursor);
            else if (inspectAt)
                inspectAt(mapToScene(m_drag_position));
            event->accept();
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (inspecting && (event->buttons() & Qt::LeftButton)) {
            const QPoint position = event->position().toPoint();
            if (pan) pan(m_drag_position - position);
            m_drag_position = position;
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (inspecting && event->button() == Qt::LeftButton) {
            setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (inspecting) {
            QPoint offset;
            switch (event->key()) {
                case Qt::Key_Left:
                    offset.setX(-1);
                    break;
                case Qt::Key_Right:
                    offset.setX(1);
                    break;
                case Qt::Key_Up:
                    offset.setY(-1);
                    break;
                case Qt::Key_Down:
                    offset.setY(1);
                    break;
                default:
                    QGraphicsView::keyPressEvent(event);
                    return;
            }
            if (pan) pan(offset);
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

   private:
    QPoint m_drag_position;
};
}  // namespace

DlpPreview::DlpPreview(QWidget* parent) : QWidget(parent) {
    resize(900, 650);
    setMinimumSize(760, 360);
    m_layer_selector = new QComboBox(this);
    m_layer_selector->setObjectName("dlpLayerSelector");
    m_layer_selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_lane_selector = new QComboBox(this);
    m_lane_selector->setObjectName("dlpLaneSelector");
    m_lane_selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_lane_borders = new QCheckBox(tr("Show lane borders"), this);
    m_lane_borders->setObjectName("dlpLaneBorders");
    m_lane_borders->setToolTip(
        tr("Solid lines mark exposure boundaries. A dashed line marks the shifted final-lane footprint."));
    m_lane_borders->setChecked(true);
    m_inspect = new QCheckBox(tr("Inspect pixels (1:1)"), this);
    m_inspect->setObjectName("dlpPixelInspection");
    m_inspect->setToolTip(
        tr("View a bounded area of a lane at its original pixel resolution. "
           "Click the lane overview to choose an area; drag to pan and use arrow keys for one-pixel steps."));
    m_export = new QPushButton(tr("Export DLP Build..."), this);
    m_export->setObjectName("dlpExport");

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(tr("Layer:"), this));
    controls->addWidget(m_layer_selector);
    controls->addWidget(new QLabel(tr("Image:"), this));
    controls->addWidget(m_lane_selector);
    controls->addWidget(m_lane_borders);
    controls->addStretch();
    controls->addWidget(m_export);
    auto* inspection_controls = new QHBoxLayout;
    inspection_controls->addWidget(m_inspect);
    auto* inspection_hint = new QLabel(tr("Click a lane overview to inspect; drag or use arrow keys to pan."), this);
    inspection_hint->setWordWrap(true);
    inspection_controls->addWidget(inspection_hint, 1);

    m_scene = new QGraphicsScene(this);
    m_view  = new DlpImageView(m_scene, this);
    m_view->setObjectName("dlpImageView");
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* image_view      = static_cast<DlpImageView*>(m_view);
    image_view->inspectAt = [this](const QPointF& point) {
        if (m_lane_image.isNull() || point.x() < 0 || point.y() < 0 || point.x() >= m_lane_image.width() ||
            point.y() >= m_lane_image.height())
            return;
        m_inspection_center = QPoint(int(std::floor(point.x())), int(std::floor(point.y())));
        m_inspect->setChecked(true);
    };
    image_view->pan               = [this](const QPoint& offset) { panInspection(offset); };
    image_view->inspectionResized = [this] { showInspection(); };
    m_status                      = new QLabel(this);
    m_status->setObjectName("dlpStatus");
    m_status->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addLayout(inspection_controls);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_status);

    connect(m_layer_selector, &QComboBox::currentIndexChanged, this, &DlpPreview::updateImage);
    connect(m_lane_selector, &QComboBox::currentIndexChanged, this, &DlpPreview::updateImage);
    connect(m_lane_borders, &QCheckBox::toggled, this, [this] {
        if (!m_inspect->isChecked()) showOverview();
    });
    connect(m_inspect, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked)
            showInspection();
        else
            showOverview();
    });
    connect(m_export, &QPushButton::clicked, this, &DlpPreview::exportRequested);
    clear();
}

void DlpPreview::setBuild(QSharedPointer<const DlpBuild> build) {
    clear();
    if (!build || build->layers.isEmpty()) return;

    m_build = std::move(build);
    if (m_build->settings.interlaced_stitching) {
        m_lane_borders->setText(tr("Show stitching bands"));
        m_lane_borders->setToolTip(
            tr("Shaded bands mark interlacing areas. Inspect numbered lanes at 1:1 for the exact finger pattern."));
    }
    const QSignalBlocker layer_blocker(m_layer_selector);
    const QSignalBlocker lane_blocker(m_lane_selector);
    for (const auto& layer : m_build->layers) { m_layer_selector->addItem(QString::number(layer.number)); }
    m_lane_selector->addItem(tr("Entire layer (diagnostic only)"));
    m_lane_selector->setItemData(0, tr("Diagnostic overview only. Project the numbered lane images."), Qt::ToolTipRole);
    for (int lane = 1; lane <= m_build->lane_count; ++lane) { m_lane_selector->addItem(tr("Lane %1").arg(lane)); }
    m_layer_selector->setEnabled(true);
    m_lane_selector->setEnabled(true);
    m_export->setEnabled(true);
    updateImage();
}

void DlpPreview::clear() {
    m_build.clear();
    const QSignalBlocker layer_blocker(m_layer_selector);
    const QSignalBlocker lane_blocker(m_lane_selector);
    m_layer_selector->clear();
    m_lane_selector->clear();
    m_layer_selector->setEnabled(false);
    m_lane_selector->setEnabled(false);
    m_lane_borders->setEnabled(false);
    m_lane_borders->setText(tr("Show lane borders"));
    m_lane_borders->setToolTip(
        tr("Solid lines mark exposure boundaries. A dashed line marks the shifted final-lane footprint."));
    m_export->setEnabled(false);
    const QSignalBlocker inspection_blocker(m_inspect);
    m_inspect->setChecked(false);
    m_inspect->setEnabled(false);
    static_cast<DlpImageView*>(m_view)->inspecting = false;
    m_view->unsetCursor();
    m_overview_image = QImage();
    m_lane_image     = QImage();
    m_image_status.clear();
    m_scene->clear();
    m_scene->setSceneRect(QRectF());
    m_status->setText(tr("Slice a build in DLP mode to preview its images."));
}

void DlpPreview::updateImage() {
    if (!m_build) return;

    const auto& layer = m_build->layers[m_layer_selector->currentIndex()];
    const int lane    = m_lane_selector->currentIndex() - 1;
    const bool full   = lane < 0;
    const auto path   = full ? layer.image_path : layer.lanes[lane].image_path;
    const bool empty  = full ? layer.empty : layer.lanes[lane].empty;
    m_lane_borders->setEnabled(full && (!m_build->settings.interlaced_stitching || m_build->lane_count > 1));
    m_inspect->setEnabled(false);
    if (full) {
        const QSignalBlocker blocker(m_inspect);
        m_inspect->setChecked(false);
    }
    m_scene->clear();
    m_lane_image     = QImage();
    m_overview_image = QImage();
    const QSize expected_size(full ? m_build->image_width : m_build->settings.lane_pixel_width,
                              full ? m_build->image_height : m_build->lane_image_height);
    const int bit_depth = m_build->settings.bit_depth;
    const auto format   = bit_depth == 1   ? QImage::Format_Mono
                          : bit_depth == 8 ? QImage::Format_Indexed8
                                           : QImage::Format_Invalid;
    QImageReader reader(path, "bmp");
    QImage image;
    if (!expected_size.isEmpty() && format != QImage::Format_Invalid && reader.size() == expected_size &&
        reader.imageFormat() == format) {
        const qint64 row_bytes = ((qint64(expected_size.width()) * bit_depth + 31) / 32) * 4;
        const int limit        = QImageReader::allocationLimit();
        if (limit <= 0 || row_bytes * expected_size.height() <= qint64(limit) * 1024 * 1024) {
            // Reuse a validated native-depth buffer instead of Qt's conservative 32-bit allocation estimate.
            image = QImage(expected_size, format);
            if (!image.isNull() && !reader.read(&image)) image = QImage();
        }
    }
    m_overview_image = image.width() > 2048 || image.height() > 2048
                           ? image.scaled(2048, 2048, Qt::KeepAspectRatio, Qt::FastTransformation)
                           : image;
    if (m_overview_image.isNull()) {
        const QSignalBlocker blocker(m_inspect);
        m_inspect->setChecked(false);
        static_cast<DlpImageView*>(m_view)->inspecting = false;
        m_view->unsetCursor();
        m_scene->setSceneRect(QRectF());
        m_status->setText(tr("Could not load DLP image: %1").arg(path));
        return;
    }
    if (!full) {
        m_lane_image = image;
        m_inspect->setEnabled(true);
        if (!m_inspect->isChecked()) m_inspection_center = QPoint(image.width() / 2, image.height() / 2);
    }
    QString status = tr("Layer %1 | Z = %2 mm | %3 | %4")
                         .arg(layer.number)
                         .arg(Distance(layer.z).to(mm), 0, 'f', 3)
                         .arg(full ? tr("Entire layer (diagnostic only; do not project)")
                                   : tr("Lane %1").arg(layer.lanes[lane].number))
                         .arg(empty ? tr("Empty") : tr("Contains geometry"));
    status += tr("\n%1 | %2 x %3 px | %4 x %5 mm | Pixel pitch = %6 mm | %7-bit")
                  .arg(m_build->settings.output_mode == DlpOutputMode::kStatic ? tr("Static") : tr("Scrolling"))
                  .arg(image.width())
                  .arg(image.height())
                  .arg(Distance(image.width() * m_build->pixel_pitch).to(mm), 0, 'f', 3)
                  .arg(Distance(image.height() * m_build->pixel_pitch).to(mm), 0, 'f', 3)
                  .arg(Distance(m_build->pixel_pitch).to(mm), 0, 'g', 8)
                  .arg(m_build->settings.bit_depth);
    if (!full) {
        const int start = m_build->lane_starts[lane];
        const int overlap =
            m_build->settings.interlaced_stitching
                ? (lane == 0 ? 0 : m_build->lane_starts[lane - 1] + m_build->settings.lane_pixel_width - start)
                : lane * m_build->settings.lane_pixel_width - start;
        status +=
            (m_build->settings.interlaced_stitching ? tr("\nX origin = %1 mm | Left footprint overlap = %2 px (%3 mm)")
                                                    : tr("\nX origin = %1 mm | Left overlap masked = %2 px (%3 mm)"))
                .arg(Distance(m_build->settings.x_min + start * m_build->pixel_pitch).to(mm), 0, 'f', 3)
                .arg(overlap)
                .arg(Distance(overlap * m_build->pixel_pitch).to(mm), 0, 'f', 3);
    }
    else if (!m_build->settings.interlaced_stitching &&
             m_build->lane_starts.last() != (m_build->lane_count - 1) * m_build->settings.lane_pixel_width) {
        status += tr("\nSolid: exposure boundaries; dashed: shifted final-lane footprint.");
    }
    if (m_build->settings.interlaced_stitching) {
        if (m_build->lane_count == 1)
            status += tr("\nInterlaced stitching enabled; this single-lane build has no seam.");
        else {
            status += tr("\nInterlaced band and finger height: %1 px (%2 mm); pattern alternates each layer.")
                          .arg(m_build->stitch_overlap_pixels)
                          .arg(Distance(m_build->stitch_overlap_pixels * m_build->pixel_pitch).to(mm), 0, 'g', 6);
            if (full) status += tr("\nShaded: interlacing bands; inspect numbered lanes at 1:1 for the exact pattern.");
        }
    }
    m_image_status = status;
    if (m_inspect->isChecked())
        showInspection();
    else
        showOverview();
}

void DlpPreview::showOverview() {
    if (!m_build || m_overview_image.isNull()) return;
    auto* view       = static_cast<DlpImageView*>(m_view);
    view->inspecting = false;
    view->unsetCursor();
    m_scene->clear();
    const bool full = m_lane_selector->currentIndex() == 0;
    const QSize size(full ? m_build->image_width : m_build->settings.lane_pixel_width,
                     full ? m_build->image_height : m_build->lane_image_height);
    auto* pixels = m_scene->addPixmap(QPixmap::fromImage(m_overview_image));
    pixels->setTransform(QTransform::fromScale(double(size.width()) / m_overview_image.width(),
                                               double(size.height()) / m_overview_image.height()));
    const QRectF image_rect(QPointF(), size);
    QPen frame(QColor(128, 128, 128));
    frame.setCosmetic(true);
    m_scene->addRect(image_rect, frame);
    if (full && m_lane_borders->isChecked()) {
        QPen border(QColor(255, 140, 0), 2);
        border.setCosmetic(true);
        if (m_build->settings.interlaced_stitching) {
            border.setStyle(Qt::DashLine);
            for (int start : m_build->stitch_band_starts) {
                m_scene->addRect(QRectF(start, 0, m_build->stitch_overlap_pixels, size.height()), border,
                                 QBrush(QColor(255, 140, 0, 35)));
            }
        }
        else {
            for (int i = 1; i < m_build->lane_count; ++i) {
                const int x = i * m_build->settings.lane_pixel_width;
                m_scene->addLine(x, 0, x, size.height(), border);
            }
            const int final_start = m_build->lane_starts.last();
            if (final_start != (m_build->lane_count - 1) * m_build->settings.lane_pixel_width) {
                border.setStyle(Qt::DashLine);
                m_scene->addLine(final_start, 0, final_start, size.height(), border);
            }
        }
    }
    m_scene->setSceneRect(image_rect);
    m_view->fitInView(image_rect, Qt::KeepAspectRatio);
    QString status = m_image_status;
    if (m_overview_image.size() != size)
        status += tr(" | Preview: %1 x %2 px").arg(m_overview_image.width()).arg(m_overview_image.height());
    m_status->setText(status);
}

void DlpPreview::showInspection() {
    if (m_lane_image.isNull() || !m_inspect->isChecked()) return;
    auto* view = static_cast<DlpImageView*>(m_view);
    if (!view->inspecting) view->setCursor(Qt::OpenHandCursor);
    view->inspecting = true;
    const int width  = std::min({m_lane_image.width(), 1024, std::max(1, m_view->viewport()->width() - 6)});
    const int height = std::min({m_lane_image.height(), 1024, std::max(1, m_view->viewport()->height() - 6)});
    const int x      = std::clamp(m_inspection_center.x() - width / 2, 0, m_lane_image.width() - width);
    const int y      = std::clamp(m_inspection_center.y() - height / 2, 0, m_lane_image.height() - height);
    const QRect region(x, y, width, height);
    m_inspection_center = region.topLeft() + QPoint(width / 2, height / 2);
    m_scene->clear();
    auto* pixels = m_scene->addPixmap(QPixmap::fromImage(m_lane_image.copy(region)));
    pixels->setPos(region.topLeft());
    QPen frame(QColor(255, 140, 0));
    frame.setCosmetic(true);
    m_scene->addRect(QRectF(region).adjusted(-1, -1, 1, 1), frame);
    m_scene->setSceneRect(QRectF(region).adjusted(-2, -2, 2, 2));
    m_view->resetTransform();
    m_view->centerOn(QRectF(region).center());
    m_status->setText(m_image_status +
                      tr("\n1:1 image pixels (from top-left): X %1–%2, Y %3–%4 | %5 x %6 px. "
                         "Drag to pan; arrow keys move one pixel. Uncheck inspection for the overview.")
                          .arg(region.left())
                          .arg(region.right())
                          .arg(region.top())
                          .arg(region.bottom())
                          .arg(width)
                          .arg(height));
}

void DlpPreview::panInspection(const QPoint& offset) {
    if (!m_inspect->isChecked() || m_lane_image.isNull()) return;
    m_inspection_center += offset;
    showInspection();
}
}  // namespace ORNL
