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

void ensureSampleParts(MainWindow* window) {
    PreferencesManager::getInstance()->setFileShiftPreference(PreferenceChoice::kSkipAutomatically);

    auto base = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 250.0 * mm));
    CSM->addPart(base, "Base_Bracket", MeshType::kBuild);
    auto clip = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(150.0 * mm, 300.0 * mm));
    clip->setType(MeshType::kClipping);
    CSM->addPart(clip, "Cavity_Void", MeshType::kClipping);

    auto meta     = window->findChild<PartWidget*>()->getPartMeta();
    auto baseItem = meta->lookupByPointer(CSM->parts().value("Base_Bracket"));
    auto clipItem = meta->lookupByPointer(CSM->parts().value("Cavity_Void"));

    clipItem->setMeshType(MeshType::kClipping);
    baseItem->adoptChild(clipItem);
    clipItem->setTranslation(baseItem->translation() +
                             QVector3D(450.0 * mm(), 250.0 * mm(), 0.0f) * Constants::OpenGL::kObjectToView);
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
    if (gcodeBar) {
        int maxL = gcodeBar->maxLayer();
        low      = std::clamp(low, 0, maxL);
        high     = std::clamp(high, low, maxL);
        gcodeBar->setLowerLayer(low);
        gcodeBar->setUpperLayer(high);
    }
    if (gcodeView) {
        gcodeView->setLowLayer(low);
        gcodeView->setHighLayer(high);
    }
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
    if (auto* gcodeBar = window->findChild<GcodeBar*>()) { setGCodeLayerRange(window, 0, gcodeBar->maxLayer()); }
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
    auto* gcodeView = window->findChild<GCodeWidget*>()->view();

    // Default 3-D true bead widths
    gcodeView->useOrthographic(false);
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

    // Ghosted source model
    gcodeView->showGhosts(true);
    QApplication::sendPostedEvents();
    capture("preview_ghosted_model", gcodeView);
    gcodeView->showGhosts(false);

    // Orthographic 2-D
    gcodeView->resetCamera();
    gcodeView->useOrthographic(true);
    QApplication::sendPostedEvents();
    capture("preview_orthographic_2d", gcodeView);
    gcodeView->useOrthographic(false);
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
    if (baseItem && baseItem->graphicsPart()) {
        baseItem->graphicsPart()->rotateAbsolute(QQuaternion::fromEulerAngles(45.0f, 0.0f, 0.0f));
    }
    mainToolbar->showOverhang(true);
    QApplication::sendPostedEvents();
    capture("overlay_overhangs", partView);
    mainToolbar->showOverhang(false);
    if (baseItem && baseItem->graphicsPart()) { baseItem->graphicsPart()->rotateAbsolute(QQuaternion()); }
    QApplication::sendPostedEvents();
}

void captureModelRolesDemo(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partWidget = window->findChild<PartWidget*>();
    auto* partView   = partWidget->view();
    auto* gcodeView  = window->findChild<GCodeWidget*>()->view();
    auto* gcodeBar   = window->findChild<GcodeBar*>();

    partWidget->clear();
    CSM->clearParts();

    auto base = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 250.0 * mm));
    CSM->addPart(base, "Build_Base", MeshType::kBuild);

    auto clip = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(180.0 * mm, 300.0 * mm));
    clip->setType(MeshType::kClipping);
    CSM->addPart(clip, "Clipper_Cylinder", MeshType::kClipping);

    auto settingsMesh =
        QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(500.0 * mm, 500.0 * mm, 300.0 * mm));
    settingsMesh->setType(MeshType::kSettings);
    CSM->addPart(settingsMesh, "Settings_Region", MeshType::kSettings);

    auto meta     = partWidget->getPartMeta();
    auto baseItem = meta->lookupByPointer(CSM->parts().value("Build_Base"));
    auto clipItem = meta->lookupByPointer(CSM->parts().value("Clipper_Cylinder"));
    auto setItem  = meta->lookupByPointer(CSM->parts().value("Settings_Region"));

    clipItem->setMeshType(MeshType::kClipping);
    setItem->setMeshType(MeshType::kSettings);

    clipItem->setTranslation(baseItem->translation() +
                             QVector3D(300.0 * mm(), 250.0 * mm(), 0.0f) * Constants::OpenGL::kObjectToView);
    setItem->setTranslation(baseItem->translation() +
                            QVector3D(750.0 * mm(), 350.0 * mm(), 0.0f) * Constants::OpenGL::kObjectToView);

    auto setSb = CSM->parts().value("Settings_Region")->getSb();
    setSb->setSetting(PS::Infill::kEnable, true);
    setSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kGrid));
    setSb->setSetting(PS::Infill::kManualLineSpacing, true);
    setSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(25.0 * mm)()));

    GSM->getGlobal()->setSetting(PS::Skin::kEnable, false);
    GSM->getGlobal()->setSetting(PS::Infill::kEnable, true);
    GSM->getGlobal()->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kLines));
    GSM->getGlobal()->setSetting(PS::Infill::kManualLineSpacing, true);
    GSM->getGlobal()->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(60.0 * mm)()));

    QApplication::sendPostedEvents();
    capture("model_roles_setup", partView);

    sliceAndWait(window);
    if (gcodeBar) { setGCodeLayerRange(window, 0, gcodeBar->maxLayer() / 2); }
    QApplication::sendPostedEvents();
    capture("model_roles_sliced", gcodeView);
}

