#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QOpenGLWidget>
#include <QPair>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVector>
#include <algorithm>
#include <functional>
#include <iostream>

#include "geometry/mesh/mesh_factory.h"
#include "graphics/base_view.h"
#include "graphics/objects/part_object.h"
#include "graphics/view/gcode_view.h"
#include "graphics/view/part_view.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "widgets/cmd_widget.h"
#include "widgets/gcode_widget.h"
#include "widgets/gcodebar.h"
#include "widgets/layerbar.h"
#include "widgets/main_toolbar.h"
#include "widgets/part_widget/part_control/part_control.h"
#include "widgets/part_widget/part_toolbar.h"
#include "widgets/part_widget/part_widget.h"
#include "widgets/part_widget/right_click_menu.h"
#include "widgets/settings/setting_bar.h"
#include "widgets/settings/setting_pane.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_tab.h"
#include "widgets/view_controls_toolbar.h"
#include "windows/dialogs/cs_dbg.h"
#include "windows/dialogs/slice_dialog.h"
#include "windows/dialogs/template_save.h"
#include "windows/flowratecalc.h"
#include "windows/gcode_export.h"
#include "windows/gcode_to_s2c.h"
#include "windows/layer_times_window.h"
#include "windows/main_window.h"
#include "windows/preferences_window.h"
#include "windows/xtrudecalc.h"

namespace ORNL {

namespace {

QString toSlug(const QString& name) {
    QString slug = name.toLower();
    slug.replace(QRegularExpression("[^a-z0-9]+"), "_");
    slug.replace(QRegularExpression("^_+|_+$"), "");
    return slug;
}

QImage captureWidget(QWidget* widget) {
    if (auto* glWidget = qobject_cast<QOpenGLWidget*>(widget)) {
        QImage img = glWidget->grabFramebuffer();
        glWidget->makeCurrent();
        return img;
    }
    return widget->grab().toImage();
}

void saveWidget(QWidget* widget, const QString& filePath) {
    QDir().mkpath(QFileInfo(filePath).path());
    captureWidget(widget).save(filePath, "PNG");
}

void clearScene(MainWindow* window) {
    window->findChild<PartWidget*>()->clear();
    CSM->clearParts();
}

QSharedPointer<PartMetaItem> addScenePart(MainWindow* window, const QSharedPointer<MeshBase>& mesh, const QString& name,
                                          MeshType mt = MeshType::kBuild) {
    mesh->setType(mt);
    CSM->addPart(mesh, name, mt);
    auto item = window->findChild<PartWidget*>()->getPartMeta()->lookupByPointer(CSM->parts().value(name));
    item->setMeshType(mt);
    return item;
}

QVector3D offsetFrom(const QSharedPointer<PartMetaItem>& baseItem, float dx_mm, float dy_mm, float dz_mm = 0.0f) {
    return baseItem->translation() +
           QVector3D(dx_mm * mm(), dy_mm * mm(), dz_mm * mm()) * Constants::OpenGL::kObjectToView;
}

void ensureSampleParts(MainWindow* window) {
    PreferencesManager::getInstance()->setFileShiftPreference(PreferenceChoice::kSkipAutomatically);

    auto base     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 250.0 * mm));
    auto baseItem = addScenePart(window, base, "Base_Bracket", MeshType::kBuild);
    auto clip     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(150.0 * mm, 300.0 * mm));
    auto clipItem = addScenePart(window, clip, "Cavity_Void", MeshType::kClipping);

    baseItem->adoptChild(clipItem);
    clipItem->setTranslation(offsetFrom(baseItem, 450.0f, 250.0f));
    baseItem->setSelected(true);
    window->findChild<PartControl*>()->expandAll();
    window->findChild<PartToolbar*>()->setEnabled(true);
    window->findChild<LayerTimesWindow*>()->setEnabled(true);

    for (QLabel* label : window->findChild<SettingBar*>()->findChildren<QLabel*>()) {
        if (label->text().contains("Currently searching in")) {
            label->setText("Currently searching in: <settings-path>");
            label->hide();
        }
    }
}

