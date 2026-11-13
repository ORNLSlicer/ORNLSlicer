#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
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

QVector<int> laneBorders(QGraphicsView* view) {
    QVector<int> positions;
    for (auto* item : view->scene()->items()) {
        if (auto* line = qgraphicsitem_cast<QGraphicsLineItem*>(item)) { positions.append(line->line().x1()); }
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

QColor renderedColor(const QImage& capture, QGraphicsView* view, QWidget* preview, const QPointF& scene_point) {
    const QPoint position = view->viewport()->mapTo(preview, view->mapFromScene(scene_point));
    return capture.pixelColor(position * capture.devicePixelRatio());
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
    build->lane_count                = 3;
    build->pixel_pitch               = 100;
    build->settings.lane_pixel_width = 100;
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
        expect(full.save(layer.image_path, "BMP"), "Could not save full DLP preview fixture.");
        for (int lane = 1; lane <= 3; ++lane) {
            QImage image(100, 140, QImage::Format_RGB32);
            image.fill(Qt::black);
            QPainter painter(&image);
            painter.drawImage(0, 0, full.copy((lane - 1) * 100, 0, std::min(100, 250 - (lane - 1) * 100), 140));
            painter.end();
            const auto path = build->storage->filePath(QString("layer%1_lane%2.bmp").arg(number).arg(lane));
            expect(image.save(path, "BMP"), "Could not save lane DLP preview fixture.");
            layer.lanes.append({lane, path, layer.empty});
        }
        build->layers.append(layer);
    }

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
        expect(displayedImage(view) == QImage(build->layers[0].image_path),
               "Whole-layer preview must preserve the exported image pixels and orientation.");
        expect(laneBorders(view) == QVector<int>({100, 200}), "Lane borders must match exported pixel boundaries.");
        expect(std::abs(view->transform().m11() - view->transform().m22()) < 1e-8,
               "The preview must preserve the physical aspect ratio of square pixels.");
        const auto whole_capture = screenshot(prefix + "whole-layer");
        expect(renderedColor(whole_capture, view, preview, QPointF(40.5, 40.5)) == QColor(Qt::white) &&
                   renderedColor(whole_capture, view, preview, QPointF(110.5, 70.5)) == QColor(Qt::black) &&
                   renderedColor(whole_capture, view, preview, QPointF(10.5, 10.5)) == QColor(Qt::black),
               "The rendered whole layer must show solid geometry, its hole, and the empty background in both themes.");
        borders->setChecked(false);
        expect(laneBorders(view).isEmpty(), "The lane-border toggle must hide the overlay.");
        borders->setChecked(true);

        lanes->setCurrentIndex(3);
        app.processEvents();
        expect(
            displayedImage(view) == QImage(build->layers[0].lanes[2].image_path) && displayedImage(view).width() == 100,
            "Selecting the final lane must show its complete fixed-width exported image, including padding.");
        expect(!borders->isEnabled() && laneBorders(view).isEmpty(), "Lane images must not show whole-layer borders.");
        const auto lane_capture = screenshot(prefix + "last-lane");
        expect(renderedColor(lane_capture, view, preview, QPointF(5.5, 40.5)) == QColor(Qt::white) &&
                   renderedColor(lane_capture, view, preview, QPointF(75.5, 40.5)) == QColor(Qt::black),
               "The rendered final lane must show its geometry and black padding in both themes.");
        layers->setCurrentIndex(1);
        expect(lanes->currentIndex() == 3 && status->text().contains("Empty") &&
                   displayedImage(view) == QImage(build->layers[1].lanes[2].image_path),
               "Changing layers must preserve the selected lane and identify empty images.");
        export_button->click();
        lanes->setCurrentIndex(0);
        expect(borders->isEnabled() && laneBorders(view) == QVector<int>({100, 200}),
               "Returning to the whole layer must restore the border overlay.");
    }
    expect(export_requests == 2, "The export control must request export of the cached build.");
    preview->clear();
    export_button->click();
    expect(export_requests == 2, "A cleared preview must not permit export.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
