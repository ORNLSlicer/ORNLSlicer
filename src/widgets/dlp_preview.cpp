#include "widgets/dlp_preview.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <utility>

#include "dlp/dlp_build.h"
#include "units/unit.h"

namespace ORNL {
namespace {
class DlpImageView : public QGraphicsView {
   public:
    using QGraphicsView::QGraphicsView;

   protected:
    void resizeEvent(QResizeEvent* event) override {
        QGraphicsView::resizeEvent(event);
        fitInView(sceneRect(), Qt::KeepAspectRatio);
    }
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
    m_lane_borders->setChecked(true);
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

    m_scene = new QGraphicsScene(this);
    m_view  = new DlpImageView(m_scene, this);
    m_view->setObjectName("dlpImageView");
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_status = new QLabel(this);
    m_status->setObjectName("dlpStatus");
    m_status->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_status);

    connect(m_layer_selector, &QComboBox::currentIndexChanged, this, &DlpPreview::updateImage);
    connect(m_lane_selector, &QComboBox::currentIndexChanged, this, &DlpPreview::updateImage);
    connect(m_lane_borders, &QCheckBox::toggled, this, &DlpPreview::updateImage);
    connect(m_export, &QPushButton::clicked, this, &DlpPreview::exportRequested);
    clear();
}

void DlpPreview::setBuild(QSharedPointer<const DlpBuild> build) {
    clear();
    if (!build || build->layers.isEmpty()) return;

    m_build = std::move(build);
    const QSignalBlocker layer_blocker(m_layer_selector);
    const QSignalBlocker lane_blocker(m_lane_selector);
    for (const auto& layer : m_build->layers) { m_layer_selector->addItem(QString::number(layer.number)); }
    m_lane_selector->addItem(tr("Entire layer"));
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
    m_export->setEnabled(false);
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
    m_lane_borders->setEnabled(full);
    m_scene->clear();
    const QImage image(path);
    if (image.isNull()) {
        m_scene->setSceneRect(QRectF());
        m_status->setText(tr("Could not load DLP image: %1").arg(path));
        return;
    }

    m_scene->addPixmap(QPixmap::fromImage(image));
    const QRectF image_rect(0, 0, image.width(), image.height());
    QPen frame(QColor(128, 128, 128));
    frame.setCosmetic(true);
    m_scene->addRect(image_rect, frame);
    if (full && m_lane_borders->isChecked()) {
        QPen border(QColor(255, 140, 0), 2);
        border.setCosmetic(true);
        for (int i = 1; i < m_build->lane_count; ++i) {
            const int x = i * m_build->settings.lane_pixel_width;
            m_scene->addLine(x, 0, x, image.height(), border);
        }
    }
    m_scene->setSceneRect(image_rect);
    m_view->fitInView(image_rect, Qt::KeepAspectRatio);
    m_status->setText(tr("Layer %1 | Z = %2 mm | %3 | %4")
                          .arg(layer.number)
                          .arg(Distance(layer.z).to(mm), 0, 'f', 3)
                          .arg(full ? tr("Entire layer") : tr("Lane %1").arg(layer.lanes[lane].number))
                          .arg(empty ? tr("Empty") : tr("Contains geometry")));
}
}  // namespace ORNL
