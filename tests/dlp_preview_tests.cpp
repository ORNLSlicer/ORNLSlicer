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
#include <QLabel>
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
    auto* export_button = preview->findChild<QPushButton*>("dlpExport");
    auto* view          = preview->findChild<QGraphicsView*>("dlpImageView");
    auto* status        = preview->findChild<QLabel*>("dlpStatus");
    if (!ORNL::Testing::expect(layers && lanes && borders && export_button && view && status,
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
        expect(!layers->isEnabled() && !lanes->isEnabled() && !export_button->isEnabled() &&
                   view->scene()->items().isEmpty(),
               "Clearing the preview must disable selection/export and remove previous images.");
        screenshot(prefix + "before");

        preview->setBuild(build);
        app.processEvents();
        expect(layers->count() == 2 && lanes->count() == 4 && export_button->isEnabled(),
               "Completed builds must expose every layer and lane and enable export.");
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
        borders->setChecked(false);
        expect(laneBorders(view).isEmpty() && laneBorders(view, Qt::DashLine).isEmpty(),
               "The lane-border toggle must hide both exposure and footprint borders.");
        borders->setChecked(true);

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
        expect(QImageReader::allocationLimit() == 256, "DLP previews must not raise the global image reader limit.");

        auto mismatched = QSharedPointer<ORNL::DlpBuild>::create(*build);
        mismatched->image_width += 1;
        preview->setBuild(mismatched);
        expect(view->scene()->items().isEmpty() && status->text().contains("Could not load DLP image"),
               "A BMP whose dimensions differ from its cached build must be rejected before allocating its raster.");
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
