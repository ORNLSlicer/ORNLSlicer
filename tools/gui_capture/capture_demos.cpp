#include <QApplication>
#include <QCheckBox>
#include <QVector>
#include <algorithm>

#include "capture_common.h"
#include "configs/settings_base.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "geometry/mesh/open_mesh.h"
#include "graphics/view/gcode_view.h"
#include "graphics/view/part_view.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
#include "widgets/gcode_widget.h"
#include "widgets/gcodebar.h"
#include "widgets/main_toolbar.h"
#include "widgets/part_widget/part_widget.h"
#include "windows/main_window.h"

namespace ORNL {

namespace {

void capturePreviewModes(MainWindow* window, const CaptureCallback& capture) {
    auto* gcodeView  = requireNonNull(requireChild<GCodeWidget>(window)->view(), "GCodeWidget::view");
    auto* gcodeBar   = requireChild<GcodeBar>(window);
    auto* partWidget = requireChild<PartWidget>(window);

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
    for (int i = 0; i < 4; ++i) { gcodeView->zoomIn(); }
    gcodeView->translateCameraByScreenDelta(QVector2D(-0.28f, -0.19f));
    QApplication::sendPostedEvents();
    capture("preview_orthographic_2d", gcodeView);
    gcodeView->useOrthographic(false);
    setupCaptureCameras(window);
}

void captureInspectionOverlays(MainWindow* window, const CaptureCallback& capture) {
    auto* partWidget  = requireChild<PartWidget>(window);
    auto* partView    = requireNonNull(partWidget->view(), "PartWidget::view");
    auto* mainToolbar = requireChild<MainToolbar>(window);

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

void sliceAndCaptureGCode(MainWindow* window, const CaptureCallback& capture, const QString& name,
                          const std::function<QPair<int, int>(int)>& rangeForMaxLayer = {}) {
    sliceAndWait(window);
    if (rangeForMaxLayer) {
        const auto [low, high] = rangeForMaxLayer(requireChild<GcodeBar>(window)->maxLayer());
        setGCodeLayerRange(window, low, high);
    }
    QApplication::sendPostedEvents();
    capture(name, requireNonNull(requireChild<GCodeWidget>(window)->view(), "GCodeWidget::view"));
}

void captureModelRolesDemo(MainWindow* window, const CaptureCallback& capture) {
    auto* partView = requireNonNull(requireChild<PartWidget>(window)->view(), "PartWidget::view");
    clearScene(window);

    auto base     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 250.0 * mm));
    auto baseItem = addScenePart(window, base, "Build_Base", MeshType::kBuild);
    auto clip     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(180.0 * mm, 300.0 * mm));
    auto clipItem = addScenePart(window, clip, "Clipper_Cylinder", MeshType::kClipping);
    auto setMesh  = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(500.0 * mm, 500.0 * mm, 300.0 * mm));
    auto setItem  = addScenePart(window, setMesh, "Settings_Region", MeshType::kSettings);

    clipItem->setTranslation(offsetFrom(baseItem, 300.0f, 250.0f));
    setItem->setTranslation(offsetFrom(baseItem, 750.0f, 350.0f));

    const QVector3D groupCenter = offsetFrom(baseItem, 650.0f, 400.0f, 125.0f);
    setupCaptureCameras(window, QVector2D(-35.0f, -42.0f), 1400.0f, groupCenter);

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

void capturePrimitiveShapesGrid(MainWindow* window, const CaptureCallback& capture) {
    auto* partView = requireNonNull(requireChild<PartWidget>(window)->view(), "PartWidget::view");
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

    const QVector3D gridCenter = offsetFrom(baseItem, 550.0f, 180.0f, 80.0f);
    setupCaptureCameras(window, QVector2D(-30.0f, -45.0f), 1400.0f, gridCenter);

    partView->showLabels(true);
    QApplication::sendPostedEvents();
    capture("primitive_geometry_grid", partView);
    partView->showLabels(false);
}

