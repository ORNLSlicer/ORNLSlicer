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
#include <QRegularExpression>
#include <QStyleFactory>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QVector>
#include <algorithm>
#include <iostream>

#include "geometry/mesh/mesh_factory.h"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "units/unit.h"
#include "utilities/constants.h"
#include "widgets/gcode_widget.h"
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
    if (auto* glWidget = qobject_cast<QOpenGLWidget*>(widget)) { return glWidget->grabFramebuffer(); }
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

void sliceSampleParts(MainWindow* window) {
    auto* textEdit = window->findChild<QDockWidget*>("m_gcodedock")->findChild<QPlainTextEdit*>();
    QEventLoop loop;
    QObject::connect(textEdit, &QPlainTextEdit::textChanged, &loop, &QEventLoop::quit);
    emit window->findChild<MainToolbar*>()->slice();
    loop.exec();

    auto* cmd    = window->findChild<QDockWidget*>("m_cmddock")->findChild<QTextEdit*>();
    QString text = cmd->toPlainText().replace(QDir::homePath(), "/home/user");
    text.replace(QRegularExpression("Total Slice Time.*"),
                 "Total Slice Time (excluding gcode writing/parsing): 00:00:02.000");
    cmd->setPlainText(text);
    window->findChild<LayerBar*>()->addRange(10, 40);
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
    capture("dock_panels_sidebar", window->findChild<QDockWidget*>("m_settingdock"));
    capture("gcode_editor_panel", window->findChild<QDockWidget*>("m_gcodedock"));
    auto* settingBar = selectSettingsTab(window, 0);
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