void capturePrimitiveShapesGrid(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partWidget = window->findChild<PartWidget*>();
    auto* partView   = partWidget->view();

    partWidget->clear();
    CSM->clearParts();

    auto box = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(250.0 * mm, 250.0 * mm, 200.0 * mm));
    CSM->addPart(box, "Rectangular_Prism", MeshType::kBuild);

    auto hex = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateHexagonalPrismMesh(150.0 * mm, 200.0 * mm));
    CSM->addPart(hex, "Hexagonal_Prism", MeshType::kBuild);

    auto openBox =
        QSharedPointer<OpenMesh>::create(MeshFactory::CreateOpenTopBoxMesh(250.0 * mm, 250.0 * mm, 200.0 * mm));
    CSM->addPart(openBox, "Open_Top_Box", MeshType::kBuild);

    auto pyr = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateTriaglePyramidMesh(280.0 * mm));
    CSM->addPart(pyr, "Triangular_Pyramid", MeshType::kBuild);

    auto cyl = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(140.0 * mm, 200.0 * mm));
    CSM->addPart(cyl, "Cylinder", MeshType::kBuild);

    auto cone = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateConeMesh(150.0 * mm, 220.0 * mm));
    CSM->addPart(cone, "Cone", MeshType::kBuild);

    auto sbox = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(220.0 * mm, 220.0 * mm, 200.0 * mm));
    sbox->setType(MeshType::kSettings);
    CSM->addPart(sbox, "Settings_Box", MeshType::kSettings);

    auto meta        = partWidget->getPartMeta();
    auto baseItem    = meta->lookupByPointer(CSM->parts().value("Rectangular_Prism"));
    QVector3D origin = baseItem->translation();

    auto place = [&](const QString& name, float dx_mm, float dy_mm, MeshType mt = MeshType::kBuild) {
        auto item = meta->lookupByPointer(CSM->parts().value(name));
        if (item) {
            item->setMeshType(mt);
            item->setTranslation(origin +
                                 QVector3D(dx_mm * mm(), dy_mm * mm(), 0.0f) * Constants::OpenGL::kObjectToView);
        }
    };

    place("Rectangular_Prism", 0.0f, 0.0f);
    place("Hexagonal_Prism", 400.0f, 0.0f);
    place("Open_Top_Box", 800.0f, 0.0f);
    place("Triangular_Pyramid", 0.0f, 400.0f);
    place("Cylinder", 400.0f, 400.0f);
    place("Cone", 800.0f, 400.0f);
    place("Settings_Box", 1200.0f, 200.0f, MeshType::kSettings);

    partView->showLabels(true);
    QApplication::sendPostedEvents();
    capture("primitive_geometry_grid", partView);
    partView->showLabels(false);
}

void captureInfillPatterns(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partWidget = window->findChild<PartWidget*>();
    auto* gcodeView  = window->findChild<GCodeWidget*>()->view();
    auto* gcodeBar   = window->findChild<GcodeBar*>();

    partWidget->clear();
    CSM->clearParts();

    auto box = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 150.0 * mm));
    CSM->addPart(box, "Infill_Sample", MeshType::kBuild);

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

    for (const auto& pair : patterns) {
        globalSb->setSetting(PS::Infill::kPattern, static_cast<int>(pair.first));
        sliceAndWait(window);
        if (gcodeBar) {
            int targetLayer = std::max(0, gcodeBar->maxLayer() / 2);
            setGCodeLayerRange(window, targetLayer, targetLayer);
        }
        QApplication::sendPostedEvents();
        capture(pair.second, gcodeView);
    }
    globalSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kLines));
}