void setupCaptureCameras(MainWindow* window) {
    const QVector2D captureAngle(-30.0f, -55.0f);
    const QVector2D captureTranslate(-0.25f, -0.15f);
    constexpr float captureZoomDelta = 1200.0f;
    for (auto* view : window->findChildren<BaseView*>()) {
        view->resetCamera();
        view->camera()->rotateAbsolute(captureAngle);
        view->camera()->zoom(captureZoomDelta);
        view->translateCameraByScreenDelta(captureTranslate);
    }
}

void setGCodeLayerRange(MainWindow* window, int low, int high) {
    auto* gcodeBar  = window->findChild<GcodeBar*>();
    auto* gcodeView = window->findChild<GCodeWidget*>()->view();
    int maxL        = gcodeBar->maxLayer();
    low             = std::clamp(low, 0, maxL);
    high            = std::clamp(high, low, maxL);
    gcodeBar->setLowerLayer(low);
    gcodeBar->setUpperLayer(high);
    gcodeView->setLowLayer(low);
    gcodeView->setHighLayer(high);
}

void sliceAndWait(MainWindow* window) {
    auto* textEdit = window->findChild<GcodeBar*>()->findChild<QPlainTextEdit*>();
    QEventLoop loop;
    QObject::connect(textEdit, &QPlainTextEdit::textChanged, &loop, [textEdit, &loop]() {
        if (!textEdit->toPlainText().isEmpty()) { QTimer::singleShot(150, &loop, &QEventLoop::quit); }
    });
    emit window->findChild<MainToolbar*>()->slice();
    for (auto* dlg : window->findChildren<SliceDialog*>()) {
        QObject::disconnect(dlg, &SliceDialog::cancelSlice, nullptr, nullptr);
        for (auto* btn : dlg->findChildren<QPushButton*>()) { btn->setEnabled(false); }
        dlg->hide();
    }
    loop.exec();
    setGCodeLayerRange(window, 0, window->findChild<GcodeBar*>()->maxLayer());
    QApplication::sendPostedEvents();
}

void sliceSampleParts(MainWindow* window) {
    sliceAndWait(window);

    auto* cmd    = window->findChild<CmdWidget*>()->findChild<QTextEdit*>();
    QString text = cmd->toPlainText().replace(QDir::homePath(), "/home/user");
    text.replace(QRegularExpression("Total Slice Time.*"),
                 "Total Slice Time (excluding gcode writing/parsing): 00:00:02.000");
    cmd->setPlainText(text);
    window->findChild<LayerBar*>()->addRange(10, 40);

    setupCaptureCameras(window);
    window->findChild<MainToolbar*>()->setView(0);
}

QWidget* selectSettingsTab(MainWindow* window, int tabIndex) {
    auto* bar = window->findChild<SettingBar*>();
    bar->settingsBasesSelected({"", {}}, {});
    bar->findChild<QTabWidget*>()->setCurrentIndex(tabIndex);
    QApplication::sendPostedEvents();
    return bar;
}

QWidget* preparePreferencesWindow(MainWindow* window) {
    auto* w           = window->findChild<PreferencesWindow*>();
    auto* tabWidget   = w->findChild<QTabWidget*>();
    const int margins = w->layout()->contentsMargins().left() + w->layout()->contentsMargins().right();
    w->resize(std::max(w->width(), tabWidget->tabBar()->sizeHint().width() + margins), w->height());
    tabWidget->setCurrentIndex(0);
    return w;
}

QWidget* prepareSliceDialog(MainWindow* w) {
    auto* dlg = new SliceDialog(w);
    dlg->updateStatus(StatusUpdateStepType::kPreProcess, 100);
    dlg->updateStatus(StatusUpdateStepType::kCompute, 100);
    dlg->updateStatus(StatusUpdateStepType::kPostProcess, 100);
    dlg->updateStatus(StatusUpdateStepType::kGcodeGeneraton, 30);
    return dlg;
}

