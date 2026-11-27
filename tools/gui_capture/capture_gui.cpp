#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QHeaderView>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTreeWidget>

#include "capture_common.h"
#include "configs/settings_base.h"
#include "geometry/mesh/closed_mesh.h"
#include "geometry/mesh/mesh_factory.h"
#include "graphics/view/gcode_view.h"
#include "graphics/view/part_view.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "part/part.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "utilities/enums.h"
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
#include "windows/flowratecalc.h"
#include "windows/gcode_export.h"
#include "windows/layer_times_window.h"
#include "windows/main_window.h"
#include "windows/xtrudecalc.h"

namespace ORNL {

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
        capture("menu_bar_menus", requireNonNull(window->menuBar(), "MainWindow::menuBar"));
        capture("main_toolbar", requireChild<MainToolbar>(window));
        capture("main_window_overview", window);
        capture("part_view_workspace",
                static_cast<QWidget*>(requireNonNull(requireChild<PartWidget>(window)->view(), "PartWidget::view")));
        capture("gcode_view_workspace",
                static_cast<QWidget*>(requireNonNull(requireChild<GCodeWidget>(window)->view(), "GCodeWidget::view")));
        auto* settingBar = selectSettingsTab(window, 0);
        auto* gcodeBar   = requireChild<GcodeBar>(window);
        capture("dock_panels_sidebar", requireNonNull(settingBar->parentWidget(), "SettingBar::parentWidget"));
        capture("gcode_editor_panel", requireNonNull(gcodeBar->parentWidget(), "GcodeBar::parentWidget"));
        capture("settings_sidebar", settingBar);
        capture("printer_settings_panel", settingBar);
        capture("material_settings_panel", selectSettingsTab(window, 1));
        capture("profile_settings_panel", selectSettingsTab(window, 2));
        capture("experimental_settings_panel", selectSettingsTab(window, 3));
        capture("transform_controls", requireChild<PartToolbar>(window));
        capture("view_controls_toolbar", requireChild<ViewControlsToolbar>(window));
        capture("layer_bar", requireChild<LayerBar>(window));
        capture("preferences_window", preparePreferencesWindow(window));
        capture("slice_dialog", prepareSliceDialog(window));
        capture("template_save_dialog", prepareTemplateSaveDialog(window));
        capture("gcode_export_dialog", requireChild<GcodeExport>(window));
        capture("gcode_to_s2c_dialog", prepareGcodeToS2CDialog(window));
        capture("settings_file_compare", prepareSettingsFileCompareDialog(window));
        capture("layer_times_window", requireChild<LayerTimesWindow>(window));
        capture("flowrate_calculator", requireChild<FlowrateCalcWindow>(window));
        capture("xtrude_calculator", requireChild<XtrudeCalcWindow>(window));
        capture("part_context_menu", new RightClickMenu(window));
        capture("cs_debug_dialog", new CsDebugDialog(window));

        auto* mainToolbar     = requireChild<MainToolbar>(window);
        bool capturedImport   = false;
        bool capturedPrimMenu = false;
        for (QAction* act : mainToolbar->actions()) {
            if (act->toolTip().contains("Load new model")) {
                auto* menu = requireNonNull(act->menu(), "Load new model menu");
                menu->adjustSize();
                capture("model_import_menu", menu);
                capturedImport = true;
            }
            if (act->toolTip().contains("Generates a new shape")) {
                auto* menu = requireNonNull(act->menu(), "Primitive geometry menu");
                menu->adjustSize();
                capture("primitive_geometry_menu", menu);
                capturedPrimMenu = true;
            }
        }
        if (!capturedImport) { throw std::runtime_error("Missing 'Load new model' toolbar action"); }
        if (!capturedPrimMenu) { throw std::runtime_error("Missing 'Generates a new shape' toolbar action"); }

        auto* settingDock =
            requireNonNull(qobject_cast<QDockWidget*>(settingBar->parentWidget()), "SettingBar QDockWidget parent");
        window->addDockWidget(Qt::LeftDockWidgetArea, settingDock);
        QApplication::sendPostedEvents();
        capture("workspace_custom_layout", window);
        window->addDockWidget(Qt::RightDockWidgetArea, settingDock);
        QApplication::sendPostedEvents();

