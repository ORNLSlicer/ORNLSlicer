#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QOpenGLContext>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QTemporaryFile>
#include <cstdlib>
#include <iostream>

#include "geometry/segments/arc.h"
#include "geometry/segments/line.h"
#include "graphics/view/gcode_view.h"
#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "threading/gcode_loader.h"
#include "utilities/theme_tool.h"
#include "widgets/gcodebar.h"

namespace {
class TestView : public ORNL::GCodeView {
   public:
    using ORNL::GCodeView::GCodeView;

    void clickPoint(const QVector3D& point) {
        const auto ndc = (projectionMatrix() * viewMatrix() * QVector4D(point, 1)).toVector3DAffine();
        handleLeftClick(QPointF(ndc.x(), ndc.y()));
    }

    void zoomOrthographic() {
        for (int step = 0; step < 10; ++step) { handleWheelForward({}, 120); }
    }
};

QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> fixture() {
    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> layers;
    const float mm = (1.0 * ORNL::mm)() * ORNL::Constants::OpenGL::kObjectToView;
    for (int layer = 0; layer < 2; ++layer) {
        QVector<QSharedPointer<ORNL::SegmentBase>> segments;
        for (int index = 0; index < 12; ++index) {
            const QVector3D start((10.0f + index * 6.0f) * mm, (20.0f + layer * 12.0f) * mm, 10.0f * mm);
            const QVector3D end = start + QVector3D(5.0f * mm, 0, 0);
            auto segment        = QSharedPointer<ORNL::LineSegment>::create(ORNL::Point(start), ORNL::Point(end));
            segment->setDisplayInfo(1.0f * mm, 5.0f * mm, 0.5f * mm,
                                    index == 0 ? ORNL::SegmentDisplayType::kTravel : ORNL::SegmentDisplayType::kLine,
                                    QColor(180, 120, 40), layer * 12 + index + 1, layer);
            segment->setToolOrientation(QQuaternion::fromAxisAndAngle(0, 0, 1, index * 15.0f) *
                                        QQuaternion::fromAxisAndAngle(0, 1, 0, index * 4.0f) *
                                        QQuaternion::fromAxisAndAngle(1, 0, 0, 180));
            segments.append(segment);
        }
        layers.append(segments);
    }
    return layers;
}

QImage frame(TestView& view, QApplication& app) {
    view.update();
    app.processEvents();
    return view.grabFramebuffer();
}

bool axesAreVisible(const QImage& before, const QImage& after) {
    int red = 0, green = 0, blue = 0;
    for (int y = 0; y < after.height(); ++y) {
        for (int x = 0; x < after.width(); ++x) {
            const QColor color = after.pixelColor(x, y);
            if (before.pixelColor(x, y) == color) { continue; }
            if (color.red() > 100 && color.green() < 80 && color.blue() < 80) { ++red; }
            if (color.green() > 100 && color.red() < 80 && color.blue() < 80) { ++green; }
            if (color.blue() > 100 && color.red() < 80 && color.green() < 100) { ++blue; }
        }
    }
    if (red <= 5 || green <= 5 || blue <= 5) {
        std::cerr << "Changed axis pixels: red=" << red << " green=" << green << " blue=" << blue << '\n';
    }
    return ORNL::Testing::expect(red > 5 && green > 5 && blue > 5,
                                 "All three tool axes must produce visible pixels in each view/theme.");
}

bool renderDonutReference(TestView& view, QWidget& window, QApplication& app, const QString& output) {
    const QString source_path =
        qEnvironmentVariable("ORNLSLICER_ORIENTATION_REFERENCE_GCODE",
                             QStringLiteral(ORNLSLICER_TEST_SOURCE_DIR "/fixtures/arc_specialties_donut1827.nc"));
    QFile source(source_path);
    if (!source.open(QIODevice::ReadOnly)) { return false; }
    QTemporaryFile copy;
    if (!copy.open() || copy.write(source.readAll()) < 0) { return false; }
    copy.close();
    ORNL::GCodeLoader loader(copy.fileName(), false);
    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> reference;
    bool failed = false;
    QObject::connect(&loader, &ORNL::GCodeLoader::error, [&](const QString& error) {
        std::cerr << error.toStdString() << '\n';
        failed = true;
    });
    QObject::connect(&loader, &ORNL::GCodeLoader::gcodeLoadedVisualization, [&](const auto& layers) {
        QVector<QSharedPointer<ORNL::SegmentBase>> revolution;
        for (const auto& layer : layers) {
            for (const auto& segment : layer) {
                if (revolution.size() < 30 && dynamic_cast<ORNL::ArcSegment*>(segment.data()) &&
                    segment->depositionActive()) {
                    revolution.append(segment);
                }
            }
        }
        reference.append(revolution);
    });
    loader.run();
    if (failed || reference.isEmpty() || reference.first().size() != 30) { return false; }
    QVector<QVector<QSharedPointer<ORNL::SegmentBase>>> uncorrected(1);
    const auto raw_frame = QQuaternion::fromAxisAndAngle(0, 0, 1, -134.3784f) *
                           QQuaternion::fromAxisAndAngle(0, 1, 0, -8.3794f) *
                           QQuaternion::fromAxisAndAngle(1, 0, 0, 171.5295f);
    for (const auto& segment : reference.first()) {
        auto raw = segment->clone();
        raw->setToolOrientation(raw_frame);
        uncorrected.first().append(raw);
    }
    bool passed            = true;
    const auto preferences = ORNL::PreferencesManager::getInstance();
    preferences->setGCodePreviewModePreference(ORNL::GCodePreviewMode::kThinLines);
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode}) {
        preferences->setTheme(theme);
        QApplication::setPalette(preferences->getTheme().getPalette());
        QFile stylesheet(preferences->getTheme().getFolderPath() + "style.qss");
        if (!stylesheet.open(QIODevice::ReadOnly)) { return false; }
        window.setStyleSheet(stylesheet.readAll());
        view.setupStyle();
        view.addGCode(uncorrected);
        view.setLowSegment(0);
        view.setHighSegment(29);
        view.showToolOrientation(true);
        view.useOrthographic(false);
        view.camera()->setDefaultZoom(3.0f);
        view.camera()->reset();
        view.camera()->rotateAbsolute(QVector2D(20, -55));
        const auto arc = reference.first().first().dynamicCast<ORNL::ArcSegment>();
        view.camera()->panAbsolute(QVector3D(arc->center().x(), arc->center().y(), arc->start().z()));
        const auto before = frame(view, app);
        view.addGCode(reference);
        const auto after = frame(view, app);
        passed &= axesAreVisible(before, after);
        if (!output.isEmpty()) {
            const QString name = theme == ORNL::ThemeName::kLightMode ? "donut1827-light" : "donut1827-dark";
            before.save(output + "/" + name + "-before.png");
            after.save(output + "/" + name + "-after.png");
        }
    }
    view.clear();
    return passed;
}
}  // namespace