QWidget* prepareTemplateSaveDialog(MainWindow* w) {
    auto* dlg = new TemplateSaveDialog(w);
    for (auto* edit : dlg->findChildren<QLineEdit*>()) {
        if (edit->text().contains("template.s2c")) {
            edit->setText("/home/user/template.s2c");
            break;
        }
    }
    return dlg;
}

QWidget* prepareGcodeToS2CDialog(MainWindow* w) {
    auto* dlg  = new GcodeToS2CDialog(w);
    auto edits = dlg->findChildren<QLineEdit*>();
    edits[0]->setText("/home/user/sample_toolpath.gcode");
    edits[1]->setText("/home/user/extracted_settings.s2c");
    return dlg;
}

void capturePreviewModes(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* gcodeView  = window->findChild<GCodeWidget*>()->view();
    auto* gcodeBar   = window->findChild<GcodeBar*>();
    auto* partWidget = window->findChild<PartWidget*>();

    setupCaptureCameras(window);

    // Default 3-D true bead widths
    gcodeView->updateSegmentWidths(true);
    gcodeView->showGhosts(false);
    QApplication::sendPostedEvents();
    capture("preview_true_bead_width", gcodeView);

    // Thin lines
    gcodeView->updateSegmentWidths(false);
    QApplication::sendPostedEvents();
    capture("preview_thin_lines", gcodeView);

    // Restore true widths
    gcodeView->updateSegmentWidths(true);

    // Ghosted source model (show half-height toolpath inside translucent ghosted part)
    auto baseItem = partWidget->getPartMeta()->lookupByPointer(CSM->parts().value("Base_Bracket"));
    baseItem->setTransparency(110);
    setGCodeLayerRange(window, 0, gcodeBar->maxLayer() / 2);
    gcodeView->showGhosts(true);
    QApplication::sendPostedEvents();
    capture("preview_ghosted_model", gcodeView);
    gcodeView->showGhosts(false);
    baseItem->setTransparency(255);
    setGCodeLayerRange(window, 0, gcodeBar->maxLayer());

    // Orthographic 2-D
    gcodeView->useOrthographic(true);
    for (int i = 0; i < 6; ++i) { gcodeView->zoomIn(); }
    gcodeView->translateCameraByScreenDelta(QVector2D(-0.25f, -0.10f));
    QApplication::sendPostedEvents();
    capture("preview_orthographic_2d", gcodeView);
    gcodeView->useOrthographic(false);
    setupCaptureCameras(window);
}

void captureInspectionOverlays(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partWidget  = window->findChild<PartWidget*>();
    auto* partView    = partWidget->view();
    auto* mainToolbar = window->findChild<MainToolbar*>();

    partView->makeCurrent();

    // Slicing Planes + Part Names
    mainToolbar->showSlicingPlanes(true);
    QApplication::sendPostedEvents();
    capture("overlay_slicing_planes", partView);
    mainToolbar->showSlicingPlanes(false);

    mainToolbar->showLabels(true);
    QApplication::sendPostedEvents();
    capture("overlay_part_labels", partView);
    mainToolbar->showLabels(false);

    auto meta     = partWidget->getPartMeta();
    auto baseItem = meta->lookupByPointer(CSM->parts().value("Base_Bracket"));
    baseItem->graphicsPart()->rotateAbsolute(QQuaternion::fromEulerAngles(45.0f, 0.0f, 0.0f));
    mainToolbar->showOverhang(true);
    QApplication::sendPostedEvents();
    capture("overlay_overhangs", partView);
    mainToolbar->showOverhang(false);
    baseItem->graphicsPart()->rotateAbsolute(QQuaternion());
    QApplication::sendPostedEvents();
}