        {
            auto* partControl = requireChild<PartControl>(window);
            auto globalModMesh =
                QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(200.0 * mm, 200.0 * mm, 100.0 * mm));
            addScenePart(window, globalModMesh, "Density_Zone", MeshType::kSettings);
            partControl->expandAll();
            const QSize origSize = partControl->size();
            partControl->resize(340, 210);
            auto* tree = requireChild<QTreeWidget>(partControl);
            tree->header()->setStretchLastSection(true);
            tree->setColumnWidth(0, 175);
            tree->setColumnWidth(1, 145);
            QApplication::sendPostedEvents();
            capture("part_control", partControl);
            partControl->resize(origSize);
        }

        clearScene(window);
        capture("part_view_empty_workspace",
                static_cast<QWidget*>(requireNonNull(requireChild<PartWidget>(window)->view(), "PartWidget::view")));
        ensureSampleParts(window);
        QApplication::sendPostedEvents();

        auto* profileBar =
            requireNonNull(qobject_cast<SettingBar*>(selectSettingsTab(window, 2)), "profile SettingBar");
        profileBar->settingsBasesSelected({"", {}}, {});
        QApplication::sendPostedEvents();
        auto* tabWidget      = requireChild<QTabWidget>(profileBar);
        auto* profilePane    = requireNonNull(qobject_cast<SettingPane*>(tabWidget->widget(2)), "Profile SettingPane");
        bool capturedLayer   = false;
        bool capturedSlicing = false;
        bool capturedHiddenSettings = false;
        for (SettingTab* tab : profilePane->getTabs()) {
            if (toSlug(tab->getName()) == "layer") {
                tab->show();
                tab->expandTab();
                auto basePart = requireNonNull(CSM->parts().value("Base_Bracket"), "CSM part 'Base_Bracket'");
                basePart->getSb()->setSetting(PS::Layer::kLayerHeight, static_cast<double>(Distance(6.0 * mm)()));
                basePart->createSettingsRange(0, 5);
                auto range = requireNonNull(basePart->getSettingsRange(0, 5), "Base_Bracket settings range 0..5");
                range->getSb()->setSetting(PS::Layer::kBeadWidth, static_cast<double>(Distance(12.0 * mm)()));
                profileBar->settingsBasesSelected({"", {}}, {});
                profileBar->refreshOverrideIndicators();
                QApplication::sendPostedEvents();
                tab->adjustSize();
                capture("setting_row_anatomy", tab);
                basePart->getSb()->remove(PS::Layer::kLayerHeight);
                basePart->clearSettingsRanges();
                profileBar->refreshOverrideIndicators();
                capturedLayer = true;
            }
            if (toSlug(tab->getName()) == "slicing") {
                const fifojson savedGlobalJson = GSM->getGlobal()->json();
                const QSignalBlocker layerBarBlocker(requireChild<LayerBar>(window));
                tab->show();
                tab->expandTab();
                tab->setSettingBase(GSM->getGlobal());
                profileBar->settingsBasesSelected({"", {}}, {});
                PreferencesManager::getInstance()->setDisabledSettingVisibilityPreference(
                    DisabledSettingVisibility::kHide);

                auto refreshSlicingTab = [&]() {
                    for (auto& row : tab->getRows()) {
                        row->show();
                        row->reloadValue();
                    }
                    for (auto& row : tab->getRows()) { row->checkDependencies(); }
                    tab->refreshGroupVisibility();
                    QApplication::sendPostedEvents();
                    tab->adjustSize();
                };

                GSM->getGlobal()->setSetting(PS::Slicing::kSlicingMode, static_cast<int>(SlicingMode::kPlanar));
                refreshSlicingTab();
                capture("setting_dependencies_planar", tab);

                GSM->getGlobal()->setSetting(PS::Slicing::kSlicingMode, static_cast<int>(SlicingMode::kCylindrical));
                GSM->getGlobal()->setSetting(QStringLiteral("cylinder_axis_source"), 1);
                refreshSlicingTab();
                capture("setting_dependencies_cylindrical", tab);

                GSM->getGlobal()->json(savedGlobalJson);
                PreferencesManager::getInstance()->setDisabledSettingVisibilityPreference(
                    DisabledSettingVisibility::kGrey);
                refreshSlicingTab();
                capturedSlicing = true;
            }
        }
        if (!capturedLayer) { throw std::runtime_error("Missing Profile > Layer tab for setting_row_anatomy"); }
        if (!capturedSlicing) { throw std::runtime_error("Missing Profile > Slicing tab for dependency captures"); }

        for (QAction* act : requireNonNull(window->menuBar(), "MainWindow::menuBar")->actions()) {
            if (toSlug(act->text()) == "view" && act->menu()) {
                for (QAction* viewAct : act->menu()->actions()) {
                    if (viewAct->menu() && toSlug(viewAct->text()) == "hidden_settings") {
                        viewAct->menu()->adjustSize();
                        capture("hidden_settings_menu", viewAct->menu());
                        capturedHiddenSettings = true;
                    }
                }
            }
        }
        if (!capturedHiddenSettings) { throw std::runtime_error("Missing View > Hidden Settings menu"); }
    }

    if (includeDemos) { captureAllSceneDemos(window, capture); }

    return saved_files;
}

}  // namespace ORNL
