#include "windows/main_window.h"

#include "QFile"
#include "QFileDialog"
#include "QSettings"
#include "QStatusBar"
#include "QTimer"
#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "threading/gcode_loader.h"
#include "threading/session_loader.h"
#include "windows/dialogs/cs_dbg.h"
#include "windows/dialogs/template_save.h"

namespace ORNL {

MainWindow* MainWindow::m_singleton = nullptr;

MainWindow* MainWindow::getInstance() {
    if (m_singleton == nullptr)
        m_singleton = new MainWindow();
    return m_singleton;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), m_status(false) { continueStartup(); }

void MainWindow::continueStartup() {
    PreferencesManager::getInstance()->importPreferences();

    QString app_path = qApp->applicationDirPath();

    QStringList template_paths = {app_path + "/templates/", app_path + "/../../../templates/",
                                  app_path + "/../share/slicer2/templates/"};

    QString templates;

    for (QString path : template_paths) {
        if (QDir(path).exists()) {
            templates = path;
        }
    }

    if (!templates.isEmpty()) {
        GSM->loadAllGlobals(templates);
    }
    else {
        qWarning() << "Cannot find base templates directory.";
    }

    QString tmp = CSM->getMostRecentSettingFolderLocation();

#ifndef WIN32
    if (tmp == QStandardPaths::writableLocation(QStandardPaths::HomeLocation)) {
        tmp = "/tmp/";
    }
#endif

    GSM->loadAllGlobals(tmp);
    GSM->constructActiveGlobal(CSM->getMostRecentSettingHistory());
    // GSM->loadLayerBarTemplate(qApp->applicationDirPath() + "/layerbartemplates");
    GSM->loadLayerBarTemplate(CSM->getMostRecentLayerBarSettingFolderLocation());
    this->setupClasses();
    this->setupUi();
    CSM->setupTCPServer();
}

MainWindow::~MainWindow() {
    this->teardownClasses();

    if (CSM->parts().count() > 0) {
        CSM->saveSession(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/_lastsession.s2p", false)
            ->wait();
    }

    // Parenting system should take care of all widget teardown.

    m_singleton = nullptr;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (PreferencesManager::getInstance()->getWindowMaximizedPreference() != this->isMaximized())
        PreferencesManager::getInstance()->setWindowMaximizedPreference(this->isMaximized());
    if (PreferencesManager::getInstance()->getWindowSizePreference() != this->size())
        PreferencesManager::getInstance()->setWindowSizePreference(this->size());
    if (PreferencesManager::getInstance()->getWindowPosPreference() != this->pos())
        PreferencesManager::getInstance()->setWindowPosPreference(this->pos());

    QApplication::quit();
}

CmdWidget* MainWindow::getCmdOut() { return m_cmdbar; }

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (PreferencesManager::getInstance()->getWindowMaximizedPreference())
        this->setWindowState(Qt::WindowMaximized);
    else {
        if (PreferencesManager::getInstance()->getWindowSizePreference().isValid()) {
            this->resize(PreferencesManager::getInstance()->getWindowSizePreference());
            this->move(PreferencesManager::getInstance()->getWindowPosPreference());
        }
    }
}

void MainWindow::debug() {}

bool MainWindow::setStyleFromFile() {
    m_style_file =
        QSharedPointer<QFile>(new QFile(PreferencesManager::getInstance()->getTheme().getFolderPath() + "style.qss"));

    if (!m_style_file->open(QIODevice::ReadOnly)) {
        qDebug("Could not open style resource file '%s'.\n",
               (PreferencesManager::getInstance()->getTheme().getFolderPath() + "style.qss").toStdString().c_str());
        m_style_file.clear();
        return false;
    }

    this->setStyleSheet(m_style_file->readAll());
    m_style_file->close();
    return true;
}

void MainWindow::setupClasses() {
    // Timer. This controls auto save.
    m_timer = new QTimer(this);
    m_timer->start(300000);
}

void MainWindow::teardownClasses() {

    if (PreferencesManager::getInstance()->isDirty())
        PreferencesManager::getInstance()->exportPreferences();
}

void MainWindow::setupUi() {
    this->setStyleFromFile();
    this->setupWindows();
    this->setupWidgets();
    this->setupDocks();
    this->setupLayouts();
    this->setupBars();
    this->setupActions();
    this->setupInsert();
    this->setupEvents();

    this->retranslateUi();
    QMetaObject::connectSlotsByName(this);
}

void MainWindow::setupWindows() {
    // Main Window
    if (this->objectName().isEmpty())
        this->setObjectName(QStringLiteral("MainWindow"));
    this->resize(Constants::UI::MainWindow::kWindowSize);

    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/slicer-2_logo.png"), QSize(), QIcon::Normal, QIcon::Off);
    this->setWindowIcon(icon);

    // Preferences Window
    m_pref_window = new PreferencesWindow(this);
    m_flowrate_calc_window = new FlowrateCalcWindow(this);
    m_xtrude_calc_window = new XtrudeCalcWindow(this);
    m_export_window = new GcodeExport(this);
    m_layertimebar = new LayerTimesWindow(this);
    // m_ingersollPostProcessor = new IngersollPostProcessor(this);
    m_about_window = new AboutWindow(this);
    m_external_file_window = new ExternalFileWindow(this);
}

void MainWindow::setupWidgets() {
    QSizePolicy sizePolicyHStretch(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sizePolicyHStretch.setHorizontalStretch(1);
    sizePolicyHStretch.setVerticalStretch(0);

    QSizePolicy sizePolicyNoStretch(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizePolicyNoStretch.setHorizontalStretch(0);
    sizePolicyNoStretch.setVerticalStretch(0);

    // Main Widget
    m_main_widget = new QWidget(this);
    m_main_widget->setObjectName(QStringLiteral("m_main_widget"));

    // Main Container
    m_main_container = new QWidget(m_main_widget);
    m_main_container->setObjectName(QStringLiteral("m_main_container"));
    m_main_container->setSizePolicy(sizePolicyHStretch);
    sizePolicyHStretch.setHeightForWidth(m_main_container->sizePolicy().hasHeightForWidth());

    m_tab_widget = new QTabWidget(m_main_container);
    m_tab_widget->setObjectName(QStringLiteral("m_tab_widget"));
    m_tab_widget->setMinimumSize(Constants::UI::MainWindow::kViewWidgetSize);
    m_tab_widget->setSizePolicy(sizePolicyHStretch);
    sizePolicyHStretch.setHeightForWidth(m_tab_widget->sizePolicy().hasHeightForWidth());

    // Hide tabbar
    QTabBar* tabBar = m_tab_widget->findChild<QTabBar*>();
    tabBar->hide();

    // Part Viewer
    m_part_widget = new ORNL::PartWidget(m_tab_widget);
    m_part_widget->setObjectName(QStringLiteral("m_part_widget"));
    m_part_widget->setSizePolicy(sizePolicyHStretch);
    // colors need to be specified in base_view.cpp > intializeGL
    sizePolicyHStretch.setHeightForWidth(m_part_widget->sizePolicy().hasHeightForWidth());

    // GCode Viewer
    m_gcode_widget = new ORNL::GCodeWidget(m_tab_widget);
    m_gcode_widget->setObjectName(QStringLiteral("m_gcode"));
    m_gcode_widget->setPartMeta(m_part_widget->getPartMeta());

    // Layerbar
    m_layerbar = new ORNL::LayerBar(m_part_widget->getPartMeta(), m_main_container);
    m_layerbar->setObjectName(QStringLiteral("m_layerbar"));
    m_layerbar->setMinimumSize(Constants::UI::MainWindow::kLayerbarMinSize);
    // m_layerbar->setMinimumSize(QSize(40, 0));

    // Settingbar
    m_settingbar = new ORNL::SettingBar(CSM->getMostRecentSettingHistory());
    m_settingbar->setObjectName(QStringLiteral("m_setting_tab"));
    m_settingbar->setCurrentFolder(CSM->getMostRecentSettingFolderLocation());

    QIcon setting_icon;
    setting_icon.addFile(QStringLiteral(":/icons/settings_black.png"), QSize(), QIcon::Normal, QIcon::Off);

    // Sidebar TabWidget G-Code Editor Tab
    m_gcodebar = new ORNL::GcodeBar();
    m_gcodebar->setObjectName(QStringLiteral("m_gcode_tab"));
    QIcon gcode_icon;
    gcode_icon.addFile(QStringLiteral(":/icons/edit_property_black.png"), QSize(), QIcon::Normal, QIcon::Off);

    // Cmdbar Container
    m_cmdbar = new ORNL::CmdWidget;
    m_cmdbar->setObjectName(QStringLiteral("m_cmdbar"));

    // Progress bar
    m_progressbar = new QProgressBar(this);
    m_progressbar->setFixedWidth(Constants::UI::MainWindow::kProgressBarWidth);
    m_progressbar->setMinimum(0);
    m_progressbar->setMaximum(0);
    m_progressbar->hide();
}

void MainWindow::setupDocks() {
    // SettingDock
    m_settingdock = new QDockWidget(this);
    m_settingdock->setObjectName(QStringLiteral("m_settingdock"));
    //        m_settingdock->setFeatures(QDockWidget::AllDockWidgetFeatures);
    m_settingdock->setMinimumWidth(Constants::UI::MainWindow::SideDock::kSettingsWidth);
    m_settingdock->setMinimumWidth(610);

    // GCodeDock
    m_gcodedock = new QDockWidget(this);
    m_gcodedock->setObjectName(QStringLiteral("m_gcodedock"));
    //        m_gcodedock->setFeatures(QDockWidget::AllDockWidgetFeatures);
    m_gcodedock->setMinimumWidth(Constants::UI::MainWindow::SideDock::kGCodeWidth);

    // layer times dock
    m_layertimesdock = new QDockWidget(this);
    m_layertimesdock->setObjectName(QStringLiteral("m_layertimesdock"));
    //        m_layertimesdock->setFeatures(QDockWidget::AllDockWidgetFeatures);
    m_layertimesdock->setMinimumWidth(Constants::UI::MainWindow::SideDock::kLayerTimesWidth);

    // external file dock
    m_external_file_dock = new QDockWidget(this);
    m_external_file_dock->setObjectName(QStringLiteral("m_external_file_dock"));
    //        m_external_file_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    m_external_file_dock->setMinimumWidth(Constants::UI::MainWindow::SideDock::kExternalFileWidth);

    // CmdDock
    m_cmddock = new QDockWidget(this);
    m_cmddock->setObjectName(QStringLiteral("m_cmddock"));

    // Set the Sidebar to have precedence over the commandbar.
    this->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    // Set the Sidebar to have precedence over the main control
    this->setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
}

void MainWindow::setupLayouts() {
    // Main Layout
    m_main_layout = new QGridLayout(m_main_widget);
    m_main_layout->setSpacing(Constants::UI::MainWindow::Margins::kMainLayoutSpacing);
    m_main_layout->setContentsMargins(
        Constants::UI::MainWindow::Margins::kMainLayout, Constants::UI::MainWindow::Margins::kMainLayout,
        Constants::UI::MainWindow::Margins::kMainLayout, Constants::UI::MainWindow::Margins::kMainLayout);
    m_main_layout->setObjectName(QStringLiteral("m_main_layout"));

    // Main Container Layout
    m_main_container_layout = new QHBoxLayout(m_main_container);
    m_main_container_layout->setSpacing(Constants::UI::MainWindow::Margins::kMainContainerSpacing);
    m_main_container_layout->setContentsMargins(
        Constants::UI::MainWindow::Margins::kMainContainer, Constants::UI::MainWindow::Margins::kMainContainer,
        Constants::UI::MainWindow::Margins::kMainContainer, Constants::UI::MainWindow::Margins::kMainContainer);
    m_main_container_layout->setObjectName(QStringLiteral("m_main_container_layout"));
}

void MainWindow::setupBars() {
    // Menubar
    m_menubar = new QMenuBar(this);
    m_menubar->setObjectName(QStringLiteral("menuBar"));
    m_menubar->setGeometry(QRect(0, 0, Constants::UI::MainWindow::kWindowSize.width(), 19));
    m_menu_file = new QMenu(m_menubar);
    m_menu_file->setObjectName(QStringLiteral("m_menu_file"));
    m_menu_edit = new QMenu(m_menubar);
    m_menu_edit->setObjectName(QStringLiteral("m_menu_edit"));
    m_menu_view = new QMenu(m_menubar);
    m_menu_view->setObjectName(QStringLiteral("m_menu_view"));
    m_menu_zoom = new QMenu(m_menubar);
    m_menu_zoom->setObjectName(QStringLiteral("m_menu_zoom"));
    m_menu_hidden_settings = new QMenu(m_menubar);
    m_menu_hidden_settings->setObjectName(QStringLiteral("m_menu_hidden_settings"));
    m_menu_hidden_printer_settings = new QMenu(m_menubar);
    m_menu_hidden_printer_settings->setObjectName(QStringLiteral("m_menu_hidden_printer_settings"));
    m_menu_hidden_material_settings = new QMenu(m_menubar);
    m_menu_hidden_material_settings->setObjectName(QStringLiteral("m_menu_hidden_material_settings"));
    m_menu_hidden_profile_settings = new QMenu(m_menubar);
    m_menu_hidden_profile_settings->setObjectName(QStringLiteral("m_menu_hidden_profile_settings"));
    m_menu_hidden_experimental_settings = new QMenu(m_menubar);
    m_menu_hidden_experimental_settings->setObjectName(QStringLiteral("m_menu_hidden_experimental_settings"));

    // create mapping for addition/removal of hidden settings
    m_setting_panel_to_menu_map.insert("Printer", m_menu_hidden_printer_settings);
    m_setting_panel_to_menu_map.insert("Material", m_menu_hidden_material_settings);
    m_setting_panel_to_menu_map.insert("Profile", m_menu_hidden_profile_settings);
    m_setting_panel_to_menu_map.insert("Experimental", m_menu_hidden_experimental_settings);

    m_menu_settings = new QMenu(m_menubar);
    m_menu_settings->setObjectName(QStringLiteral("m_menu_settings"));
    m_menu_project = new QMenu(m_menubar);
    m_menu_project->setObjectName(QStringLiteral("m_menu_project"));
    m_menu_tools = new QMenu(m_menubar);
    m_menu_tools->setObjectName(QStringLiteral("m_menu_tools"));
    m_menu_scripts = new QMenu(m_menubar);
    m_menu_scripts->setObjectName(QStringLiteral("m_menu_scripts"));
    m_menu_help = new QMenu(m_menubar);
    m_menu_help->setObjectName(QStringLiteral("m_menu_help"));
    m_menu_debug = new QMenu(m_menubar);
    m_menu_debug->setObjectName(QStringLiteral("m_menu_debug"));

    // Main Toolbar
    m_main_toolbar = new MainToolbar(m_main_container);
    m_main_toolbar->setObjectName(QStringLiteral("m_main_toolbar"));

    // Status bar.
    m_statusbar = new QStatusBar(this);
}

void MainWindow::setupActions() {
    // To define a new menu option, add a new initalizer struct to the list below with the icon and the display name as
    // shown. Then below, add the action to the location in the menu desired.

    // Menu File
    m_actions["sel_printer"] = {"Select Printer", ":/icons/3d_printer.png", false, QKeySequence(), nullptr};
    m_actions["load_model"] = {"Load Model for Building", ":/icons/file_black.png", false, QKeySequence(tr("Ctrl+o")),
                               nullptr};
    m_actions["load_point_cloud"] = {"Load Point Cloud", ":/icons/file_cloud_black.png", false, QKeySequence(),
                                     nullptr};
    m_actions["last_session"] = {"Restore Last Session", ":/icons/restore_session_black.png", false, QKeySequence(),
                                 nullptr};
    m_actions["slice"] = {"Slice", ":/icons/layers_black.png", false, QKeySequence(tr("Ctrl+g")), nullptr};
    m_actions["screenshot"] = {"Take Screenshot", ":/icons/screenshot_black.png", false, QKeySequence(), nullptr};
    m_actions["exit"] = {"Exit", ":/icons/exit_black.png", false, QKeySequence(), nullptr};

    // Menu Edit
    m_actions["undo"] = {"Undo", ":/icons/undo_black.png", false, QKeySequence(), nullptr};
    m_actions["redo"] = {"Redo", ":/icons/redo_black.png", false, QKeySequence(), nullptr};
    m_actions["copy"] = {"Copy", ":/icons/copy_black.png", false, QKeySequence(tr("Ctrl+c")), nullptr};
    m_actions["paste"] = {"Paste", ":/icons/paste_black.png", false, QKeySequence(tr("Ctrl+v")), nullptr};
    m_actions["reload"] = {"Reload", ":/icons/file_refresh_black.png", false, QKeySequence(tr("Ctrl+r")), nullptr};
    m_actions["delete"] = {"Delete", ":/icons/delete_black.png", false, QKeySequence(tr("Ctrl+Delete")), nullptr};

    // Menu Zoom
    m_actions["zoom_in"] = {"Zoom In", ":/icons/magnify_plus_black.png", false, QKeySequence(tr("Ctrl+=")), nullptr};
    m_actions["zoom_out"] = {"Zoom Out", ":/icons/magnify_minus_black.png", false, QKeySequence(tr("Ctrl+-")), nullptr};
    m_actions["zoom_reset"] = {"Reset", ":/icons/magnify_reset_black.png", false, QKeySequence(tr("Ctrl+0")), nullptr};

    // Menu Hidden Settings
    m_actions["show_all"] = {"Show All Settings", ":/icons/eye_black.png", false, QKeySequence(), nullptr};
    m_actions["show_all_printer"] = {"Show All Printer Settings", ":/icons/eye_black.png", false, QKeySequence(),
                                     nullptr};
    m_actions["show_all_material"] = {"Show All Material Settings", ":/icons/eye_black.png", false, QKeySequence(),
                                      nullptr};
    m_actions["show_all_profile"] = {"Show All Profile Settings", ":/icons/eye_black.png", false, QKeySequence(),
                                     nullptr};
    m_actions["show_all_experimental"] = {"Show All Experimental Settings", ":/icons/eye_black.png", false,
                                          QKeySequence(), nullptr};

    // Menu View
    m_actions["part_view"] = {"Part View", ":/icons/cube_black.png", false, QKeySequence(), nullptr};
    m_actions["gcode_view"] = {"G-Code View", ":/icons/3d_black.png", false, QKeySequence(), nullptr};
    m_actions["reset_camera"] = {"Reset Camera", ":/icons/camera_reset_black.png", false, QKeySequence(), nullptr};

    // Menu Settings
    m_actions["template_load"] = {"Load from Template", ":/icons/settings_file_black.png", false,
                                  QKeySequence(tr("Ctrl+t")), nullptr};
    m_actions["template_save"] = {"Save as Template", ":/icons/settings_save_black.png", false,
                                  QKeySequence(tr("Ctrl+Shift+t")), nullptr};
    m_actions["setting_folder"] = {"Additional Setting Location", ":/icons/settings_folder_black.png", false,
                                   QKeySequence(), nullptr};
    m_actions["layer_bar_setting_folder"] = {"Additional Layer Bar Setting Location",
                                             ":/icons/settings_folder_black.png", false, QKeySequence(), nullptr};
    m_actions["pref"] = {"Application Preferences", ":/icons/settings_black.png", false, QKeySequence(tr("Ctrl+p")),
                         nullptr};

    // Menu Project
    m_actions["project_new"] = {"New Project", ":/icons/file_add_black.png", false, QKeySequence(), nullptr};
    m_actions["project_load"] = {"Load Project", ":/icons/folder_black.png", false, QKeySequence(tr("Ctrl+Shift+o")),
                                 nullptr};
    m_actions["project_save"] = {"Save Project", ":/icons/save_project_black.png", false, QKeySequence(tr("Ctrl+s")),
                                 nullptr};
    m_actions["gcode_import"] = {"Import G-Code", ":/icons/import_black.png", false, QKeySequence(), nullptr};
    m_actions["gcode_export"] = {"Export G-Code", ":/icons/export_black.png", false, QKeySequence(), nullptr};

    // Menu Tools
    m_actions["flowrate_calc"] = {"Flowrate Calculator", ":/icons/calculator_black.png", false, QKeySequence(),
                                  nullptr};
    m_actions["xtrude_calc"] = {"Xtrude Calculator", ":/icons/print_head.png", false, QKeySequence(), nullptr};
    m_actions["build_log"] = {"Build Log", ":/icons/list.png", false, QKeySequence(), nullptr};
    m_actions["remote_connectivity"] = {"Remote Connectivity", ":/icons/remote_connect_black.png", false,
                                        QKeySequence(), nullptr};

    // Menu Scripts
    m_actions["python_int"] = {"Python Interpreter", ":/icons/python.ico", false, QKeySequence(), nullptr};

    // Menu Help
    m_actions["manual"] = {"User's Manual", ":/icons/help_black.png", false, QKeySequence(), nullptr};
    m_actions["repo"] = {"Open Website/Repository", ":/icons/web_black.png", false, QKeySequence(), nullptr};
    m_actions["bug"] = {"Report Bug", ":/icons/bug_black.png", false, QKeySequence(), nullptr};
    m_actions["about_s2"] = {"About ORNL Slicer-2", ":/icons/slicer-2_logo.png", false, QKeySequence(), nullptr};
    m_actions["about_qt"] = {"About Qt", ":/icons/qt.png", false, QKeySequence(), nullptr};

    // Menu Debug
    m_actions["debug"] = {"Run MainWindow::debug()", ":/icons/test_black.png", false, QKeySequence(tr("Ctrl+Space")),
                          nullptr};
    m_actions["cs_view"] = {"Cross-section Viewer", ":/icons/layers_black.png", false, QKeySequence(), nullptr};

    // Setup all defined actions above.
    for (menu_info& curr_act : m_actions) {
        createAction(curr_act);
        this->addAction(curr_act.action);
    }

    // Add the menus to the main bar.
    m_menubar->addAction(m_menu_file->menuAction());
    m_menubar->addAction(m_menu_edit->menuAction());
    m_menubar->addAction(m_menu_view->menuAction());
    m_menubar->addAction(m_menu_settings->menuAction());
    m_menubar->addAction(m_menu_project->menuAction());
    m_menubar->addAction(m_menu_tools->menuAction());
    m_menubar->addAction(m_menu_scripts->menuAction());
    m_menubar->addAction(m_menu_help->menuAction());
    m_menubar->addAction(m_menu_debug->menuAction());

    // Add the actions to the menus.
    // File
    m_menu_file->addAction(m_actions["sel_printer"].action);
    m_menu_file->addAction(m_actions["load_model"].action);
    m_menu_file->addAction(m_actions["load_point_cloud"].action);
    m_menu_file->addAction(m_actions["last_session"].action);
    m_menu_file->addSeparator();
    m_menu_file->addAction(m_actions["slice"].action);
    m_menu_file->addSeparator();
    m_menu_file->addAction(m_actions["screenshot"].action);
    m_menu_file->addSeparator();
    m_menu_file->addAction(m_actions["exit"].action);

    // Edit
    m_menu_edit->addAction(m_actions["undo"].action);
    m_menu_edit->addAction(m_actions["redo"].action);
    m_menu_edit->addSeparator();
    m_menu_edit->addAction(m_actions["copy"].action);
    m_menu_edit->addAction(m_actions["paste"].action);
    m_menu_edit->addAction(m_actions["reload"].action);
    m_menu_edit->addAction(m_actions["delete"].action);

    // View > Toolbars is auto generated.

    // View > Zoom
    m_menu_zoom->addAction(m_actions["zoom_in"].action);
    m_menu_zoom->addAction(m_actions["zoom_out"].action);
    m_menu_zoom->addSeparator();
    m_menu_zoom->addAction(m_actions["zoom_reset"].action);

    // View > Hidden Settings
    m_menu_hidden_settings->addAction(m_actions["show_all"].action);
    m_menu_hidden_settings->addSeparator();
    m_menu_hidden_settings->addMenu(m_menu_hidden_printer_settings);
    m_menu_hidden_settings->addMenu(m_menu_hidden_material_settings);
    m_menu_hidden_settings->addMenu(m_menu_hidden_profile_settings);
    m_menu_hidden_settings->addMenu(m_menu_hidden_experimental_settings);

    m_menu_hidden_printer_settings->addAction(m_actions["show_all_printer"].action);
    m_menu_hidden_printer_settings->addSeparator();
    m_menu_hidden_printer_settings->addActions(setupSettingActions(m_menu_hidden_printer_settings, "Printer"));

    m_menu_hidden_material_settings->addAction(m_actions["show_all_material"].action);
    m_menu_hidden_material_settings->addSeparator();
    m_menu_hidden_material_settings->addActions(setupSettingActions(m_menu_hidden_material_settings, "Material"));

    m_menu_hidden_profile_settings->addAction(m_actions["show_all_profile"].action);
    m_menu_hidden_profile_settings->addSeparator();
    m_menu_hidden_profile_settings->addActions(setupSettingActions(m_menu_hidden_profile_settings, "Profile"));

    m_menu_hidden_experimental_settings->addAction(m_actions["show_all_experimental"].action);
    m_menu_hidden_experimental_settings->addSeparator();
    m_menu_hidden_experimental_settings->addActions(
        setupSettingActions(m_menu_hidden_experimental_settings, "Experimental"));

    // View
    m_menu_view->addAction(m_actions["part_view"].action);
    m_menu_view->addAction(m_actions["gcode_view"].action);
    m_menu_view->addSeparator();
    m_menu_view->addAction(m_actions["reset_camera"].action);
    m_menu_view->addSeparator();
    m_menu_view->addMenu(m_menu_zoom);
    m_menu_view->addSeparator();
    m_menu_view->addMenu(m_menu_hidden_settings);
    m_menu_view->addSeparator();

    // Settings
    m_menu_settings->addAction(m_actions["template_load"].action);
    m_menu_settings->addAction(m_actions["template_save"].action);
    m_menu_settings->addSeparator();
    m_menu_settings->addAction(m_actions["setting_folder"].action);
    m_menu_settings->addAction(m_actions["layer_bar_setting_folder"].action);
    m_menu_settings->addSeparator();
    m_menu_settings->addAction(m_actions["pref"].action);

    // Project
    m_menu_project->addAction(m_actions["project_new"].action);
    m_menu_project->addAction(m_actions["project_load"].action);
    m_menu_project->addAction(m_actions["project_save"].action);
    m_menu_project->addSeparator();
    m_menu_project->addAction(m_actions["gcode_import"].action);
    m_menu_project->addAction(m_actions["gcode_export"].action);

    // Tools
    m_menu_tools->addAction(m_actions["flowrate_calc"].action);
    m_menu_tools->addAction(m_actions["xtrude_calc"].action);
    m_menu_tools->addAction(m_actions["remote_connectivity"].action);

    // Scripts
    m_menu_scripts->addAction(m_actions["python_int"].action);

    // Help
    m_menu_help->addAction(m_actions["manual"].action);
    m_menu_help->addSeparator();
    m_menu_help->addAction(m_actions["repo"].action);
    m_menu_help->addAction(m_actions["bug"].action);
    m_menu_help->addSeparator();
    m_menu_help->addAction(m_actions["about_s2"].action);
    m_menu_help->addAction(m_actions["about_qt"].action);

    // Debug
    m_menu_debug->addAction(m_actions["debug"].action);
    m_menu_debug->addAction(m_actions["cs_view"].action);
}

void MainWindow::createAction(menu_info& info) {
    info.action = new QAction(this);
    info.action->setIcon(QIcon(info.icon));
    info.action->setShortcut(info.shortcut);
    info.action->setShortcutContext(Qt::ApplicationShortcut);
    info.action->setCheckable(info.checkable);
}

void MainWindow::setupInsert() {
    m_main_container_layout->addWidget(m_tab_widget);
    m_main_container_layout->addWidget(m_layerbar);

    m_main_layout->addWidget(m_main_container, 0, 0, 1, 1);

    m_cmddock->setWidget(m_cmdbar);
    m_settingdock->setWidget(m_settingbar);
    m_gcodedock->setWidget(m_gcodebar);
    m_layertimesdock->setWidget(m_layertimebar);
    m_external_file_dock->setWidget(m_external_file_window);
    m_statusbar->addPermanentWidget(m_progressbar);

    m_tab_widget->addTab(m_part_widget, "Object View");
    m_tab_widget->addTab(m_gcode_widget, "GCode View");

    this->setStatusBar(m_statusbar);

    this->addDockWidget(static_cast<Qt::DockWidgetArea>(2), m_gcodedock);
    this->addDockWidget(static_cast<Qt::DockWidgetArea>(2), m_settingdock);
    this->addDockWidget(static_cast<Qt::DockWidgetArea>(2), m_layertimesdock);
    this->addDockWidget(static_cast<Qt::DockWidgetArea>(2), m_external_file_dock);

    m_cmddock->setMaximumHeight(Constants::UI::MainWindow::kStatusBarMaxHeight);

    this->tabifyDockWidget(m_settingdock, m_gcodedock);
    this->tabifyDockWidget(m_gcodedock, m_layertimesdock);
    this->tabifyDockWidget(m_layertimesdock, m_external_file_dock);
    this->setTabPosition(static_cast<Qt::DockWidgetArea>(2), QTabWidget::West);
    this->addDockWidget(static_cast<Qt::DockWidgetArea>(2), m_cmddock);
    m_settingdock->raise();

    this->setMenuBar(m_menubar);

    // After inserting all docks/bars, then create the toolbars menu.
    m_menu_toolbars = this->createPopupMenu();
    m_menu_view->addMenu(m_menu_toolbars);

    this->setCentralWidget(m_main_widget);
}

void MainWindow::setupEvents() {
    // Menu connections.
    connect(m_actions["last_session"].action, &QAction::triggered, this, &MainWindow::autoLoad);
    connect(m_actions["screenshot"].action, &QAction::triggered, m_part_widget, &PartWidget::takeScreenshot);

    connect(m_actions["undo"].action, &QAction::triggered, m_part_widget, &PartWidget::undo);
    connect(m_actions["redo"].action, &QAction::triggered, m_part_widget, &PartWidget::redo);
    connect(m_actions["copy"].action, &QAction::triggered, this, [this]() {
        m_actions["paste"].action->setEnabled(true);
        m_part_widget->copy();
    });
    connect(m_actions["paste"].action, &QAction::triggered, m_part_widget, &PartWidget::paste);
    connect(m_actions["reload"].action, &QAction::triggered, m_part_widget, QOverload<>::of(&PartWidget::reload));
    connect(m_actions["delete"].action, &QAction::triggered, m_part_widget, QOverload<>::of(&PartWidget::remove));

    m_actions["slice"].action->setEnabled(false);
    m_actions["copy"].action->setEnabled(false);
    m_actions["paste"].action->setEnabled(false);
    m_actions["reload"].action->setEnabled(false);
    m_actions["delete"].action->setEnabled(false);
    m_actions["gcode_export"].action->setEnabled(false);
    m_actions["build_log"].action->setEnabled(false);

    m_actions["sel_printer"].action->setEnabled(false);
    m_actions["python_int"].action->setEnabled(false);
    m_actions["debug"].action->setEnabled(true);

    connect(m_actions["zoom_in"].action, &QAction::triggered, m_part_widget, &PartWidget::zoomIn);
    connect(m_actions["zoom_out"].action, &QAction::triggered, m_part_widget, &PartWidget::zoomOut);
    connect(m_actions["zoom_reset"].action, &QAction::triggered, m_part_widget, &PartWidget::resetZoom);

    connect(m_actions["show_all"].action, &QAction::triggered, this, [this]() {
        for (QString key : this->m_setting_panel_to_menu_map.keys()) {
            QMenu* menu = this->m_setting_panel_to_menu_map[key];
            this->showAllSettings(key, menu);
        }
    });

    connect(m_actions["show_all_printer"].action, &QAction::triggered, this,
            [this]() { this->showAllSettings("Printer", m_menu_hidden_printer_settings); });

    connect(m_actions["show_all_material"].action, &QAction::triggered, this,
            [this]() { this->showAllSettings("Material", m_menu_hidden_material_settings); });

    connect(m_actions["show_all_profile"].action, &QAction::triggered, this,
            [this]() { this->showAllSettings("Profile", m_menu_hidden_profile_settings); });

    connect(m_actions["show_all_experimental"].action, &QAction::triggered, this,
            [this]() { this->showAllSettings("Experimental", m_menu_hidden_experimental_settings); });

    connect(m_actions["gcode_view"].action, &QAction::triggered, this, [this]() {
        switchViews(1);
        m_main_toolbar->setView(1);
    });
    connect(m_actions["part_view"].action, &QAction::triggered, this, [this]() {
        switchViews(0);
        m_main_toolbar->setView(0);
    });
    connect(m_actions["reset_camera"].action, &QAction::triggered, m_part_widget, &PartWidget::resetCamera);

    connect(m_actions["load_model"].action, &QAction::triggered, this, [this]() { loadModel(MeshType::kBuild); });
    connect(m_actions["load_point_cloud"].action, &QAction::triggered, this, [this]() { loadPointCloud(); });
    connect(m_actions["slice"].action, &QAction::triggered, m_part_widget, &PartWidget::preSliceUpdate);
    connect(m_actions["exit"].action, &QAction::triggered, qApp, &QApplication::quit);

    connect(m_actions["template_load"].action, &QAction::triggered, this, &MainWindow::loadTemplate);
    connect(m_actions["template_save"].action, &QAction::triggered, this, &MainWindow::saveTemplate);
    connect(m_actions["setting_folder"].action, &QAction::triggered, this, &MainWindow::setSettingFolder);
    connect(m_actions["layer_bar_setting_folder"].action, &QAction::triggered, this,
            &MainWindow::setLayerBarSettingFolder);
    connect(m_actions["pref"].action, &QAction::triggered, m_pref_window, [this] {
        m_pref_window->raise();
        m_pref_window->showNormal();
    });

    connect(m_actions["manual"].action, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(qApp->applicationDirPath() + "/../share/doc/slicer2/slicer-2_user_guide.pdf"));
    });
    connect(m_actions["repo"].action, &QAction::triggered, this,
            [this] { QDesktopServices::openUrl(QUrl("https://github.com/ORNLSlicer/Slicer-2")); });
    connect(m_actions["bug"].action, &QAction::triggered, this,
            [this] { QDesktopServices::openUrl(QUrl("https://github.com/ORNLSlicer/Slicer-2/issues")); });
    connect(m_actions["about_s2"].action, &QAction::triggered, m_about_window, [this] {
        m_about_window->raise();
        m_about_window->showNormal();
    });
    connect(m_actions["about_qt"].action, &QAction::triggered, qApp, &QApplication::aboutQt);

    connect(m_actions["gcode_import"].action, &QAction::triggered, this, &MainWindow::importGCode);
    connect(m_actions["gcode_export"].action, &QAction::triggered, m_export_window, [this] {
        m_export_window->raise();
        m_export_window->showNormal();
        m_export_window->setDefaultName(m_part_widget->getFirstPartName());
    });
    connect(m_actions["project_new"].action, &QAction::triggered, this, &MainWindow::createProject);
    connect(m_actions["project_save"].action, &QAction::triggered, this, &MainWindow::saveSession);
    connect(m_actions["project_load"].action, &QAction::triggered, this, &MainWindow::loadSession);

    connect(m_actions["flowrate_calc"].action, &QAction::triggered, m_flowrate_calc_window, [this] {
        m_flowrate_calc_window->raise();
        m_flowrate_calc_window->showNormal();
    });
    connect(m_actions["xtrude_calc"].action, &QAction::triggered, m_xtrude_calc_window, [this] {
        m_xtrude_calc_window->raise();
        m_xtrude_calc_window->showNormal();
    });
    connect(m_actions["debug"].action, &QAction::triggered, this, &MainWindow::debug);
    connect(m_actions["cs_view"].action, &QAction::triggered, []() {
        CsDebugDialog dialog;
        dialog.exec();
    });

    // Connect slice button
    connect(m_part_widget, &PartWidget::slice, this, &MainWindow::doSlice);
    // connect(m_part_widget->m_slice_btn, &QToolButton::clicked, this, &MainWindow::doSlice);

    // Session connections.
    connect(CSM.get(), &SessionManager::partAdded, m_part_widget, &PartWidget::add);
    connect(CSM.get(), &SessionManager::forwardSliceComplete, this, &MainWindow::importFile);
    connect(CSM.get(), &SessionManager::forwardSliceComplete, this, &MainWindow::enableExportMenu);
    connect(CSM.get(), &SessionManager::forwardSliceComplete, this, [this]() {
        m_main_toolbar->setView(1);
        switchViews(1);
    });
    connect(CSM.get(), &SessionManager::forwardStatusUpdate, this, &MainWindow::updateStatus);

    // Hook up updated layer min/max from gcode bar to gcodewidget
    connect(m_gcodebar, &GcodeBar::lowerLayerUpdated, m_gcode_widget->view(), &GCodeView::setLowLayer);
    connect(m_gcodebar, &GcodeBar::upperLayerUpdated, m_gcode_widget->view(), &GCodeView::setHighLayer);
    connect(m_gcodebar, &GcodeBar::lowerSegmentUpdated, m_gcode_widget->view(), &GCodeView::setLowSegment);
    connect(m_gcodebar, &GcodeBar::upperSegmentUpdated, m_gcode_widget->view(), &GCodeView::setHighSegment);
    connect(m_gcodebar, &GcodeBar::lineChanged, m_gcode_widget->view(), &GCodeView::updateSegments);
    connect(m_gcodebar, &GcodeBar::refreshGCode, this, &MainWindow::importGCodeHelper);
    connect(m_gcodebar, &GcodeBar::forwardVisibilityChange, m_gcode_widget->view(), &GCodeView::hideSegmentType);
    connect(m_gcodebar, &GcodeBar::forwardSegmentWidthChange, m_gcode_widget->view(), &GCodeView::updateSegmentWidths);

    // Update segment slider when layer is changed
    connect(m_gcode_widget->view(), &GCodeView::maxSegmentChanged, m_gcodebar, &GcodeBar::setMaxSegment);

    // Gcode widget to bar.
    connect(m_gcode_widget->view(), &GCodeView::updateSelectedSegments, this,
            [this](QList<int> linesToAdd, QList<int> linesToRemove) {
                m_gcodebar->setLineNumber(linesToAdd, linesToRemove, false);
            });

    // Connect layerbar to settingbar
    connect(m_layerbar, &LayerBar::setSelectedSettings, m_settingbar, &SettingBar::settingsBasesSelected);

    // Part widget connection
    connect(m_part_widget, &PartWidget::selected, this,
            [this](QSet<QSharedPointer<Part>> pl) { this->enableSelectionMenu(!pl.empty()); });
    connect(m_part_widget, &PartWidget::added, this, [this](QSharedPointer<Part> p) {
        m_object_count++;

        if (m_object_count > 0) {
            m_actions["slice"].action->setEnabled(true);
            m_main_toolbar->setSliceAbility(true);
        }
        else {
            m_actions["slice"].action->setEnabled(false);
            m_main_toolbar->setSliceAbility(false);
        }
    });
    connect(m_part_widget, &PartWidget::removed, this, [this](QSharedPointer<Part> p) {
        m_object_count--;

        if (m_object_count > 0) {
            m_actions["slice"].action->setEnabled(true);
            m_main_toolbar->setSliceAbility(true);
        }
        else {
            m_actions["slice"].action->setEnabled(false);
            m_main_toolbar->setSliceAbility(false);
        }
    });
    connect(m_part_widget, &PartWidget::displayRotationInfoMsg, this, [this]() {
        m_cmdbar->append("\r\nScaling applied permenantly to current transformation\r\n"
                         "Scaling factors are reset to 100%\r\n");
    });

    // Connect to timer.
    connect(m_timer, &QTimer::timeout, this, &MainWindow::autoSave);

    // Allow stuff to changed at runtime based upon user-modified settings
    connect(m_settingbar, &SettingBar::settingModified, m_part_widget, &PartWidget::handleModifiedSetting);
    connect(m_settingbar, &SettingBar::settingModified, m_gcode_widget, &GCodeWidget::handleModifiedSetting);
    connect(m_settingbar, &SettingBar::settingModified, m_layerbar, &LayerBar::handleModifiedSetting);
    connect(m_settingbar, &SettingBar::settingModified, m_main_toolbar, &MainToolbar::handleModifiedSetting);
    connect(m_settingbar, &SettingBar::settingModified, this, &MainWindow::handleModifiedSetting);
    connect(m_settingbar, &SettingBar::tabHidden, this, &MainWindow::addHiddenSetting);
    connect(GSM.get(), &SettingsManager::globalLoaded, this, &MainWindow::updateSettings);

    connect(m_external_file_window, &ExternalFileWindow::forwardGridInfo, CSM.get(), &SessionManager::setExternalInfo);

    // Toolbar -> MainWindow
    connect(m_main_toolbar, &MainToolbar::viewChanged, this, &MainWindow::switchViews);
    connect(m_main_toolbar, &MainToolbar::loadModel, this, &MainWindow::loadModel);

    // Toolbar -> Part Widget
    connect(m_main_toolbar, &MainToolbar::slice, m_part_widget, &PartWidget::preSliceUpdate);
    connect(m_main_toolbar, &MainToolbar::showSlicingPlanes, m_part_widget, &PartWidget::showSlicingPlanes);
    connect(m_main_toolbar, &MainToolbar::showLabels, m_part_widget, &PartWidget::showLabels);
    connect(m_main_toolbar, &MainToolbar::showSeams, m_part_widget, &PartWidget::showSeams);
    connect(m_main_toolbar, &MainToolbar::showOverhang, m_part_widget, &PartWidget::showOverhang);

    // Toolbar -> GCode Widget
    connect(m_main_toolbar, &MainToolbar::showSegmentInfo, m_gcode_widget, &GCodeWidget::showSegmentInfo);
    connect(m_main_toolbar, &MainToolbar::setOrthoGcode, m_gcode_widget, &GCodeWidget::setOrthoView);
    connect(m_main_toolbar, &MainToolbar::showGhosts, m_gcode_widget, &GCodeWidget::showGhosts);
    connect(m_main_toolbar, &MainToolbar::exportGCode, m_export_window, [this] {
        m_export_window->raise();
        m_export_window->showNormal();
        m_export_window->setDefaultName(m_part_widget->getFirstPartName());
    });

    connect(CSM.get(), &SessionManager::requestTransformationUpdate, m_part_widget,
            &PartWidget::updatePartTransformations);

    // Init to part view
    switchViews(0);

    // Connect theme signals
    connect(m_pref_window, &PreferencesWindow::updateTheme, this, &MainWindow::setStyleFromFile);
    connect(m_pref_window, &PreferencesWindow::updateTheme, m_part_widget, &PartWidget::setupStyle);
    connect(m_pref_window, &PreferencesWindow::updateTheme, m_gcode_widget, &GCodeWidget::setupStyle);
    connect(m_pref_window, &PreferencesWindow::updateTheme, m_main_toolbar, &MainToolbar::setupStyle);
    connect(m_pref_window, &PreferencesWindow::updateTheme, m_settingbar, &SettingBar::setupStyle);
    connect(m_tab_widget, &QTabWidget::currentChanged, m_part_widget, &PartWidget::setupStyle);
    connect(m_tab_widget, &QTabWidget::currentChanged, m_gcode_widget, &GCodeWidget::setupStyle);
}

void MainWindow::switchViews(int index) {
    m_tab_widget->setCurrentIndex(index);
    if (index) {
        connect(m_gcode_widget, &GCodeWidget::resized, m_main_toolbar, &MainToolbar::resize);
        connect(m_actions["reset_camera"].action, &QAction::triggered, m_gcode_widget, &GCodeWidget::resetCamera);
        connect(m_actions["zoom_in"].action, &QAction::triggered, m_gcode_widget, &GCodeWidget::zoomIn);
        connect(m_actions["zoom_out"].action, &QAction::triggered, m_gcode_widget, &GCodeWidget::zoomOut);
        connect(m_actions["zoom_reset"].action, &QAction::triggered, m_gcode_widget, &GCodeWidget::resetZoom);

        QObject::disconnect(m_part_widget, &PartWidget::resized, m_main_toolbar, &MainToolbar::resize);
        QObject::disconnect(m_actions["reset_camera"].action, &QAction::triggered, m_part_widget,
                            &PartWidget::resetCamera);
        QObject::disconnect(m_actions["zoom_in"].action, &QAction::triggered, m_part_widget, &PartWidget::zoomIn);
        QObject::disconnect(m_actions["zoom_out"].action, &QAction::triggered, m_part_widget, &PartWidget::zoomOut);
        QObject::disconnect(m_actions["zoom_reset"].action, &QAction::triggered, m_part_widget, &PartWidget::resetZoom);
    }
    else {
        connect(m_part_widget, &PartWidget::resized, m_main_toolbar, &MainToolbar::resize);
        connect(m_actions["reset_camera"].action, &QAction::triggered, m_part_widget, &PartWidget::resetCamera);
        connect(m_actions["zoom_in"].action, &QAction::triggered, m_part_widget, &PartWidget::zoomIn);
        connect(m_actions["zoom_out"].action, &QAction::triggered, m_part_widget, &PartWidget::zoomOut);
        connect(m_actions["zoom_reset"].action, &QAction::triggered, m_part_widget, &PartWidget::resetZoom);

        QObject::disconnect(m_gcode_widget, &GCodeWidget::resized, m_main_toolbar, &MainToolbar::resize);
        QObject::disconnect(m_actions["reset_camera"].action, &QAction::triggered, m_gcode_widget,
                            &GCodeWidget::resetCamera);
        QObject::disconnect(m_actions["zoom_in"].action, &QAction::triggered, m_gcode_widget, &GCodeWidget::zoomIn);
        QObject::disconnect(m_actions["zoom_out"].action, &QAction::triggered, m_gcode_widget, &GCodeWidget::zoomOut);
        QObject::disconnect(m_actions["zoom_reset"].action, &QAction::triggered, m_gcode_widget,
                            &GCodeWidget::resetZoom);
    }
}

void MainWindow::handleModifiedSetting(const QString key) {}

QList<QAction*> MainWindow::setupSettingActions(QMenu* submenu, QString panel) {
    QList<QAction*> actions;

    for (QString setting : PreferencesManager::getInstance()->getHiddenSettings(panel)) {
        menu_info info = {setting, "", false, QKeySequence(), nullptr};
        createAction(info);
        m_actions.insert(setting, info);

        info.action->setText(QApplication::translate("MainWindow", info.name.toStdString().c_str(), nullptr));
        connect(info.action, &QAction::triggered, this,
                [this, submenu, panel, setting] { this->removeHiddenSetting(submenu, panel, setting); });

        actions.push_back(info.action);
    }

    return actions;
}

void MainWindow::addHiddenSetting(QString panel, QString setting) {
    menu_info info = {setting, "", false, QKeySequence(), nullptr};
    createAction(info);
    m_actions.insert(setting, info);

    QMenu* submenu;
    if (panel == "Printer")
        submenu = m_menu_hidden_printer_settings;
    else if (panel == "Material")
        submenu = m_menu_hidden_material_settings;
    else if (panel == "Profile")
        submenu = m_menu_hidden_profile_settings;
    else
        submenu = m_menu_hidden_experimental_settings;

    submenu->addAction(info.action);
    info.action->setText(QApplication::translate("MainWindow", info.name.toStdString().c_str(), nullptr));
    connect(info.action, &QAction::triggered, this,
            [this, submenu, panel, setting] { this->removeHiddenSetting(submenu, panel, setting); });

    PreferencesManager::getInstance()->addHiddenSetting(panel, setting);
}

void MainWindow::removeHiddenSetting(QMenu* menu, QString panel, QString setting) {
    menu->removeAction(m_actions[setting].action);
    delete m_actions[setting].action;
    m_actions.remove(setting);
    m_settingbar->showHiddenSetting(panel, setting);
    PreferencesManager::getInstance()->removeHiddenSetting(panel, setting);
}

void MainWindow::retranslateUi() {
    this->setWindowTitle(QApplication::translate("MainWindow", "ORNL Slicer-2", nullptr));

    // Iterate through the actions and retranslate them.
    for (menu_info curr_act : m_actions) {
        curr_act.action->setText(QApplication::translate("MainWindow", curr_act.name.toStdString().c_str(), nullptr));
    }

    m_menu_file->setTitle(QApplication::translate("MainWindow", "File", nullptr));
    m_menu_edit->setTitle(QApplication::translate("MainWindow", "Edit", nullptr));
    m_menu_zoom->setTitle(QApplication::translate("MainWindow", "Zoom", nullptr));
    m_menu_toolbars->setTitle(QApplication::translate("MainWindow", "Toolbars", nullptr));
    m_menu_hidden_settings->setTitle(QApplication::translate("MainWindow", "Hidden Settings", nullptr));
    m_menu_hidden_printer_settings->setTitle(QApplication::translate("MainWindow", "Printer Settings", nullptr));
    m_menu_hidden_material_settings->setTitle(QApplication::translate("MainWindow", "Material Settings", nullptr));
    m_menu_hidden_profile_settings->setTitle(QApplication::translate("MainWindow", "Profile Settings", nullptr));
    m_menu_hidden_experimental_settings->setTitle(
        QApplication::translate("MainWindow", "Experimental Settings", nullptr));
    m_menu_view->setTitle(QApplication::translate("MainWindow", "View", nullptr));
    m_menu_project->setTitle(QApplication::translate("MainWindow", "Project", nullptr));
    m_menu_tools->setTitle(QApplication::translate("MainWindow", "Tools", nullptr));
    m_menu_settings->setTitle(QApplication::translate("MainWindow", "Settings", nullptr));
    m_menu_scripts->setTitle(QApplication::translate("MainWindow", "Scripts", nullptr));
    m_menu_help->setTitle(QApplication::translate("MainWindow", "Help", nullptr));
    m_menu_debug->setTitle(QApplication::translate("MainWindow", "Debug", nullptr));

    m_main_toolbar->setWindowTitle(QApplication::translate("MainWindow", "Main Control", nullptr));
    m_settingdock->setWindowTitle(QApplication::translate("MainWindow", "Settings", nullptr));
    m_gcodedock->setWindowTitle(QApplication::translate("MainWindow", "G-Code Editor", nullptr));
    m_layertimesdock->setWindowTitle(QApplication::translate("MainWindow", "Layer Times", nullptr));
    m_external_file_dock->setWindowTitle(QApplication::translate("MainWindow", "External File Input", nullptr));
    m_cmddock->setWindowTitle(QApplication::translate("MainWindow", "Status", nullptr));
}

void MainWindow::setBusy(bool busy, QString msg) {
    this->setLock(busy);
    m_progressbar->setHidden(!busy);
    m_statusbar->showMessage(msg);

    m_cmdbar->append(msg);
    m_status = busy;
}

void MainWindow::doSlice() {
    m_gcode_widget->clear();
    m_gcodebar->clear();
    m_layertimebar->clear();

    if (!CSM->isBuildMode())
        return;

    m_slice_dialog.reset(new SliceDialog(this));
    connect(CSM.get(), &SessionManager::updateDialog, m_slice_dialog.get(),
            QOverload<StatusUpdateStepType, int>::of(&SliceDialog::updateStatus));
    connect(m_slice_dialog.get(), &SliceDialog::cancelSlice, CSM.get(), &SessionManager::cancelSlice);

    m_slice_dialog->show();

    // Execute.
    CSM->doSlice();

    m_statusbar->showMessage("Slicing the loaded part(s) ...");
    m_cmdbar->append("Slicing the loaded part(s) ...");
    // m_progressbar->show();
}

void MainWindow::loadModel(MeshType mt) {
    QStringList filepaths;
    if (mt == MeshType::kClipping) {
        filepaths = QFileDialog::getOpenFileNames(nullptr, QObject::tr("Open STL clipping file"),
                                                  CSM->getMostRecentModelLocation(),
                                                  QObject::tr("Model File (*.stl *.3mf *.obj *.amf)"));
    }
    else if (mt == MeshType::kSettings) {
        filepaths = QFileDialog::getOpenFileNames(nullptr, QObject::tr("Open STL settings file"),
                                                  CSM->getMostRecentModelLocation(),
                                                  QObject::tr("Model File (*.stl *.3mf *.obj *.amf)"));
    }
    else {
        filepaths =
            QFileDialog::getOpenFileNames(nullptr, QObject::tr("Open STL part file"), CSM->getMostRecentModelLocation(),
                                          QObject::tr("Model File (*.stl *.3mf *.obj *.amf)"));
    }

    if (filepaths.isEmpty())
        return;

    QString statusMsg = "Loading model(s): " + filepaths.join("\n");
    m_cmdbar->append(statusMsg);

    // For each file in the selection, load it.
    for (QString file : filepaths) {
        CSM->loadModel(file, true, mt);
    }
}

void MainWindow::loadPointCloud() {
    // Load a point cloud from file
    QStringList filepaths =
        QFileDialog::getOpenFileNames(nullptr, QObject::tr("Open STL part file"), CSM->getMostRecentModelLocation(),
                                      QObject::tr("Point Cloud (*.matrix *.xyz)"));
    if (filepaths.isEmpty())
        return;

    QString statusMsg = "Loading model(s): " + filepaths.join("\n");
    m_cmdbar->append(statusMsg);

    // Add parts/ load into graphics
    for (QString file : filepaths) {
        auto new_mesh = OpenMesh::BuildMeshFromPointCloud(file);
        if (new_mesh != nullptr)
            CSM->addPart(new_mesh);
    }
}

void MainWindow::importGCode() {
    QString filepath = QFileDialog::getOpenFileName(
        this, tr("Open Gcode file"), CSM->getMostRecentGcodeLocation(),
        tr("All Gcode Files (*.nc *.gcode *.mpf *.eia *.txt *.cli);;Gcode File (*.nc);;Gcode File (*.gcode);;Gcode "
           "File (*.mpf);;Gcode File (*.eia);;Gcode File (*.txt);;Gcode File (*.cli)"));

    if (!filepath.isEmpty()) {
        importGCodeHelper(filepath, false);
        CSM->setMostRecentGcodeLocation(QFileInfo(filepath).absolutePath());
        m_main_toolbar->setView(1);
        switchViews(1);
    }
}

void MainWindow::importGCodeHelper(QString filepath, bool alterFile) {
    m_gcode_widget->clear();
    disconnect(m_slice_dialog.get(), &SliceDialog::cancelSlice, CSM.get(), &SessionManager::cancelSlice);

    GCodeLoader* loader = new GCodeLoader(filepath, alterFile);
    connect(loader, &GCodeLoader::finished, loader, &GCodeLoader::deleteLater);
    connect(loader, &GCodeLoader::gcodeLoadedVisualization, m_gcode_widget, &GCodeWidget::addGCode);
    connect(loader, &GCodeLoader::gcodeLoadedText, m_gcodebar, &GcodeBar::updateGcodeText);

    if (!m_slice_dialog.isNull()) {
        connect(loader, &GCodeLoader::finished, this, [this]() { m_slice_dialog->close(); });
        connect(loader, &GCodeLoader::updateDialog, m_slice_dialog.get(), &SliceDialog::updateStatus);
        connect(m_slice_dialog.get(), &SliceDialog::cancelSlice, loader, &GCodeLoader::cancelSlice);
    }

    connect(loader, &GCodeLoader::finished, this, &MainWindow::enableExportBuildLogMenu);
    connect(loader, &GCodeLoader::forwardInfoToMainWindow, this, &MainWindow::showGCodeKeyInfo);

    connect(loader, &GCodeLoader::forwardInfoToLayerTimeWindow, m_layertimebar,
            &LayerTimesWindow::updateTimeInformation);
    connect(loader, &GCodeLoader::forwardInfoToBuildExportWindow, m_export_window,
            &GcodeExport::updateOutputInformation);

    connect(loader, &GCodeLoader::gcodeLoadedVisualization, this,
            [this](QVector<QVector<QSharedPointer<SegmentBase>>> segments) {
                if (segments.empty()) {
                    return;
                }

                m_gcodebar->setMaxLayer(qMax(segments.size() - 1, 0));
                if (segments.size() > 1)
                    m_gcodebar->setMaxSegment(qMax(segments[0].size() + segments[1].size() - 1, 0));
                else
                    m_gcodebar->setMaxSegment(qMax(segments[0].size() - 1, 0));
            });

    connect(loader, &GCodeLoader::error, this, &MainWindow::updateStatus);
    loader->start();

    m_gcodedock->raise();

    enableExportMenu();
}

void MainWindow::importFile(QString filepath, bool alterFile) { importGCodeHelper(filepath, alterFile); }

void MainWindow::updateStatus(QString status) {
    m_cmdbar->append(status);
    m_statusbar->showMessage(status);
}

void MainWindow::saveSession() {
    QFileDialog save_dialog;
    save_dialog.setWindowTitle("Save project");
    save_dialog.setDirectory(CSM->getMostRecentProjectLocation());
    save_dialog.setAcceptMode(QFileDialog::AcceptSave);
    save_dialog.setNameFilters(QStringList() << "Slicer-2 Project File (*.s2p)" << "Any Files (*)");
    save_dialog.setDefaultSuffix("s2p");
    if (!save_dialog.exec())
        return;

    QString filename = save_dialog.selectedFiles().first();
    if (filename.isEmpty())
        return;

    CSM->saveSession(filename);
    CSM->saveSession(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/_lastsession.s2p", false);

    this->setTitleInfo(filename.split("/").back());
    m_statusbar->showMessage("Session saved");
    m_cmdbar->append("Session saved: " + filename);
}

void MainWindow::loadSession() {
    QFileDialog load_dialog;
    load_dialog.setWindowTitle("Load project");
    load_dialog.setDirectory(CSM->getMostRecentProjectLocation());
    load_dialog.setAcceptMode(QFileDialog::AcceptOpen);
    load_dialog.setNameFilters(QStringList() << "Slicer-2 Project File (*.s2p)" << "Any Files (*)");
    load_dialog.setDefaultSuffix("s2p");
    if (!load_dialog.exec())
        return;

    QString filename = load_dialog.selectedFiles().first();
    if (filename.isEmpty())
        return;

    loadASession(filename);
    m_statusbar->showMessage("Session loaded");
    m_cmdbar->append("Session loaded: " + filename);

    CSM->setMostRecentProjectLocation(QFileInfo(filename).absolutePath());
}

void MainWindow::loadASession(const QString& filename) {
    m_part_widget->clear();
    CSM->loadSession(true, filename);
}

void MainWindow::updateSettings(const QString& name) {
    QStringList tabs {Constants::Settings::SettingTab::kPrinter, Constants::Settings::SettingTab::kMaterial,
                      Constants::Settings::SettingTab::kProfile, Constants::Settings::SettingTab::kExperimental};
    m_settingbar->displayNewSetting(tabs, name);
}

void MainWindow::showAllSettings(QString key, QMenu* menu) {
    QList<QString> settings = PreferencesManager::getInstance()->getHiddenSettings(key);
    for (QString setting : settings) {
        this->removeHiddenSetting(menu, key, setting);
    }
}

void MainWindow::autoSave() {
    if (m_status)
        return;

    if (CSM->parts().count() == 0) {
        qDebug() << "Not auto saving session - no part exists";
        return;
    }

    QString homePathStr = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir homePath(homePathStr);
    try {
        if (!homePath.exists())
            QDir().mkpath(homePathStr);
    } catch (...) { qWarning() << "Check your path, cannot create directory:" + homePathStr; }
    CSM->saveSession(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/_lastsession.s2p", false);
}

void MainWindow::autoLoad() {

    QString filename = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/_lastsession.s2p";
    QFileInfo file(filename);
    if (!file.exists() || !file.isFile()) {
        qDebug() << "No last session! Cannot load.";
        return;
    }

    loadASession(filename);
    m_cmdbar->append("Last session restored: " + filename);
    m_statusbar->showMessage("Last session restored" + filename);
}

// New Project: delete all existing loaded parts, start from scratch
//   it does not replace the existing settings though
void MainWindow::createProject() { m_part_widget->clear(); }

void MainWindow::saveTemplate() {
    TemplateSaveDialog dialog;
    if (!dialog.exec())
        return;

    GSM->saveTemplate(dialog.keys(), dialog.filename(), dialog.name());
    GSM->loadGlobalJson(dialog.filename());
    QFileInfo fileInfo(dialog.filename());
    updateSettings(fileInfo.baseName());
}

void MainWindow::loadTemplate() {
    QFileDialog load_dialog;
    load_dialog.setWindowTitle("Load Template");
    load_dialog.setAcceptMode(QFileDialog::AcceptOpen);
    load_dialog.setNameFilters(QStringList() << "Slicer-2 Configuration/Template File (*.s2c)"
                                             << "Any Files (*)");
    load_dialog.setDefaultSuffix("s2c");
    if (!load_dialog.exec())
        return;

    QString filename = load_dialog.selectedFiles().first();
    if (filename.isEmpty())
        return;

    GSM->loadGlobalJson(filename);

    QFileInfo fileInfo(filename);
    QString actualFilename = fileInfo.completeBaseName();
    QStringList tabs {Constants::Settings::SettingTab::kPrinter, Constants::Settings::SettingTab::kMaterial,
                      Constants::Settings::SettingTab::kProfile, Constants::Settings::SettingTab::kExperimental};

    for (QString tab : tabs)
        GSM->constructActiveGlobal(tab, actualFilename);

    QMap<QString, QMap<QString, QSharedPointer<SettingsBase>>> allGlobalCopy = GSM->getAllGlobals();
    for (int i = tabs.size() - 1; i >= 0; --i) {
        if (!allGlobalCopy[tabs[i]].contains(actualFilename))
            tabs.removeAt(i);
    }
    m_settingbar->displayNewSetting(tabs, actualFilename);
}

void MainWindow::setSettingFolder() {
    QFileDialog load_dialog;
    load_dialog.setFileMode(QFileDialog::Directory);
    load_dialog.setWindowTitle("Select Directory");
    load_dialog.setAcceptMode(QFileDialog::AcceptOpen);

    if (!load_dialog.exec())
        return;

    QString path = load_dialog.selectedFiles().first() + "/";
    if (path.isEmpty())
        return;

    CSM->setMostRecentSettingFolderLocation(path);
    GSM->loadAllGlobals(path);
    m_settingbar->reloadDisplayedList();
    m_settingbar->setCurrentFolder(path);
}

void MainWindow::setLayerBarSettingFolder() {
    QFileDialog load_dialog;
    load_dialog.setFileMode(QFileDialog::Directory);
    load_dialog.setWindowTitle("Select Directory");
    load_dialog.setAcceptMode(QFileDialog::AcceptOpen);

    if (!load_dialog.exec())
        return;

    QString path = load_dialog.selectedFiles().first() + "/";
    if (path.isEmpty())
        return;

    CSM->setMostRecentLayerBarSettingFolderLocation(path);
    GSM->loadLayerBarTemplate(path);
}

void MainWindow::setLock(bool lock) {
    m_actions["sel_printer"].action->setDisabled(lock);
    m_actions["load_model"].action->setDisabled(lock);
    m_actions["load_point_cloud"].action->setDisabled(lock);
    m_actions["last_session"].action->setDisabled(lock);
    m_actions["slice"].action->setDisabled(lock);

    m_menu_edit->setDisabled(lock);
    m_menu_settings->setDisabled(lock);
    m_menu_project->setDisabled(lock);
    m_menu_tools->setDisabled(lock);

    m_layerbar->setDisabled(lock);
    m_settingbar->setLock(lock);
    m_gcodebar->setDisabled(lock);
}

void MainWindow::setTitleInfo(const QString& str) { this->setWindowTitle("ORNL Slicer-2 - " + str); }

void MainWindow::enableSelectionMenu(bool partSelected) {
    m_actions["reload"].action->setEnabled(partSelected);
    m_actions["delete"].action->setEnabled(partSelected);
    m_actions["copy"].action->setEnabled(partSelected);
}

void MainWindow::enableSliceMenu(bool partExists) { m_actions["slice"].action->setEnabled(partExists); }

void MainWindow::enableExportBuildLogMenu() { m_actions["build_log"].action->setEnabled(true); }

void MainWindow::enableExportMenu() {
    m_actions["gcode_export"].action->setEnabled(true);
    m_main_toolbar->setExportAbility(true);
}

void MainWindow::showGCodeKeyInfo(QString msg) {
    m_statusbar->showMessage("GCode file loaded", 5000);
    m_progressbar->hide();

    m_cmdbar->append(msg);
}
} // namespace ORNL
