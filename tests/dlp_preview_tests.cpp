#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionComboBox>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "dlp/dlp_build.h"
#include "managers/preferences_manager.h"
#include "test_utils.h"
#include "units/unit.h"
#include "utilities/theme_tool.h"
#include "widgets/dlp_preview.h"

namespace {
QImage displayedImage(QGraphicsView* view) {
    for (auto* item : view->scene()->items()) {
        if (auto* pixmap = qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) { return pixmap->pixmap().toImage(); }
    }
    return {};
}

QRect displayedRegion(QGraphicsView* view) {
    for (auto* item : view->scene()->items()) {
        if (auto* pixels = qgraphicsitem_cast<QGraphicsPixmapItem*>(item)) return pixels->sceneBoundingRect().toRect();
    }
    return {};
}

void clickImage(QGraphicsView* view, const QPointF& point) {
    const QPoint position = view->mapFromScene(point);
    const QPoint global   = view->viewport()->mapToGlobal(position);
    QMouseEvent press(QEvent::MouseButtonPress, position, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, position, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &release);
    QApplication::processEvents();
}

void dragImage(QGraphicsView* view, const QPoint& offset) {
    const QPoint position = view->viewport()->rect().center();
    const QPoint target   = position - offset;
    QMouseEvent press(QEvent::MouseButtonPress, position, view->viewport()->mapToGlobal(position), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &press);
    QMouseEvent move(QEvent::MouseMove, target, view->viewport()->mapToGlobal(target), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &move);
    QMouseEvent release(QEvent::MouseButtonRelease, target, view->viewport()->mapToGlobal(target), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &release);
    QApplication::processEvents();
}

QVector<int> laneBorders(QGraphicsView* view, Qt::PenStyle style = Qt::SolidLine) {
    QVector<int> positions;
    for (auto* item : view->scene()->items()) {
        if (auto* line = qgraphicsitem_cast<QGraphicsLineItem*>(item); line && line->pen().style() == style) {
            positions.append(line->line().x1());
        }
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

QColor renderedColor(const QImage& capture, QGraphicsView* view, QWidget* preview, const QPointF& scene_point) {
    const QPoint position = view->viewport()->mapTo(preview, view->mapFromScene(scene_point));
    return capture.pixelColor(position * capture.devicePixelRatio());
}

bool saveFixture(const QImage& image, const QString& path, int bit_depth) {
    if (bit_depth == 1)
        return image.convertToFormat(QImage::Format_Mono, {qRgb(0, 0, 0), qRgb(255, 255, 255)}, Qt::ThresholdDither)
            .save(path, "BMP");
    QVector<QRgb> colors;
    for (int i = 0; i < 256; ++i) colors.append(qRgb(i, i, i));
    return image.convertToFormat(QImage::Format_Indexed8, colors, Qt::ThresholdDither).save(path, "BMP");
}

QByteArray fileDigest(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) ? hash.result() : QByteArray();
}
}  // namespace

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("fusion"));
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    const int previous_limit = QImageReader::allocationLimit();
    QImageReader::setAllocationLimit(256);

    bool passed       = ORNL::Theme::initializeFonts();
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    auto build     = QSharedPointer<ORNL::DlpBuild>::create();
    build->storage = QSharedPointer<QTemporaryDir>::create();
    if (!ORNL::Testing::expect(build->storage->isValid(), "Could not create DLP preview fixture directory."))
        return EXIT_FAILURE;
    build->image_width               = 250;
    build->image_height              = 140;
    build->lane_image_height         = 160;
    build->lane_count                = 3;
    build->lane_starts               = {0, 100, 150};
    build->pixel_pitch               = 100;
    build->settings.x_min            = 2000;
    build->settings.projection_width = 10000;
    build->settings.lane_pixel_width = 100;
    build->settings.bit_depth        = 8;
    QImage full(250, 140, QImage::Format_RGB32);
    full.fill(Qt::black);
    {
        QPainter painter(&full);
        painter.fillRect(30, 20, 180, 100, Qt::white);
        painter.fillRect(70, 50, 70, 40, Qt::black);
    }
    full.setPixelColor(210, 60, QColor(96, 96, 96));
    for (int number = 1; number <= 2; ++number) {
        ORNL::DlpLayer layer;
        layer.number     = number;
        layer.z          = number * 0.1 * ORNL::mm();
        layer.empty      = number == 2;
        layer.image_path = build->storage->filePath(QString("layer%1.bmp").arg(number));
        if (layer.empty) full.fill(Qt::black);
        expect(saveFixture(full, layer.image_path, 8), "Could not save full DLP preview fixture.");
        for (int lane = 1; lane <= 3; ++lane) {
            QImage image(100, build->lane_image_height, QImage::Format_RGB32);
            image.fill(Qt::black);
            QPainter painter(&image);
            const int source_start = (lane - 1) * 100;
            const int overlap      = source_start - build->lane_starts[lane - 1];
            painter.drawImage(overlap, 0, full.copy(source_start, 0, std::min(100, 250 - source_start), 140));
            painter.end();
            const auto path = build->storage->filePath(QString("layer%1_lane%2.bmp").arg(number).arg(lane));
            expect(saveFixture(image, path, 8), "Could not save lane DLP preview fixture.");
            layer.lanes.append({lane, path, layer.empty});
        }
        build->layers.append(layer);
    }

    auto scrolling                  = QSharedPointer<ORNL::DlpBuild>::create(*build);
    scrolling->settings.output_mode = ORNL::DlpOutputMode::kScrolling;
    scrolling->settings.bit_depth   = 1;
    for (auto& layer : scrolling->layers) {
        const QString path = build->storage->filePath(QString("scrolling%1.bmp").arg(layer.number));
        expect(saveFixture(QImage(layer.image_path), path, 1), "Could not save 1-bit full preview fixture.");
        layer.image_path = path;
        for (auto& lane : layer.lanes) {
            const QString lane_path =
                build->storage->filePath(QString("scrolling%1_lane%2.bmp").arg(layer.number).arg(lane.number));
            expect(saveFixture(QImage(lane.image_path), lane_path, 1), "Could not save 1-bit lane preview fixture.");
            lane.image_path = lane_path;
        }
    }

    auto large                       = QSharedPointer<ORNL::DlpBuild>::create();
    large->storage                   = build->storage;
    large->image_width               = 16384;
    large->image_height              = 12288;
    large->lane_image_height         = 13056;
    large->lane_count                = 4;
    large->lane_starts               = {0, 4096, 8192, 12288};
    large->settings.lane_pixel_width = 4096;
    large->settings.projection_width = 127000;
    large->settings.output_mode      = ORNL::DlpOutputMode::kScrolling;
    large->settings.bit_depth        = 1;
    large->pixel_pitch               = 127000.0 / 4096;
    ORNL::DlpLayer large_layer;
    large_layer.number     = 1;
    large_layer.z          = 50;
    large_layer.empty      = false;
    large_layer.image_path = build->storage->filePath("large-full.bmp");
    {
        QImage image(16384, 12288, QImage::Format_Mono);
        image.setColorTable({qRgb(0, 0, 0), qRgb(255, 255, 255)});
        image.fill(0);
        for (int y = 1024; y < 11264; ++y) std::memset(image.scanLine(y) + 128, 255, 1792);
        for (int y = 4096; y < 8192; ++y) std::memset(image.scanLine(y) + 768, 0, 512);
        image.setPixel(13313, 2049, 0);
        image.setPixel(15789, 513, 1);
        expect(image.save(large_layer.image_path, "BMP"), "Could not save full-resolution 1-bit preview fixture.");
        for (int lane = 0; lane < 4; ++lane) {
            QImage lane_image(4096, 13056, QImage::Format_Mono);
            lane_image.setColorTable(image.colorTable());
            lane_image.fill(0);
            for (int y = 0; y < image.height(); ++y)
                std::memcpy(lane_image.scanLine(y), image.constScanLine(y) + lane * 512, 512);
            const QString path = build->storage->filePath(QString("large-lane%1.bmp").arg(lane + 1));
            expect(lane_image.save(path, "BMP"), "Could not save full-resolution 1-bit lane fixture.");
            large_layer.lanes.append({lane + 1, path, false});
        }
    }
    large->layers.append(large_layer);
    const QByteArray full_digest = fileDigest(large_layer.image_path);
    const QByteArray lane_digest = fileDigest(large_layer.lanes.last().image_path);

    QWidget root;
    auto* preview = new ORNL::DlpPreview(&root);
    preview->setWindowFlag(Qt::Window);
    auto* layers        = preview->findChild<QComboBox*>("dlpLayerSelector");
    auto* lanes         = preview->findChild<QComboBox*>("dlpLaneSelector");
    auto* borders       = preview->findChild<QCheckBox*>("dlpLaneBorders");
    auto* inspection    = preview->findChild<QCheckBox*>("dlpPixelInspection");
    auto* export_button = preview->findChild<QPushButton*>("dlpExport");
    auto* view          = preview->findChild<QGraphicsView*>("dlpImageView");
    auto* status        = preview->findChild<QLabel*>("dlpStatus");
    if (!ORNL::Testing::expect(layers && lanes && borders && inspection && export_button && view && status,
                               "DLP preview controls must be available."))
        return EXIT_FAILURE;
    int export_requests = 0;
    QObject::connect(preview, &ORNL::DlpPreview::exportRequested, [&]() { ++export_requests; });
    const auto preferences = ORNL::PreferencesManager::getInstance();
    const auto screenshot  = [&](const QString& name) {
        const QImage capture = preview->grab().toImage();
        if (argc > 1) {
            expect(capture.save(QDir(QString::fromUtf8(argv[1])).filePath(name + ".png")),
                   "Could not save DLP preview screenshot.");
        }
        return capture;
    };
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode}) {
        preferences->setTheme(theme);
        QApplication::setPalette(preferences->getTheme().getPalette());
        QFile style(preferences->getTheme().getFolderPath() + "style.qss");
        if (!ORNL::Testing::expect(style.open(QIODevice::ReadOnly), "Could not read the root application stylesheet."))
            return EXIT_FAILURE;
        root.setStyleSheet(style.readAll());
        const auto prefix = theme == ORNL::ThemeName::kLightMode ? QString("light-") : QString("dark-");
        preview->clear();
        preview->show();
        app.processEvents();
        expect(!layers->isEnabled() && !lanes->isEnabled() && !inspection->isEnabled() && !export_button->isEnabled() &&
                   view->scene()->items().isEmpty(),
               "Clearing the preview must disable selection/export and remove previous images.");
        screenshot(prefix + "before");

        preview->setBuild(build);
        app.processEvents();
        expect(layers->count() == 2 && lanes->count() == 4 && export_button->isEnabled(),
               "Completed builds must expose every layer and lane and enable export.");
        expect(lanes->currentText().contains("diagnostic only") && !inspection->isEnabled() &&
                   status->text().contains("do not project"),
               "Full-layer images must be marked diagnostic and cannot enter lane pixel inspection.");
        QStyleOptionComboBox lane_style;
        lane_style.initFrom(lanes);
        lane_style.currentText = lanes->currentText();
        const auto text_rect =
            lanes->style()->subControlRect(QStyle::CC_ComboBox, &lane_style, QStyle::SC_ComboBoxEditField, lanes);
        expect(text_rect.width() >= lanes->fontMetrics().horizontalAdvance(lanes->currentText()),
               "Image selection must show the complete label when a build arrives after the empty preview is shown.");
        expect(displayedImage(view).convertToFormat(QImage::Format_RGB32) ==
                   QImage(build->layers[0].image_path).convertToFormat(QImage::Format_RGB32),
               "Whole-layer preview must preserve the exported image pixels and orientation.");
        expect(laneBorders(view) == QVector<int>({100, 200}), "Lane borders must match exported pixel boundaries.");
        expect(laneBorders(view, Qt::DashLine) == QVector<int>({150}),
               "The shifted final footprint must be distinct from exposure boundaries.");
        expect(status->text().contains("Static") && status->text().contains("250 x 140 px") &&
                   status->text().contains("25.000 x 14.000 mm") && status->text().contains("Pixel pitch = 0.1 mm") &&
                   status->text().contains("8-bit") && status->text().contains("dashed: shifted") &&
                   !status->text().contains("Preview:"),
               "The full-layer status must show the cached format, dimensions, physical size, pitch, depth, and "
               "overlay legend.");
        expect(std::abs(view->transform().m11() - view->transform().m22()) < 1e-8,
               "The preview must preserve the physical aspect ratio of square pixels.");
        const auto whole_capture = screenshot(prefix + "whole-layer");
        expect(renderedColor(whole_capture, view, preview, QPointF(40.5, 40.5)) == QColor(Qt::white) &&
                   renderedColor(whole_capture, view, preview, QPointF(110.5, 70.5)) == QColor(Qt::black) &&
                   renderedColor(whole_capture, view, preview, QPointF(10.5, 10.5)) == QColor(Qt::black),
               "The rendered whole layer must show solid geometry, its hole, and the empty background in both themes.");
        const QString hidden_full = build->layers[0].image_path + ".hidden";
        const bool hidden         = QFile::rename(build->layers[0].image_path, hidden_full);
        expect(hidden, "Could not temporarily hide the image for cached-overlay verification.");
        borders->setChecked(false);
        expect(laneBorders(view).isEmpty() && laneBorders(view, Qt::DashLine).isEmpty(),
               "The lane-border toggle must hide both exposure and footprint borders.");
        borders->setChecked(true);
        expect(!displayedImage(view).isNull() && laneBorders(view) == QVector<int>({100, 200}) &&
                   !status->text().contains("Could not load"),
               "Changing border visibility must use the bounded cached overview without reopening the BMP.");
        if (hidden)
            expect(QFile::rename(hidden_full, build->layers[0].image_path), "Could not restore the preview fixture.");

        lanes->setCurrentIndex(3);
        app.processEvents();
        expect(displayedImage(view).convertToFormat(QImage::Format_RGB32) ==
                       QImage(build->layers[0].lanes[2].image_path).convertToFormat(QImage::Format_RGB32) &&
                   displayedImage(view).size() == QSize(100, 160),
               "Selecting the final lane must show its complete fixed-width exported image, including padding.");
        expect(!borders->isEnabled() && laneBorders(view).isEmpty() && laneBorders(view, Qt::DashLine).isEmpty(),
               "Lane images must not show whole-layer borders.");
        expect(status->text().contains("100 x 160 px") && status->text().contains("10.000 x 16.000 mm") &&
                   status->text().contains("X origin = 17.000 mm") &&
                   status->text().contains("Left overlap masked = 50 px (5.000 mm)"),
               "Lane status must report its padded physical footprint, shifted origin, and masked overlap.");
        const auto lane_capture = screenshot(prefix + "last-lane");
        expect(renderedColor(lane_capture, view, preview, QPointF(5.5, 40.5)) == QColor(Qt::black) &&
                   renderedColor(lane_capture, view, preview, QPointF(55.5, 40.5)) == QColor(Qt::white) &&
                   renderedColor(lane_capture, view, preview, QPointF(75.5, 40.5)) == QColor(Qt::black) &&
                   renderedColor(lane_capture, view, preview, QPointF(55.5, 150.5)) == QColor(Qt::black),
               "The rendered final lane must show a masked left overlap, right-hand exposure, and black padding in "
               "both themes.");
        inspection->setChecked(true);
        app.processEvents();
        expect(inspection->isEnabled() && view->transform().isIdentity() &&
                   displayedRegion(view) == QRect(0, 0, 100, 160) &&
                   displayedImage(view).pixelColor(60, 60) == QColor(96, 96, 96) &&
                   status->text().contains("1:1 image pixels (from top-left)") && !status->text().contains("Preview:"),
               "Native 8-bit inspection must retain single-pixel grayscale values without scaling or overlays.");
        const auto small_inspection = screenshot(prefix + "last-lane-inspection");
        expect(renderedColor(small_inspection, view, preview, QPointF(60, 60)) == QColor(96, 96, 96),
               "The actual 1:1 view must render its single-pixel grayscale value unchanged in both themes.");
        layers->setCurrentIndex(1);
        expect(inspection->isChecked() && status->text().contains("Empty") &&
                   displayedImage(view).pixelColor(60, 60) == QColor(Qt::black),
               "Changing layers in inspection must replace the native pixels and retain the inspection mode.");
        layers->setCurrentIndex(0);
        inspection->setChecked(false);
        app.processEvents();
        layers->setCurrentIndex(1);
        expect(lanes->currentIndex() == 3 && status->text().contains("Empty") &&
                   displayedImage(view).convertToFormat(QImage::Format_RGB32) ==
                       QImage(build->layers[1].lanes[2].image_path).convertToFormat(QImage::Format_RGB32),
               "Changing layers must preserve the selected lane and identify empty images.");
        export_button->click();
        lanes->setCurrentIndex(0);
        expect(borders->isEnabled() && laneBorders(view) == QVector<int>({100, 200}) &&
                   laneBorders(view, Qt::DashLine) == QVector<int>({150}),
               "Returning to the whole layer must restore the border overlay.");
        preview->setBuild(scrolling);
        lanes->setCurrentIndex(3);
        expect(status->text().contains("Scrolling") && status->text().contains("1-bit") &&
                   status->text().contains("X origin = 17.000 mm"),
               "Preview format and output depth must come from the selected completed build snapshot.");
        screenshot(prefix + "scrolling-last-lane");

        preview->setBuild(large);
        app.processEvents();
        const QImage large_display = displayedImage(view);
        expect(!large_display.isNull() && large_display.width() <= 2048 && large_display.height() <= 2048,
               "A full layer exceeding the 32-bit reader budget must load as a bounded preview.");
        expect(
            view->sceneRect() == QRectF(0, 0, 16384, 12288) && laneBorders(view) == QVector<int>({4096, 8192, 12288}),
            "Reduced previews must retain original image coordinates and lane borders.");
        expect(status->text().contains("16384 x 12288 px") && status->text().contains("508.000 x 381.000 mm") &&
                   status->text().contains("Preview: 2048 x 1536 px"),
               "Large-image readouts must describe full-resolution exports rather than preview pixels.");
        const auto large_capture = screenshot(prefix + "four-lane-full");
        if (!large_display.isNull())
            expect(renderedColor(large_capture, view, preview, QPointF(2048.5, 2048.5)) == QColor(Qt::white) &&
                       renderedColor(large_capture, view, preview, QPointF(7000.5, 6144.5)) == QColor(Qt::black) &&
                       renderedColor(large_capture, view, preview, QPointF(512.5, 512.5)) == QColor(Qt::black),
                   "The reduced full-layer preview must preserve geometry, holes, and orientation in both themes.");
        lanes->setCurrentIndex(4);
        app.processEvents();
        const QImage large_lane = displayedImage(view);
        expect(!large_lane.isNull() && large_lane.width() <= 2048 && large_lane.height() <= 2048 &&
                   view->sceneRect() == QRectF(0, 0, 4096, 13056) && status->text().contains("4096 x 13056 px") &&
                   status->text().contains("X origin = 381.000 mm") &&
                   status->text().contains(
                       QString("Preview: %1 x %2 px").arg(large_lane.width()).arg(large_lane.height())),
               "Large lanes must use bounded pixmaps while retaining their physical footprint and source dimensions.");
        const auto large_lane_capture = screenshot(prefix + "four-lane-last");
        if (!large_lane.isNull())
            expect(renderedColor(large_lane_capture, view, preview, QPointF(1024.5, 2048.5)) == QColor(Qt::white) &&
                       renderedColor(large_lane_capture, view, preview, QPointF(3500.5, 2048.5)) == QColor(Qt::black) &&
                       renderedColor(large_lane_capture, view, preview, QPointF(1024.5, 12500.5)) == QColor(Qt::black),
                   "Reduced lane previews must retain geometry, background, and scrolling padding.");
        clickImage(view, QPointF(1025, 2049));
        const QRect native_region = displayedRegion(view);
        const QImage native_image = displayedImage(view);
        expect(inspection->isChecked() && view->transform().isIdentity() &&
                   native_region.contains(QPoint(1025, 2049)) && native_region.width() <= 1024 &&
                   native_region.height() <= 1024 &&
                   native_image.convertToFormat(QImage::Format_RGB32) == QImage(large_layer.lanes.last().image_path)
                                                                             .copy(native_region)
                                                                             .convertToFormat(QImage::Format_RGB32),
               "Clicking a lane overview must show an unscaled bounded crop of the original encoded pixels.");
        expect(native_image.pixelColor(QPoint(1025, 2049) - native_region.topLeft()) == QColor(Qt::black) &&
                   native_image.pixelColor(QPoint(1026, 2049) - native_region.topLeft()) == QColor(Qt::white),
               "Native inspection must preserve an isolated one-pixel hole and its neighboring exposure pixel.");
        const auto native_capture = screenshot(prefix + "four-lane-inspection");
        expect(renderedColor(native_capture, view, preview, QPointF(1025, 2049)) == QColor(Qt::black) &&
                   renderedColor(native_capture, view, preview, QPointF(1026, 2049)) == QColor(Qt::white),
               "One-pixel hole boundaries must remain distinguishable in the rendered native view.");
        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(view, &right);
        expect(displayedRegion(view).topLeft() == native_region.topLeft() + QPoint(1, 0),
               "Arrow keys must move inspection by exactly one original image pixel.");
        const QRect before_drag = displayedRegion(view);
        dragImage(view, QPoint(17, 11));
        expect(displayedRegion(view).topLeft() == before_drag.topLeft() + QPoint(17, 11),
               "Dragging must pan the native crop by the corresponding source pixels.");
        preview->resize(1500, 1400);
        app.processEvents();
        expect(displayedImage(view).width() <= 1024 && displayedImage(view).height() <= 1024 &&
                   view->transform().isIdentity(),
               "Even a large window must cap its native display bitmap at 1024 by 1024 pixels.");
        preview->resize(900, 650);
        app.processEvents();
        dragImage(view, QPoint(50000, 50000));
        expect(displayedRegion(view).right() == 4095 && displayedRegion(view).bottom() == 13055,
               "Inspection panning must clamp at the original image edges, including black padded rows.");
        inspection->setChecked(false);
        app.processEvents();
        expect(view->sceneRect() == QRectF(0, 0, 4096, 13056) && !view->transform().isIdentity() &&
                   displayedImage(view).width() <= 2048 && displayedImage(view).height() <= 2048,
               "Leaving native inspection must restore the bounded fitted lane overview.");
        clickImage(view, QPointF(3501, 513));
        const QRect speck_region = displayedRegion(view);
        expect(inspection->isChecked() && speck_region.contains(QPoint(3501, 513)) &&
                   displayedImage(view).pixelColor(QPoint(3501, 513) - speck_region.topLeft()) == QColor(Qt::white) &&
                   displayedImage(view).pixelColor(QPoint(3500, 513) - speck_region.topLeft()) == QColor(Qt::black),
               "Native inspection must preserve an isolated single-pixel exposure in a black region.");
        screenshot(prefix + "single-pixel-inspection");
        lanes->setCurrentIndex(0);
        expect(!inspection->isChecked() && !inspection->isEnabled() && status->text().contains("diagnostic only") &&
                   view->sceneRect() == QRectF(0, 0, 16384, 12288),
               "Selecting the diagnostic image must leave inspection and restore the full overview.");
        expect(QImageReader::allocationLimit() == 256, "DLP previews must not raise the global image reader limit.");

        auto broken_lane     = QSharedPointer<ORNL::DlpBuild>::create(*large);
        auto missing_layer   = broken_lane->layers.first();
        missing_layer.number = 2;
        missing_layer.lanes.last().image_path += ".missing";
        broken_lane->layers.append(missing_layer);
        preview->setBuild(broken_lane);
        lanes->setCurrentIndex(4);
        inspection->setChecked(true);
        layers->setCurrentIndex(1);
        expect(view->scene()->items().isEmpty() && status->text().contains("Could not load DLP image") &&
                   !inspection->isEnabled() && !inspection->isChecked(),
               "A failed lane read must discard the previous native image and disable inspection.");

        auto mismatched = QSharedPointer<ORNL::DlpBuild>::create(*build);
        mismatched->image_width += 1;
        preview->setBuild(mismatched);
        expect(view->scene()->items().isEmpty() && status->text().contains("Could not load DLP image"),
               "A BMP whose dimensions differ from its cached build must be rejected before allocating its raster.");
        expect(!inspection->isEnabled() && !inspection->isChecked(),
               "An invalid image must never enable native pixel inspection.");
        mismatched                     = QSharedPointer<ORNL::DlpBuild>::create(*build);
        mismatched->settings.bit_depth = 1;
        preview->setBuild(mismatched);
        expect(view->scene()->items().isEmpty() && status->text().contains("Could not load DLP image"),
               "A BMP whose encoding differs from its cached depth must be rejected before allocating its raster.");
    }
    expect(export_requests == 2, "The export control must request export of the cached build.");
    preview->clear();
    export_button->click();
    expect(export_requests == 2, "A cleared preview must not permit export.");
    expect(!full_digest.isEmpty() && fileDigest(large_layer.image_path) == full_digest && !lane_digest.isEmpty() &&
               fileDigest(large_layer.lanes.last().image_path) == lane_digest,
           "Preview generation must leave full-resolution exposure BMPs unchanged.");
    QImageReader::setAllocationLimit(previous_limit);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
