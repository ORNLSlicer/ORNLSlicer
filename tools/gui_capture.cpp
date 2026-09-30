#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QOpenGLWidget>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QTabBar>
#include <QTabWidget>
#include <algorithm>
#include <iostream>

#include "geometry/mesh/mesh_factory.h"
#include "managers/session_manager.h"
#include "units/unit.h"
#include "widgets/gcode_widget.h"
#include "widgets/main_toolbar.h"
#include "widgets/part_widget/part_control/part_control.h"
#include "widgets/part_widget/part_toolbar.h"
#include "widgets/part_widget/part_widget.h"
#include "widgets/settings/setting_bar.h"
#include "widgets/settings/setting_pane.h"
#include "widgets/settings/setting_tab.h"
#include "windows/flowratecalc.h"
#include "windows/gcode_export.h"
#include "windows/layer_times_window.h"
#include "windows/main_window.h"
#include "windows/preferences_window.h"
#include "windows/xtrudecalc.h"

namespace ORNL {

namespace {

const QStringList standardGuiElements = {
    "main_window_overview",   "main_toolbar",
    "part_view_workspace",    "dock_panels_sidebar",
    "menu_bar_menus",         "transform_controls",
    "settings_sidebar",       "layer_times_window",
    "preferences_window",     "gcode_export_dialog",
    "flowrate_calculator",    "xtrude_calculator",
    "printer_settings_panel", "material_settings_panel",
    "profile_settings_panel", "experimental_settings_panel",
};

template <typename T>
void enableAll(const QList<T*>& items) {
    for (T* item : items) {
        if (item) { item->setEnabled(true); }
    }
}

void ensureSampleParts() {
    if (!CSM->parts().isEmpty()) { return; }

    auto base = QSharedPointer<ClosedMesh>::create(MeshFactory::CreateBoxMesh(1200.0 * mm, 800.0 * mm, 250.0 * mm));
    CSM->addPart(base, "Base_Bracket", MeshType::kBuild);
    if (qApp) { qApp->processEvents(); }
}

template <typename T>
QWidget* showWidget(MainWindow* window, bool enable = false) {
    QWidget* w = window->findChild<T*>();
    if (w) {
        if (enable) { w->setEnabled(true); }
        w->show();
        w->adjustSize();
    }
    return w;
}

void sanitizeSettingBar(QWidget* bar) {
    if (!bar) { return; }
    for (QLabel* label : bar->findChildren<QLabel*>()) {
        if (label->text().contains("Currently searching in")) {
            label->setText("Currently searching in: <settings-path>");
            label->hide();
        }
    }
}

QWidget* prepareSettingsPanel(MainWindow* window, int tabIndex) {
    auto* bar = window->findChild<SettingBar*>();
    sanitizeSettingBar(bar);
    bar->closeAll();
    bar->findChild<QTabWidget*>()->setCurrentIndex(tabIndex);
    return bar;
}

QWidget* prepareDockPanelsSidebar(MainWindow* window) {
    prepareSettingsPanel(window, 0);
    auto* dock = window->findChild<QDockWidget*>("m_settingdock");
    dock->show();
    return dock;
}

QWidget* preparePreferencesWindow(MainWindow* window) {
    auto* w = window->findChild<PreferencesWindow*>();
    w->show();

    auto* tabWidget   = w->findChild<QTabWidget*>();
    const int margins = w->layout()->contentsMargins().left() + w->layout()->contentsMargins().right();
    w->resize(std::max(w->width(), tabWidget->tabBar()->sizeHint().width() + margins), w->height());
    w->adjustSize();
    tabWidget->setCurrentIndex(0);
    return w;
}

QWidget* prepareLayerTimesWindow(MainWindow* window) {
    auto* w = window->findChild<LayerTimesWindow*>();
    w->findChild<QLineEdit*>()->setText("20");
    w->updateTimeInformation(QList<Time>(15, Time(25.0)), QList<Time>(15, Time(28.0)), QList<double>(15, 0.92), true);
    w->show();
    w->adjustSize();
    return w;
}

QString toSlug(const QString& name) {
    QString slug = name.toLower();
    slug.replace(QRegularExpression("[^a-z0-9]+"), "_");
    slug.replace(QRegularExpression("^_+|_+$"), "");
    return slug;
}

QImage captureWidget(QWidget* widget) {
    if (auto* glWidget = qobject_cast<QOpenGLWidget*>(widget)) { return glWidget->grabFramebuffer(); }
    widget->ensurePolished();
    return widget->grab().toImage();
}

bool saveWidget(QWidget* widget, const QString& filePath, const char* format = "PNG") {
    const QImage img = captureWidget(widget);
    if (img.isNull()) { return false; }

    QDir().mkpath(QFileInfo(filePath).path());
    return img.save(filePath, format);
}

QWidget* resolveElementWidget(MainWindow* window, const QString& semantic_name) {
    ensureSampleParts();
    sanitizeSettingBar(window->findChild<SettingBar*>());

    if (semantic_name == "main_window_overview") { return window; }
    if (semantic_name == "main_toolbar") { return window->findChild<MainToolbar*>(); }
    if (semantic_name == "menu_bar_menus") { return window->menuBar(); }
    if (semantic_name == "layer_times_window") { return prepareLayerTimesWindow(window); }
    if (semantic_name == "dock_panels_sidebar") { return prepareDockPanelsSidebar(window); }
    if (semantic_name == "transform_controls") { return showWidget<PartToolbar>(window, true); }
    if (semantic_name == "part_control") { return showWidget<PartControl>(window); }
    if (semantic_name == "preferences_window") { return preparePreferencesWindow(window); }
    if (semantic_name == "gcode_export_dialog") { return showWidget<GcodeExport>(window); }
    if (semantic_name == "flowrate_calculator") { return showWidget<FlowrateCalcWindow>(window); }
    if (semantic_name == "xtrude_calculator") { return showWidget<XtrudeCalcWindow>(window); }

    if (semantic_name == "part_view_workspace") {
        QMetaObject::invokeMethod(window, "switchViews", Qt::DirectConnection, Q_ARG(int, 0));
        return static_cast<QWidget*>(window->findChild<PartWidget*>()->view());
    }

    if (semantic_name == "gcode_view_preview") {
        QMetaObject::invokeMethod(window, "switchViews", Qt::DirectConnection, Q_ARG(int, 1));
        return static_cast<QWidget*>(window->findChild<GCodeWidget*>()->view());
    }

    static const QMap<QString, int> panelMap = {
        {"settings_sidebar", 0},       {"printer_settings_panel", 0},      {"material_settings_panel", 1},
        {"profile_settings_panel", 2}, {"experimental_settings_panel", 3},
    };

    auto it = panelMap.find(semantic_name);
    if (it != panelMap.end()) { return prepareSettingsPanel(window, it.value()); }

    return nullptr;
}

QMap<QString, QString> captureAllGuiElements(MainWindow* window, const QString& outputDirectory,
                                             const QStringList& elements = standardGuiElements) {
    QMap<QString, QString> saved_files;
    const QDir outDir(outputDirectory);

    for (const auto& name : elements) {
        QWidget* target            = resolveElementWidget(window, name);
        const QString semanticFile = outDir.filePath(name + ".png");
        if (target && saveWidget(target, semanticFile)) { saved_files.insert(name, semanticFile); }
    }

    return saved_files;
}

QStringList captureAllMenuBarMenus(MainWindow* window, const QString& outputDirectory) {
    QStringList saved_paths;
    const QDir baseDir(outputDirectory);

    for (QAction* act : window->menuBar()->actions()) {
        QMenu* menu = act->menu();
        if (!menu) { continue; }

        menu->adjustSize();
        menu->show();

        enableAll(menu->actions());
        const QString destFile = baseDir.filePath(QString("menu_bar/%1_menu.png").arg(toSlug(act->text())));
        if (saveWidget(menu, destFile)) { saved_paths << destFile; }
    }

    return saved_paths;
}

QStringList captureAllPreferencesTabs(MainWindow* window, const QString& outputDirectory) {
    QStringList saved_paths;
    auto* prefWindow = preparePreferencesWindow(window);
    auto* tabWidget  = prefWindow->findChild<QTabWidget*>();
    const QDir baseDir(outputDirectory);

    enableAll(prefWindow->findChildren<QWidget*>());
    enableAll(prefWindow->findChildren<QAction*>());

    const QString prefWinFile = baseDir.filePath("preferences_window.png");
    if (saveWidget(prefWindow, prefWinFile)) { saved_paths << prefWinFile; }

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);