void sliceAndCaptureGCode(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture,
                          const QString& name, const std::function<QPair<int, int>(int)>& rangeForMaxLayer = {}) {
    sliceAndWait(window);
    if (rangeForMaxLayer) {
        const auto [low, high] = rangeForMaxLayer(window->findChild<GcodeBar*>()->maxLayer());
        setGCodeLayerRange(window, low, high);
    }
    QApplication::sendPostedEvents();
    capture(name, window->findChild<GCodeWidget*>()->view());
}

void captureModelRolesDemo(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partView = window->findChild<PartWidget*>()->view();
    clearScene(window);

    auto base     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 250.0 * mm));
    auto baseItem = addScenePart(window, base, "Build_Base", MeshType::kBuild);
    auto clip     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(180.0 * mm, 300.0 * mm));
    auto clipItem = addScenePart(window, clip, "Clipper_Cylinder", MeshType::kClipping);
    auto setMesh  = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(500.0 * mm, 500.0 * mm, 300.0 * mm));
    auto setItem  = addScenePart(window, setMesh, "Settings_Region", MeshType::kSettings);

    clipItem->setTranslation(offsetFrom(baseItem, 300.0f, 250.0f));
    setItem->setTranslation(offsetFrom(baseItem, 750.0f, 350.0f));

    auto setSb = CSM->parts().value("Settings_Region")->getSb();
    setSb->setSetting(PS::Infill::kEnable, true);
    setSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kGrid));
    setSb->setSetting(PS::Infill::kManualLineSpacing, true);
    setSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(25.0 * mm)()));

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Skin::kEnable, false);
    globalSb->setSetting(PS::Infill::kEnable, true);
    globalSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kLines));
    globalSb->setSetting(PS::Infill::kManualLineSpacing, true);
    globalSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(60.0 * mm)()));

    QApplication::sendPostedEvents();
    capture("model_roles_setup", partView);
    sliceAndCaptureGCode(window, capture, "model_roles_sliced", [](int maxL) { return QPair<int, int> {0, maxL / 2}; });
}

void capturePrimitiveShapesGrid(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partView = window->findChild<PartWidget*>()->view();
    clearScene(window);

    auto baseItem = addScenePart(
        window, QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(250.0 * mm, 250.0 * mm, 200.0 * mm)),
        "Rectangular_Prism");
    auto place = [&](const QSharedPointer<MeshBase>& mesh, const QString& name, float dx, float dy,
                     MeshType mt = MeshType::kBuild) {
        addScenePart(window, mesh, name, mt)->setTranslation(offsetFrom(baseItem, dx, dy));
    };

    place(QSharedPointer<ClosedMesh>::create(MeshFactory::CreateHexagonalPrismMesh(150.0 * mm, 200.0 * mm)),
          "Hexagonal_Prism", 400.0f, 0.0f);
    place(QSharedPointer<OpenMesh>::create(MeshFactory::CreateOpenTopBoxMesh(250.0 * mm, 250.0 * mm, 200.0 * mm)),
          "Open_Top_Box", 800.0f, 0.0f);
    place(QSharedPointer<ClosedMesh>::create(MeshFactory::CreateTriaglePyramidMesh(280.0 * mm)), "Triangular_Pyramid",
          0.0f, 400.0f);
    place(QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(140.0 * mm, 200.0 * mm)), "Cylinder",
          400.0f, 400.0f);
    place(QSharedPointer<ClosedMesh>::create(MeshFactory::CreateConeMesh(150.0 * mm, 220.0 * mm)), "Cone", 800.0f,
          400.0f);
    place(QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(220.0 * mm, 220.0 * mm, 200.0 * mm)),
          "Settings_Box", 1200.0f, 200.0f, MeshType::kSettings);

    partView->showLabels(true);
    QApplication::sendPostedEvents();
    capture("primitive_geometry_grid", partView);
    partView->showLabels(false);
}