void captureInfillPatterns(MainWindow* window, const CaptureCallback& capture) {
    clearScene(window);
    auto infillItem = addScenePart(
        window, QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 150.0 * mm)),
        "Infill_Sample");

    const QVector3D infillCenter = infillItem->translation() + QVector3D(600.0f * mm(), 400.0f * mm(), 40.0f * mm()) *
                                                                   Constants::OpenGL::kObjectToView;
    setupCaptureCameras(window, QVector2D(-25.0f, -60.0f), 1400.0f, infillCenter);

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

void captureSupportDemo(MainWindow* window, const CaptureCallback& capture) {
    auto* partView = requireNonNull(requireChild<PartWidget>(window)->view(), "PartWidget::view");
    clearScene(window);

    auto pillarItem = addScenePart(
        window, QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(300.0 * mm, 500.0 * mm, 300.0 * mm)),
        "Support_Pillar");
    auto overhangItem = addScenePart(
        window, QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(800.0 * mm, 500.0 * mm, 120.0 * mm)),
        "Overhang_Cap");
    overhangItem->setTranslation(offsetFrom(pillarItem, 0.0f, 0.0f, 300.0f));

    const QVector3D supportCenter = pillarItem->translation() + QVector3D(400.0f * mm(), 250.0f * mm(), 200.0f * mm()) *
                                                                    Constants::OpenGL::kObjectToView;
    setupCaptureCameras(window, QVector2D(-40.0f, -50.0f), 1350.0f, supportCenter);

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
    QApplication::sendPostedEvents();

    sliceAndCaptureGCode(window, capture, "support_sliced_preview");
}

void captureAdhesionModes(MainWindow* window, const CaptureCallback& capture) {
    clearScene(window);
    auto adhesionItem = addScenePart(
        window, QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(600.0 * mm, 500.0 * mm, 200.0 * mm)),
        "Adhesion_Part");

    const QVector3D adhesionCenter =
        adhesionItem->translation() +
        QVector3D(300.0f * mm(), 250.0f * mm(), 30.0f * mm()) * Constants::OpenGL::kObjectToView;
    setupCaptureCameras(window, QVector2D(-32.0f, -48.0f), 1650.0f, adhesionCenter);

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

void capturePartSettingsDemo(MainWindow* window, const CaptureCallback& capture) {
    clearScene(window);
    auto* partWidget = requireChild<PartWidget>(window);
    auto* partView   = requireNonNull(partWidget->view(), "PartWidget::view");

    auto boxA  = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(450.0 * mm, 450.0 * mm, 150.0 * mm));
    auto itemA = addScenePart(window, boxA, "Part_A");
    auto boxB  = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(450.0 * mm, 450.0 * mm, 150.0 * mm));
    auto itemB = addScenePart(window, boxB, "Part_B");
    itemB->setTranslation(offsetFrom(itemA, 550.0f, 0.0f));

    const QVector3D groupCenter = offsetFrom(itemA, 500.0f, 225.0f, 75.0f);
    setupCaptureCameras(window, QVector2D(-35.0f, -42.0f), 1500.0f, groupCenter);

    auto partSbA = CSM->parts().value("Part_A")->getSb();
    partSbA->setSetting(PS::Infill::kEnable, true);
    partSbA->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kGrid));
    partSbA->setSetting(PS::Infill::kManualLineSpacing, true);
    partSbA->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(25.0 * mm)()));

    auto partSbB = CSM->parts().value("Part_B")->getSb();
    partSbB->setSetting(PS::Infill::kEnable, true);
    partSbB->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kLines));
    partSbB->setSetting(PS::Infill::kManualLineSpacing, true);
    partSbB->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(60.0 * mm)()));

    itemA->setSelected(true);
    itemB->setSelected(false);
    QApplication::sendPostedEvents();
    capture("part_settings_setup", partView);

    sliceAndCaptureGCode(window, capture, "part_settings_sliced", [](int maxL) {
        int mid = std::max(0, maxL / 2);
        return QPair<int, int> {mid, mid};
    });
}