int main(int argc, char* argv[]) {
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
        qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QStandardPaths::setTestModeEnabled(true);
    QSurfaceFormat format;
    format.setVersion(4, 4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    if (QApplication::platformName() == "offscreen" || QApplication::platformName() == "minimal") {
        std::cerr << "A display supporting QOpenGLWidget is required for tool orientation visual checks.\n";
        return 77;
    }
    QOpenGLContext probe;
    probe.setFormat(format);
    if (!probe.create()) { return 77; }
    QApplication::setStyle(QStyleFactory::create("fusion"));
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    Q_INIT_RESOURCE(shaders);
    bool passed            = ORNL::Theme::initializeFonts();
    const auto preferences = ORNL::PreferencesManager::getInstance();
    preferences->setShowToolOrientationPreference(false);
    const auto settings = ORNL::GSM->getGlobal();
    settings->setSetting(ORNL::PRS::Dimensions::kBuildVolumeType, int(ORNL::BuildVolumeType::kRectangular));

    QWidget window;
    auto* layout = new QHBoxLayout(&window);
    auto* view   = new TestView(settings, {});
    auto* bar    = new ORNL::GcodeBar;
    layout->addWidget(view, 1);
    layout->addWidget(bar);
    bar->setFixedWidth(550);
    window.resize(1300, 650);
    window.show();
    app.processEvents();
    if (!view->isValid() || !view->shaderProgram()->isLinked()) {
        std::cerr << "An OpenGL 4.4 widget is unavailable; tool orientation visual checks cannot run.\n";
        return 77;
    }
    auto* checkbox = bar->findChild<QCheckBox*>("show_tool_orientation");
    if (!ORNL::Testing::expect(checkbox && !checkbox->isChecked(), "Orientation display must start disabled.")) {
        return EXIT_FAILURE;
    }
    QObject::connect(bar, &ORNL::GcodeBar::forwardToolOrientationChange, view, &ORNL::GCodeView::showToolOrientation);
    const auto gcode = fixture();
    {
        ORNL::GCodeObject visible_path(view, gcode, {}, false);
        visible_path.showLayers(0, 0);
        visible_path.hideSegmentType(ORNL::SegmentDisplayType::kTravel, true);
        passed &= ORNL::Testing::expect(visible_path.segmentLines().size() == 11,
                                        "Type filtering must apply when the layer range narrows the path.");
        visible_path.showSegments(2, 4);
        visible_path.hideSegmentType(ORNL::SegmentDisplayType::kTravel, false);
        const auto lines = visible_path.segmentLines();
        passed &= ORNL::Testing::expect(lines.size() == 3 && lines.first().first == 3 && lines.last().first == 5,
                                        "Unhiding a path type must retain the current segment range.");
    }
    QString output = qEnvironmentVariable("ORNLSLICER_ORIENTATION_PREVIEW_DIR");
    if (!output.isEmpty()) { QDir().mkpath(output); }
    for (auto theme : {ORNL::ThemeName::kLightMode, ORNL::ThemeName::kDarkMode}) {
        preferences->setTheme(theme);
        QApplication::setPalette(preferences->getTheme().getPalette());
        QFile stylesheet(preferences->getTheme().getFolderPath() + "style.qss");
        if (!stylesheet.open(QIODevice::ReadOnly)) { return EXIT_FAILURE; }
        window.setStyleSheet(stylesheet.readAll());
        view->setupStyle();
        for (bool true_width : {false, true}) {
            preferences->setGCodePreviewModePreference(true_width ? ORNL::GCodePreviewMode::kTrueWidths
                                                                  : ORNL::GCodePreviewMode::kThinLines);
            view->addGCode(gcode);
            view->updateSegmentWidths(true_width);
            for (bool ortho : {false, true}) {
                view->useOrthographic(ortho);
                if (ortho) { view->zoomOrthographic(); }
                view->camera()->setDefaultZoom(1.5);
                view->camera()->reset();
                view->camera()->rotateAbsolute(ortho ? QVector2D(0, 0) : QVector2D(0, -70));
                view->camera()->panAbsolute(gcode[0][6]->end().toQVector3D());
                if (checkbox->isChecked()) { checkbox->click(); }
                const auto before = frame(*view, app);
                checkbox->click();
                const auto after = frame(*view, app);
                passed &= ORNL::Testing::expect(preferences->getShowToolOrientationPreference(),
                                                "The checkbox must update the persisted preference.");
                passed &= axesAreVisible(before, after);
                if (!output.isEmpty()) {
                    const QString name = QString("%1-%2-%3")
                                             .arg(theme == ORNL::ThemeName::kLightMode ? "light" : "dark")
                                             .arg(true_width ? "beads" : "lines")
                                             .arg(ortho ? "ortho" : "perspective");
                    before.save(output + "/" + name + "-before.png");
                    after.save(output + "/" + name + "-after.png");
                    window.grab().save(output + "/" + name + "-controls.png");
                }
                view->setHighSegment(2);
                passed &= ORNL::Testing::expect(frame(*view, app) != after,
                                                "Segment playback must update the orientation scene.");
                view->setHighSegment(23);
                view->hideSegmentType(ORNL::SegmentDisplayType::kTravel, true);
                view->hideSegmentType(ORNL::SegmentDisplayType::kTravel, false);
                const auto picking = [&](bool show) {
                    view->showToolOrientation(show);
                    QList<int> selected;
                    const auto connection = QObject::connect(view, &ORNL::GCodeView::updateSelectedSegments,
                                                             [&](QList<int> added, QList<int>) { selected = added; });
                    view->clickPoint(gcode[0][6]->midpoint().toQVector3D());
                    QObject::disconnect(connection);
                    view->updateSegments({}, {6});
                    return selected;
                };
                const auto without_markers = picking(false);
                const auto with_markers    = picking(true);
                passed &= ORNL::Testing::expect(without_markers == QList<int> {6} && with_markers == without_markers,
                                                "Orientation overlays must preserve G-code segment picking.");
                view->updateSegments({6}, {});
                const auto selected_frame = frame(*view, app);
                view->updateSegmentWidths(true_width);
                passed &= ORNL::Testing::expect(frame(*view, app) == selected_frame,
                                                "Rebuilding preview widths must retain the selected active pose.");
                view->updateSegments({}, {6});
            }
        }
    }
    preferences->setGCodePreviewModePreference(ORNL::GCodePreviewMode::kAuto);
    preferences->setGCodePreviewVertexThresholdPreference(400);
    view->updateSegmentWidths(true);
    view->setLowSegment(6);
    view->setHighSegment(8);
    view->showToolOrientation(false);
    const auto local_beads = frame(*view, app);
    view->showToolOrientation(true);
    passed &= axesAreVisible(local_beads, frame(*view, app));
    view->clear();
    const auto cleared = frame(*view, app);
    view->showToolOrientation(false);
    passed &= ORNL::Testing::expect(frame(*view, app) == cleared, "Clearing must remove all orientation markers.");
    auto unsupported = fixture();
    for (auto& layer : unsupported) {
        for (auto& segment : layer) { segment->clearToolOrientation(); }
    }
    view->addGCode(unsupported);
    const auto no_frames = frame(*view, app);
    view->showToolOrientation(true);
    passed &= ORNL::Testing::expect(frame(*view, app) == no_frames,
                                    "Files without supported orientation must render no assumed markers.");
    view->clear();
    passed &= ORNL::Testing::expect(renderDonutReference(*view, window, app, output),
                                    "The Donut Ref1827 tool frames must account for the positioner rotation.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