void captureInfillPatterns(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    clearScene(window);
    addScenePart(window,
                 QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 150.0 * mm)),
                 "Infill_Sample");

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Perimeter::kEnable, true);
    globalSb->setSetting(PS::Perimeter::kCount, 1);
    globalSb->setSetting(PS::Inset::kEnable, false);
    globalSb->setSetting(PS::Skin::kEnable, false);
    globalSb->setSetting(PS::Infill::kEnable, true);
    globalSb->setSetting(PS::Infill::kManualLineSpacing, true);
    globalSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(75.0 * mm)()));

    const QVector<QPair<InfillPatterns, QString>> patterns = {
        {InfillPatterns::kLines, "infill_pattern_lines"},
        {InfillPatterns::kGrid, "infill_pattern_grid"},
        {InfillPatterns::kConcentric, "infill_pattern_concentric"},
        {InfillPatterns::kTriangles, "infill_pattern_triangles"},
        {InfillPatterns::kHexagonsAndTriangles, "infill_pattern_hexagons_and_triangles"},
        {InfillPatterns::kHoneycomb, "infill_pattern_honeycomb"},
    };

    for (const auto& [pattern, name] : patterns) {
        globalSb->setSetting(PS::Infill::kPattern, static_cast<int>(pattern));
        sliceAndCaptureGCode(window, capture, name, [](int maxL) {
            int mid = std::max(0, maxL / 2);
            return QPair<int, int> {mid, mid};
        });
    }
}

void captureSupportDemo(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partView = window->findChild<PartWidget*>()->view();
    clearScene(window);

    auto pillarItem = addScenePart(
        window, QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(300.0 * mm, 500.0 * mm, 300.0 * mm)),
        "Support_Pillar");
    auto overhangItem = addScenePart(
        window, QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(800.0 * mm, 500.0 * mm, 120.0 * mm)),
        "Overhang_Cap");
    overhangItem->setTranslation(offsetFrom(pillarItem, 0.0f, 0.0f, 300.0f));

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Support::kEnable, true);
    globalSb->setSetting(PS::Support::kThresholdAngle, static_cast<double>(Angle(40.0 * deg)()));
    globalSb->setSetting(PS::Support::kLineSpacing, static_cast<double>(Distance(50.0 * mm)()));

    overhangItem->graphicsPart()->rotateAbsolute(QQuaternion::fromEulerAngles(45.0f, 0.0f, 0.0f));
    partView->makeCurrent();
    partView->showOverhang(true);
    QApplication::sendPostedEvents();
    capture("support_overhang_preview", partView);
    partView->makeCurrent();
    partView->showOverhang(false);
    overhangItem->graphicsPart()->rotateAbsolute(QQuaternion());
    QApplication::sendPostedEvents();

    sliceAndCaptureGCode(window, capture, "support_sliced_preview");
}

void captureAdhesionModes(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    clearScene(window);
    addScenePart(window,
                 QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(600.0 * mm, 500.0 * mm, 200.0 * mm)),
                 "Adhesion_Part");

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Skin::kEnable, false);
    globalSb->setSetting(PS::Infill::kEnable, true);
    globalSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kLines));
    globalSb->setSetting(PS::Infill::kManualLineSpacing, true);
    globalSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(50.0 * mm)()));

    auto captureAdhesion = [&](const QString& name, int maxCapLayer) {
        sliceAndCaptureGCode(window, capture, name,
                             [maxCapLayer](int maxL) { return QPair<int, int> {0, std::min(maxCapLayer, maxL)}; });
    };

    // 1. Raft
    globalSb->setSetting(MS::PlatformAdhesion::kRaftEnable, true);
    globalSb->setSetting(MS::PlatformAdhesion::kRaftLayers, 2);
    globalSb->setSetting(MS::PlatformAdhesion::kRaftOffset, static_cast<double>(Distance(120.0 * mm)()));
    globalSb->setSetting(MS::PlatformAdhesion::kBrimEnable, false);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtEnable, false);
    captureAdhesion("adhesion_raft", 12);

    // 2. Brim
    globalSb->setSetting(MS::PlatformAdhesion::kRaftEnable, false);
    globalSb->setSetting(MS::PlatformAdhesion::kBrimEnable, true);
    globalSb->setSetting(MS::PlatformAdhesion::kBrimWidth, static_cast<double>(Distance(120.0 * mm)()));
    globalSb->setSetting(MS::PlatformAdhesion::kBrimLayers, 2);
    captureAdhesion("adhesion_brim", 10);

    // 3. Skirt
    globalSb->setSetting(MS::PlatformAdhesion::kBrimEnable, false);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtEnable, true);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtLayers, 2);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtLoops, 3);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtDistanceFromObject, static_cast<double>(Distance(80.0 * mm)()));
    captureAdhesion("adhesion_skirt", 10);
}