void captureSupportDemo(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partWidget = window->findChild<PartWidget*>();
    auto* partView   = partWidget->view();
    auto* gcodeView  = window->findChild<GCodeWidget*>()->view();
    auto* gcodeBar   = window->findChild<GcodeBar*>();

    partWidget->clear();
    CSM->clearParts();

    auto pillar = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(300.0 * mm, 500.0 * mm, 300.0 * mm));
    CSM->addPart(pillar, "Support_Pillar", MeshType::kBuild);

    auto overhang = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(800.0 * mm, 500.0 * mm, 120.0 * mm));
    CSM->addPart(overhang, "Overhang_Cap", MeshType::kBuild);

    auto meta         = partWidget->getPartMeta();
    auto pillarItem   = meta->lookupByPointer(CSM->parts().value("Support_Pillar"));
    auto overhangItem = meta->lookupByPointer(CSM->parts().value("Overhang_Cap"));

    if (pillarItem && overhangItem) {
        overhangItem->setTranslation(pillarItem->translation() +
                                     QVector3D(0.0f, 0.0f, 300.0 * mm()) * Constants::OpenGL::kObjectToView);
    }

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Support::kEnable, true);
    globalSb->setSetting(PS::Support::kThresholdAngle, static_cast<double>(Angle(40.0 * deg)()));
    globalSb->setSetting(PS::Support::kLineSpacing, static_cast<double>(Distance(50.0 * mm)()));

    partView->makeCurrent();
    partView->showOverhang(true);
    QApplication::sendPostedEvents();
    capture("support_overhang_preview", partView);
    partView->makeCurrent();
    partView->showOverhang(false);

    sliceAndWait(window);
    if (gcodeBar) { setGCodeLayerRange(window, 0, gcodeBar->maxLayer()); }
    QApplication::sendPostedEvents();
    capture("support_sliced_preview", gcodeView);

    globalSb->setSetting(PS::Support::kEnable, false);
}

void captureAdhesionModes(MainWindow* window, const std::function<void(const QString&, QWidget*)>& capture) {
    auto* partWidget = window->findChild<PartWidget*>();
    auto* gcodeView  = window->findChild<GCodeWidget*>()->view();
    auto* gcodeBar   = window->findChild<GcodeBar*>();

    partWidget->clear();
    CSM->clearParts();

    auto box = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(600.0 * mm, 500.0 * mm, 200.0 * mm));
    CSM->addPart(box, "Adhesion_Part", MeshType::kBuild);

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Skin::kEnable, false);
    globalSb->setSetting(PS::Infill::kEnable, true);
    globalSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kLines));
    globalSb->setSetting(PS::Infill::kManualLineSpacing, true);
    globalSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(50.0 * mm)()));

    // 1. Raft
    globalSb->setSetting(MS::PlatformAdhesion::kRaftEnable, true);
    globalSb->setSetting(MS::PlatformAdhesion::kRaftLayers, 2);
    globalSb->setSetting(MS::PlatformAdhesion::kRaftOffset, static_cast<double>(Distance(120.0 * mm)()));
    globalSb->setSetting(MS::PlatformAdhesion::kBrimEnable, false);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtEnable, false);
    sliceAndWait(window);
    if (gcodeBar) { setGCodeLayerRange(window, 0, std::min(12, gcodeBar->maxLayer())); }
    QApplication::sendPostedEvents();
    capture("adhesion_raft", gcodeView);

    // 2. Brim
    globalSb->setSetting(MS::PlatformAdhesion::kRaftEnable, false);
    globalSb->setSetting(MS::PlatformAdhesion::kBrimEnable, true);
    globalSb->setSetting(MS::PlatformAdhesion::kBrimWidth, static_cast<double>(Distance(120.0 * mm)()));
    globalSb->setSetting(MS::PlatformAdhesion::kBrimLayers, 2);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtEnable, false);
    sliceAndWait(window);
    if (gcodeBar) { setGCodeLayerRange(window, 0, std::min(10, gcodeBar->maxLayer())); }
    QApplication::sendPostedEvents();
    capture("adhesion_brim", gcodeView);

    // 3. Skirt
    globalSb->setSetting(MS::PlatformAdhesion::kRaftEnable, false);
    globalSb->setSetting(MS::PlatformAdhesion::kBrimEnable, false);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtEnable, true);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtLayers, 2);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtLoops, 3);
    globalSb->setSetting(MS::PlatformAdhesion::kSkirtDistanceFromObject, static_cast<double>(Distance(80.0 * mm)()));
    sliceAndWait(window);
    if (gcodeBar) { setGCodeLayerRange(window, 0, std::min(10, gcodeBar->maxLayer())); }
    QApplication::sendPostedEvents();
    capture("adhesion_skirt", gcodeView);

    globalSb->setSetting(MS::PlatformAdhesion::kSkirtEnable, false);
}