        QWidget* page = tabWidget->widget(i);
        if (!page) { continue; }
        page->adjustSize();

        const QString destFile = baseDir.filePath(QString("preferences/%1_tab.png").arg(toSlug(tabWidget->tabText(i))));
        if (saveWidget(page, destFile)) { saved_paths << destFile; }
    }

    return saved_paths;
}

QStringList captureAllSettingsCategories(MainWindow* window, const QString& outputDirectory) {
    QStringList saved_paths;
    auto* settingBar = window->findChild<SettingBar*>();
    sanitizeSettingBar(settingBar);

    auto* tabWidget = settingBar->findChild<QTabWidget*>();
    const QDir baseDir(outputDirectory);

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);
        const QString majorSlug = toSlug(tabWidget->tabText(i));

        auto* pane = qobject_cast<SettingPane*>(tabWidget->widget(i));
        if (!pane) { continue; }

        for (SettingTab* tab : pane->getTabs()) {
            tab->expandTab();
            tab->adjustSize();

            enableAll(tab->findChildren<QWidget*>());
            const QString destFile =
                baseDir.filePath(QString("settings/%1/%2_options.png").arg(majorSlug, toSlug(tab->getName())));
            if (saveWidget(tab, destFile)) { saved_paths << destFile; }
        }
    }

    return saved_paths;
}

}  // namespace

}  // namespace ORNL

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const QString arg = argv[i];
        if ((arg == "-s" || arg == "--scale") && i + 1 < argc) {
            qputenv("QT_SCALE_FACTOR", argv[i + 1]);
            break;
        }
    }

    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("fusion"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Captures designated GUI elements with semantic names for documentation.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption scaleOption(
        QStringList() << "s" << "scale",
        "DPI scale factor for high-resolution captures (e.g. 2 for crisp 2x retina captures).", "factor");
    parser.addOption(scaleOption);

    // TODO: Support selective capture to limit runs to only updated settings or specific areas.
    QCommandLineOption skipSettingsOption("skip-settings", "Skip capturing granular settings dropdown categories.");
    parser.addOption(skipSettingsOption);

    QCommandLineOption settingsOnlyOption(
        "settings-only", "Capture only granular settings dropdown categories, omitting top-level GUI elements.");
    parser.addOption(settingsOnlyOption);

    parser.process(app);

    const QString outputDir  = "docs/user-guide-images";
    const bool runGuiCapture = !parser.isSet(settingsOnlyOption);
    const bool runSettings   = !parser.isSet(skipSettingsOption);

    std::cout << "Starting ORNLSlicer GUI Capture...\n";
    std::cout << "Output directory: " << outputDir.toStdString() << "\n";
    std::cout << "Capture settings: " << (runSettings ? "yes" : "no") << "\n\n";

    ORNL::MainWindow* window = ORNL::MainWindow::getInstance();
    window->show();
    app.processEvents();
    ORNL::sanitizeSettingBar(window->findChild<ORNL::SettingBar*>());

    if (runGuiCapture) {
        const auto savedFiles = ORNL::captureAllGuiElements(window, outputDir);

        std::cout << "Captured " << savedFiles.size() << " GUI elements:\n";
        for (auto it = savedFiles.begin(); it != savedFiles.end(); ++it) {
            std::cout << "  - " << it.key().toStdString() << ": " << it.value().toStdString() << "\n";
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
    }

    if (runSettings) {
        std::cout << "Capturing settings dropdown categories to settings/<major>/<minor>_options.png...\n";
        const auto settingsPaths = ORNL::captureAllSettingsCategories(window, outputDir);
        std::cout << "Captured " << settingsPaths.size() << " setting category options:\n";
        for (const auto& path : settingsPaths) { std::cout << "  - " << path.toStdString() << "\n"; }
        std::cout << "\n";
    }

    std::cout << "GUI capture complete.\n";
    return 0;
}