void captureAllSceneDemos(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    const fifojson savedGlobalJson = GSM->getGlobal()->json();
    auto restoreGlobals            = [&]() { GSM->getGlobal()->json(savedGlobalJson); };

    captureInspectionOverlays(window, capture);
    capturePreviewModes(window, capture);
    captureModelRolesDemo(window, capture);
    restoreGlobals();
    capturePrimitiveShapesGrid(window, capture);
    restoreGlobals();
    captureInfillPatterns(window, capture);
    restoreGlobals();
    captureSupportDemo(window, capture);
    restoreGlobals();
    captureAdhesionModes(window, capture);
    restoreGlobals();
}

QVector<QPair<QString, QString>> captureAllGuiElements(MainWindow* window, const QString& outputDirectory,
                                                       bool includeGui, bool includeDemos) {
    QVector<QPair<QString, QString>> saved_files;
    const QDir outDir(outputDirectory);

    auto capture = [&](const QString& name, QWidget* widget) {
        const QString semanticFile = outDir.filePath(name + ".png");
        saveWidget(widget, semanticFile);
        saved_files.append({name, semanticFile});
    };

    if (includeGui) {
        capture("menu_bar_menus", window->menuBar());
        capture("main_toolbar", window->findChild<MainToolbar*>());
        capture("main_window_overview", window);
        capture("part_view_workspace", static_cast<QWidget*>(window->findChild<PartWidget*>()->view()));
        capture("gcode_view_workspace", static_cast<QWidget*>(window->findChild<GCodeWidget*>()->view()));
        auto* settingBar = selectSettingsTab(window, 0);
        auto* gcodeBar   = window->findChild<GcodeBar*>();
        capture("dock_panels_sidebar", settingBar->parentWidget());
        capture("gcode_editor_panel", gcodeBar->parentWidget());
        capture("settings_sidebar", settingBar);
        capture("printer_settings_panel", settingBar);
        capture("material_settings_panel", selectSettingsTab(window, 1));
        capture("profile_settings_panel", selectSettingsTab(window, 2));
        capture("experimental_settings_panel", selectSettingsTab(window, 3));
        capture("transform_controls", window->findChild<PartToolbar*>());
        capture("view_controls_toolbar", window->findChild<ViewControlsToolbar*>());
        capture("part_control", window->findChild<PartControl*>());
        capture("layer_bar", window->findChild<LayerBar*>());
        capture("preferences_window", preparePreferencesWindow(window));
        capture("slice_dialog", prepareSliceDialog(window));
        capture("template_save_dialog", prepareTemplateSaveDialog(window));
        capture("gcode_export_dialog", window->findChild<GcodeExport*>());
        capture("gcode_to_s2c_dialog", prepareGcodeToS2CDialog(window));
        capture("layer_times_window", window->findChild<LayerTimesWindow*>());
        capture("flowrate_calculator", window->findChild<FlowrateCalcWindow*>());
        capture("xtrude_calculator", window->findChild<XtrudeCalcWindow*>());
        capture("part_context_menu", new RightClickMenu(window));
        capture("cs_debug_dialog", new CsDebugDialog(window));

        auto* mainToolbar = window->findChild<MainToolbar*>();
        for (QAction* act : mainToolbar->actions()) {
            if (act->toolTip().contains("Load new model")) {
                act->menu()->adjustSize();
                capture("model_import_menu", act->menu());
            }
            if (act->toolTip().contains("Generates a new shape")) {
                act->menu()->adjustSize();
                capture("primitive_geometry_menu", act->menu());
            }
        }

        auto* settingDock = qobject_cast<QDockWidget*>(settingBar->parentWidget());
        window->addDockWidget(Qt::LeftDockWidgetArea, settingDock);
        QApplication::sendPostedEvents();
        capture("workspace_custom_layout", window);
        window->addDockWidget(Qt::RightDockWidgetArea, settingDock);
        QApplication::sendPostedEvents();
    }

    if (includeDemos) { captureAllSceneDemos(window, capture); }

    return saved_files;
}