QVector<QPair<QString, QString>> captureAllGuiElements(MainWindow* window, const QString& outputDirectory) {
    QVector<QPair<QString, QString>> saved_files;
    const QDir outDir(outputDirectory);

    auto capture = [&](const QString& name, QWidget* widget) {
        const QString semanticFile = outDir.filePath(name + ".png");
        saveWidget(widget, semanticFile);
        saved_files.append({name, semanticFile});
    };

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
    for (auto* btn : mainToolbar->findChildren<QToolButton*>()) {
        if (btn->toolTip().contains("Load new model") && btn->menu()) {
            btn->menu()->adjustSize();
            capture("model_import_menu", btn->menu());
        }
        if (btn->toolTip().contains("Generates a new shape") && btn->menu()) {
            btn->menu()->adjustSize();
            capture("primitive_geometry_menu", btn->menu());
        }
    }

    auto* settingDock = qobject_cast<QDockWidget*>(settingBar->parentWidget());
    if (settingDock) {
        window->addDockWidget(Qt::LeftDockWidgetArea, settingDock);
        QApplication::sendPostedEvents();
        capture("workspace_custom_layout", window);
        window->addDockWidget(Qt::RightDockWidgetArea, settingDock);
        QApplication::sendPostedEvents();
    }

    captureInspectionOverlays(window, capture);
    captureModelRolesDemo(window, capture);
    capturePrimitiveShapesGrid(window, capture);
    captureInfillPatterns(window, capture);
    captureSupportDemo(window, capture);
    captureAdhesionModes(window, capture);
    capturePreviewModes(window, capture);

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

QStringList captureAllSettingsCategories(MainWindow* window, const QString& outputDirectory) {
    QStringList saved_paths;
    auto* settingBar = window->findChild<SettingBar*>();
    settingBar->settingsBasesSelected({"", {}}, {});

    auto* tabWidget = settingBar->findChild<QTabWidget*>();
    const QDir baseDir(outputDirectory);

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);
        const QString majorSlug = toSlug(tabWidget->tabText(i));

        auto* pane = static_cast<SettingPane*>(tabWidget->widget(i));
        for (SettingTab* tab : pane->getTabs()) {
            tab->expandTab();
            for (QWidget* w : tab->findChildren<QWidget*>()) { w->setEnabled(true); }
        }

        QApplication::sendPostedEvents();

        for (SettingTab* tab : pane->getTabs()) {
            const QString destFile =
                baseDir.filePath(QString("settings/%1/%2_options.png").arg(majorSlug, toSlug(tab->getName())));
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
    parser.process(app);

    const QString outputDir = "docs/user-guide-images";

    std::cout << "Starting ORNLSlicer GUI Capture...\n";
    std::cout << "Output directory: " << outputDir.toStdString() << "\n\n";

    ORNL::MainWindow* window = ORNL::MainWindow::getInstance();
    window->show();
    ORNL::ensureSampleParts(window);
    ORNL::sliceSampleParts(window);

    const auto savedFiles = ORNL::captureAllGuiElements(window, outputDir);
    std::cout << "Captured " << savedFiles.size() << " GUI elements:\n";
    for (const auto& file : savedFiles) {
        std::cout << "  - " << file.first.toStdString() << ": " << file.second.toStdString() << "\n";
    }
    std::cout << "\n";

    std::cout << "Capturing menu bar dropdown menus to menu_bar/<menu>_menu.png...\n";
    const auto menuPaths = ORNL::captureAllMenuBarMenus(window, outputDir);
    std::cout << "Captured " << menuPaths.size() << " menu bar dropdowns:\n";
    for (const auto& path : menuPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
    std::cout << "\n";

    std::cout << "Capturing preferences tabs to preferences/<tab>_tab.png...\n";
    const auto prefPaths = ORNL::captureAllPreferencesTabs(window, outputDir);
    std::cout << "Captured " << prefPaths.size() << " preferences tabs:\n";
    for (const auto& path : prefPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
    std::cout << "\n";

    std::cout << "Capturing settings dropdown categories to settings/<major>/<minor>_options.png...\n";
    const auto settingsPaths = ORNL::captureAllSettingsCategories(window, outputDir);
    std::cout << "Captured " << settingsPaths.size() << " setting category options:\n";
    for (const auto& path : settingsPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
    std::cout << "\n";

    std::cout << "GUI capture complete.\n";
    return 0;
}