void captureSettingsRegionDemo(MainWindow* window, const CaptureCallback& capture) {
    clearScene(window);
    auto* partWidget = requireChild<PartWidget>(window);
    auto* partView   = requireNonNull(partWidget->view(), "PartWidget::view");

    auto base     = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(900.0 * mm, 500.0 * mm, 150.0 * mm));
    auto baseItem = addScenePart(window, base, "Main_Part", MeshType::kBuild);
    auto box      = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(450.0 * mm, 500.0 * mm, 200.0 * mm));
    auto boxItem  = addScenePart(window, box, "Dense_Region", MeshType::kSettings);
    boxItem->setTranslation(offsetFrom(baseItem, 400.0f, 0.0f));

    const QVector3D regionCenter = offsetFrom(baseItem, 500.0f, 250.0f, 90.0f);
    setupCaptureCameras(window, QVector2D(-35.0f, -42.0f), 1500.0f, regionCenter);

    auto setSb = CSM->parts().value("Dense_Region")->getSb();
    setSb->setSetting(PS::Infill::kEnable, true);
    setSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kGrid));
    setSb->setSetting(PS::Infill::kManualLineSpacing, true);
    setSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(25.0 * mm)()));

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Skin::kEnable, false);
    globalSb->setSetting(PS::Infill::kEnable, true);
    globalSb->setSetting(PS::Infill::kPattern, static_cast<int>(InfillPatterns::kLines));
    globalSb->setSetting(PS::Infill::kManualLineSpacing, true);
    globalSb->setSetting(PS::Infill::kLineSpacing, static_cast<double>(Distance(65.0 * mm)()));

    QApplication::sendPostedEvents();
    capture("settings_region_setup", partView);

    sliceAndCaptureGCode(window, capture, "settings_region_sliced", [](int maxL) {
        int mid = std::max(0, maxL / 2);
        return QPair<int, int> {mid, mid};
    });
}

void captureOptimizationDemo(MainWindow* window, const CaptureCallback& capture) {
    clearScene(window);

    auto cylA  = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(120.0 * mm, 100.0 * mm));
    auto itemA = addScenePart(window, cylA, "Island_1");
    auto cylB  = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(120.0 * mm, 100.0 * mm));
    auto itemB = addScenePart(window, cylB, "Island_2");
    itemB->setTranslation(offsetFrom(itemA, 500.0f, 0.0f));
    auto cylC  = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateCylinderMesh(120.0 * mm, 100.0 * mm));
    auto itemC = addScenePart(window, cylC, "Island_3");
    itemC->setTranslation(offsetFrom(itemA, 250.0f, 400.0f));

    const QVector3D groupCenter = offsetFrom(itemA, 310.0f, 200.0f, 50.0f);
    setupCaptureCameras(window, QVector2D(-30.0f, -52.0f), 2000.0f, groupCenter);

    auto globalSb = GSM->getGlobal();
    globalSb->setSetting(PS::Perimeter::kEnable, true);
    globalSb->setSetting(PS::Perimeter::kCount, 2);
    globalSb->setSetting(PS::Skin::kEnable, false);
    globalSb->setSetting(PS::Infill::kEnable, false);

    auto* gcodeBar = requireChild<GcodeBar>(window);
    for (auto* cb : gcodeBar->findChildren<QCheckBox*>()) {
        if (cb->text().contains("Hide Travel")) {
            cb->setChecked(false);
            break;
        }
    }

    sliceAndCaptureGCode(window, capture, "optimization_island_ordering", [](int maxL) {
        int mid = std::max(0, maxL / 2);
        return QPair<int, int> {mid, mid};
    });
}

}  // namespace

void captureAllSceneDemos(MainWindow* window, const CaptureCallback& capture) {
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
    capturePartSettingsDemo(window, capture);
    restoreGlobals();
    captureSettingsRegionDemo(window, capture);
    restoreGlobals();
    captureOptimizationDemo(window, capture);
    restoreGlobals();
}

}  // namespace ORNL