QStringList captureAllMenuBarMenus(MainWindow* window, const QString& outputDirectory) {
    QStringList saved_paths;
    const QDir baseDir(outputDirectory);

    for (QAction* act : window->menuBar()->actions()) {
        QMenu* menu = act->menu();
        if (!menu) { continue; }

        menu->adjustSize();
        for (QAction* a : menu->actions()) { a->setEnabled(true); }
        const QString destFile = baseDir.filePath(QString("menu_bar/%1_menu.png").arg(toSlug(act->text())));
        saveWidget(menu, destFile);
        saved_paths << destFile;
    }

    return saved_paths;
}

QStringList captureAllPreferencesTabs(MainWindow* window, const QString& outputDirectory) {
    QStringList saved_paths;
    auto* prefWindow = preparePreferencesWindow(window);
    auto* tabWidget  = prefWindow->findChild<QTabWidget*>();
    const QDir baseDir(outputDirectory);

    for (QWidget* w : prefWindow->findChildren<QWidget*>()) { w->setEnabled(true); }
    for (QAction* a : prefWindow->findChildren<QAction*>()) { a->setEnabled(true); }

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);
        QApplication::sendPostedEvents();
        auto* page = tabWidget->widget(i);
        page->adjustSize();
        const QString destFile = baseDir.filePath(QString("preferences/%1_tab.png").arg(toSlug(tabWidget->tabText(i))));
        saveWidget(page, destFile);
        saved_paths << destFile;
    }

    return saved_paths;
}

QStringList captureAllSettingsCategories(MainWindow* window, const QString& outputDirectory,
                                         const QStringList& categoryFilters = {}) {
    QStringList saved_paths;
    PreferencesManager::getInstance()->setDisabledSettingVisibilityPreference(DisabledSettingVisibility::kGrey);

    auto* settingBar = window->findChild<SettingBar*>();
    settingBar->settingsBasesSelected({"", {}}, {});

    auto* tabWidget = settingBar->findChild<QTabWidget*>();
    const QDir baseDir(outputDirectory);

    QStringList normalizedFilters;
    for (const QString& rawFilter : categoryFilters) {
        for (const QString& part : rawFilter.split(',', Qt::SkipEmptyParts)) {
            const QStringList tokens = part.split('/', Qt::SkipEmptyParts);
            if (tokens.size() == 2) { normalizedFilters << (toSlug(tokens[0]) + "/" + toSlug(tokens[1])); }
            else if (!tokens.isEmpty()) { normalizedFilters << toSlug(tokens[0]); }
        }
    }

    auto matchesCategory = [&](const QString& majorSlug, const QString& minorSlug) {
        if (normalizedFilters.isEmpty()) { return true; }
        return normalizedFilters.contains(minorSlug) || normalizedFilters.contains(majorSlug + "/" + minorSlug);
    };

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);
        const QString majorSlug = toSlug(tabWidget->tabText(i));

        auto* pane = static_cast<SettingPane*>(tabWidget->widget(i));
        for (SettingTab* tab : pane->getTabs()) {
            tab->show();
            tab->expandTab();
            for (const QSharedPointer<SettingRowBase>& row : tab->getRows()) { row->show(); }
            for (QWidget* w : tab->findChildren<QWidget*>()) { w->setEnabled(true); }
        }

        QApplication::sendPostedEvents();

        for (SettingTab* tab : pane->getTabs()) {
            const QString minorSlug = toSlug(tab->getName());
            if (!matchesCategory(majorSlug, minorSlug)) { continue; }
            const QString destFile = baseDir.filePath(QString("settings/%1/%2_options.png").arg(majorSlug, minorSlug));
            saveWidget(tab, destFile);
            saved_paths << destFile;
        }
    }

    return saved_paths;
}

}  // namespace

}  // namespace ORNL

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("fusion"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Captures designated GUI elements with semantic names for documentation.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption sectionOption(
        {"s", "section"},
        "Capture section(s): all, gui, demos, menus, preferences, settings (comma-separated or repeatable).", "section",
        "all");
    QCommandLineOption categoryOption(
        {"c", "settings-category"},
        "Capture only matching settings category slug(s), e.g. 'helical' or 'profile/helical'. Implies --section "
        "settings unless --section is also specified.",
        "category");
    parser.addOption(sectionOption);
    parser.addOption(categoryOption);
    parser.process(app);

    const QStringList categoryFilters = parser.values(categoryOption);
    QStringList sections;
    if (parser.isSet(sectionOption)) {
        for (const QString& val : parser.values(sectionOption)) {
            for (const QString& part : val.split(',', Qt::SkipEmptyParts)) { sections << part.trimmed().toLower(); }
        }
    }
    else if (!categoryFilters.isEmpty()) { sections << "settings"; }
    else { sections << "all"; }

    const bool runAll   = sections.contains("all");
    const bool runGui   = runAll || sections.contains("gui");
    const bool runDemos = runAll || sections.contains("demos");
    const bool runMenus = runAll || sections.contains("menus");
    const bool runPrefs = runAll || sections.contains("preferences");
    const bool runSets  = runAll || sections.contains("settings") || !categoryFilters.isEmpty();

    const QString outputDir = "docs/user-guide-images";

    std::cout << "Starting ORNLSlicer GUI Capture...\n";
    std::cout << "Output directory: " << outputDir.toStdString() << "\n\n";

    ORNL::MainWindow* window = ORNL::MainWindow::getInstance();
    window->show();
    ORNL::ensureSampleParts(window);
    if (runGui || runDemos) { ORNL::sliceSampleParts(window); }

    if (runGui || runDemos) {
        const auto savedFiles = ORNL::captureAllGuiElements(window, outputDir, runGui, runDemos);
        std::cout << "Captured " << savedFiles.size() << " GUI/demo elements:\n";
        for (const auto& file : savedFiles) {
            std::cout << "  - " << file.first.toStdString() << ": " << file.second.toStdString() << "\n";
        }
        std::cout << "\n";
    }

    if (runMenus) {
        std::cout << "Capturing menu bar dropdown menus to menu_bar/<menu>_menu.png...\n";
        const auto menuPaths = ORNL::captureAllMenuBarMenus(window, outputDir);
        std::cout << "Captured " << menuPaths.size() << " menu bar dropdowns:\n";
        for (const auto& path : menuPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
        std::cout << "\n";
    }

    if (runPrefs) {
        std::cout << "Capturing preferences tabs to preferences/<tab>_tab.png...\n";
        const auto prefPaths = ORNL::captureAllPreferencesTabs(window, outputDir);
        std::cout << "Captured " << prefPaths.size() << " preferences tabs:\n";
        for (const auto& path : prefPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
        std::cout << "\n";
    }

    if (runSets) {
        std::cout << "Capturing settings dropdown categories to settings/<major>/<minor>_options.png...\n";
        const auto settingsPaths = ORNL::captureAllSettingsCategories(window, outputDir, categoryFilters);
        std::cout << "Captured " << settingsPaths.size() << " setting category options:\n";
        for (const auto& path : settingsPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
        std::cout << "\n";
    }

    std::cout << "GUI capture complete.\n";
    return 0;
}
