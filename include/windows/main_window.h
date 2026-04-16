#pragma once

#include "QApplication"
#include "QDesktopServices"
#include "QDockWidget"
#include "QFile"
#include "QLabel"
#include "QMainWindow"
#include "QMenuBar"
#include "QTextEdit"
#include "QToolBar"
#include "QUdpSocket"
#include "dialogs/slice_dialog.h"
#include "widgets/cmd_widget.h"
#include "widgets/gcode_widget.h"
#include "widgets/gcodebar.h"
#include "widgets/layerbar.h"
#include "widgets/main_toolbar.h"
#include "widgets/part_widget/part_widget.h"
#include "widgets/settings/setting_bar.h"
#include "windows/about.h"
#include "windows/flowratecalc.h"
#include "windows/gcode_export.h"
#include "windows/layer_times_window.h"
#include "windows/preferences_window.h"
#include "windows/xtrudecalc.h"

namespace ORNL {

class PolymerSlicer;
class SessionLoader;

//! \brief Define for quick access to this singleton.
#define MWIN MainWindow::getInstance()

/*!
 * \class MainWindow
 * \brief The main window which contains all UI element.
 *
 * \todo The bones of this class are fine, but some cleanup should probably occur.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

  public:
    //! \brief Get the singleton instance of the MainWindow.
    static MainWindow* getInstance();

    //! \brief Destructor
    ~MainWindow();

    //! \brief Get the instance of the cmd widget to access data.
    CmdWidget* getCmdOut();

  signals:
    //! \brief Signal to indicate check of time is complete (if registry check is insufficient)
    void nistDone();

  public slots:
    //! \brief Override show event to manipulate bounds on cmd_window
    void showEvent(QShowEvent* event);

    //! \brief Generic debug function. Place all testing code here!
    void debug();

    //! \brief Set the current style from a qss file or style string.
    //! \param file: String or stylesheet
    bool setStyleFromFile();

    //! \brief Upon update, retranslate the ui.
    void retranslateUi();

    //! \brief Set the status of the main window.
    void setBusy(bool busy = true, QString msg = "Wait...");

    //! \brief Enable GCode Export menu
    void enableExportMenu();

    //! \brief Enable GCode Export Build Log menu
    void enableExportBuildLogMenu();

    //! \brief Receive information for display once GCode parsing is done
    //! \param msg: Info to display
    void showGCodeKeyInfo(QString msg);

    //! \brief Helper to differentiate between gcode load from menu vs from slice
    //! \brief filepath: Path to gcode file
    //! \brief alterFile: Whether or not the file can be altered by min layer time settings
    void importGCodeHelper(QString filepath, bool alterFile);

    //! \brief Helper to differentiate between gcode load from menu vs from slice
    //! \brief filepath: Path to gcode file
    //! \brief alterFile: Whether or not the file can be altered by min layer time settings
    void importFile(QString filepath, bool alterFile);

    //! \brief Updates status during slice
    //! \param status: Status message
    void updateStatus(QString status);

    //! \brief Load up a part.
    //! \param mt: Mesh type (default is build)
    void loadModel(MeshType mt = MeshType::kBuild);

    //! \brief loads a point cloud into a surface mesh object
    void loadPointCloud();

  private slots:
    //! \brief Start the slicing.
    void doSlice();

    //! \brief Import GCode
    void importGCode();

    //! \brief Save the current session.
    void saveSession();

    //! \brief Create a new project
    void createProject();

    //! \brief Load a session from a file.
    void loadSession();
    SessionLoader* loadASession(const QString& fileName);

    //! \brief Auto save the current session.
    void autoSave();

    //! \brief Load the last session.
    void autoLoad();

    //! \brief Save a template.
    void saveTemplate();

    //! \brief Load a template.
    void loadTemplate();

    //! \brief Set setting folder to load from.
    void setSettingFolder();

    //! \brief Set layer bar setting folder to load from.
    void setLayerBarSettingFolder();

    //! \brief Lock the window's inputs.
    //! \param lock: Whether or not to lock
    void setLock(bool lock);

    //! \brief Set the window's title info.
    //! \param str: Title string to set
    void setTitleInfo(const QString& str);

    //! \brief Enable or disable Cut/Copy/Delete menus
    //! \param partSelected: Whether or not a part is selected
    void enableSelectionMenu(bool partSelected);

    //! \brief Enable or disable Slice menu
    //! \param partExists: Whether or not a part exists
    void enableSliceMenu(bool partExists);

    //! \brief Update setting file selected in setting bar
    //! \param name: Name of setting file
    void updateSettings(const QString& name);

    //! \brief Show all settings for a given panel
    //! \param key: Panel name
    //! \param menu: Menu object to remove entries from
    void showAllSettings(QString key, QMenu* menu);

    //! \brief switches views and setups signals for resize events
    //! \param index the view index to swtich to
    void switchViews(int index);

    //! \brief handles settings changes
    //! \param key: the settings key
    void handleModifiedSetting(const QString key);

  private:
    //! \brief Struct to retain action information efficiently.
    struct menu_info {
        //! \brief Display name.
        QString name;
        //! \brief Icon.
        QString icon;
        //!  \brief Checkable option.
        bool checkable;
        //! \brief Keyboard shortcut.
        QKeySequence shortcut;
        //! \brief Action to execute.
        QAction* action;
    };

    //! \brief Constructor
    explicit MainWindow(QWidget* parent = nullptr);

    //! \brief Setup the underlying classes used by the project.
    void setupClasses();

    //! \brief Teardown the underlying classes.
    void teardownClasses();

    //! \brief Setup the static widgets and their layouts.
    void setupUi();

    //! \brief Creates action for menus from menu_info object
    //! \param info: Object holding all necessary info for a menu action
    void createAction(menu_info& info);

    //! \brief Sets up actions for setting menu items
    //! \param submenu: Menu pointer for appropriate submenu
    //! \param panel: Panel name to notify settingsbar
    //! \return List of actions to insert into top-level menu
    QList<QAction*> setupSettingActions(QMenu* submenu, QString panel);

    //! \brief Adds a hidden setting to the setting menu and notifies Preferences Manager
    //! \param panel: Panel the setting is contained on
    //! \param setting: Actual setting category header
    void addHiddenSetting(QString panel, QString setting);

    //! \brief Removes a hidden setting from the setting menu and notifies Preferences Manager
    //! \param menu: Submenu to modify
    //! \param panel: Panel the setting is contained on
    //! \param setting: Actual setting category header
    void removeHiddenSetting(QMenu* menu, QString panel, QString setting);

    // -- 8 Step initalization process --

    //! \brief 1. Setup the window's properties.
    void setupWindows();
    //! \brief 2. Setup the widgets.
    void setupWidgets();
    //! \brief 3. Setup the dockwidgets' properties.
    void setupDocks();
    //! \brief 4. Setup the layouts and insert their children.
    void setupLayouts();
    //! \brief 5. Setup the various bar in the UI.
    void setupBars();
    //! \brief 6. Setup the actions for the window.
    void setupActions();
    //! \brief 7. Setup the insertions for all UI elements.
    void setupInsert();
    //! \brief 8. Setup the events for the various widgets.
    void setupEvents();

    //! \brief Performs initial startup once constructor has verified time
    void continueStartup();

    //! \brief Current window status.
    bool m_status;

    //! \brief Temporary stuff here
    uint m_object_count = 0;

    //! \brief Loaded templates.
    QStringList m_templates;

    //! \brief Autosave timer.
    QTimer* m_timer;

    //! \brief Singleton pointer. This must be a raw pointer to avoid double free on close.
    static MainWindow* m_singleton;

    //! \brief File that contains current style.
    QSharedPointer<QFile> m_style_file;

    //! \brief Tool Windows
    PreferencesWindow* m_pref_window;
    GcodeExport* m_export_window;
    FlowrateCalcWindow* m_flowrate_calc_window;
    XtrudeCalcWindow* m_xtrude_calc_window;
    AboutWindow* m_about_window;

    //! \brief QActions
    QMap<QString, menu_info> m_actions;

    //! \brief QMenus
    QMenu* m_menu_file;
    QMenu* m_menu_edit;
    QMenu* m_menu_view;
    QMenu* m_menu_zoom;
    QMenu* m_menu_toolbars;
    QMenu* m_menu_hidden_settings;
    QMenu* m_menu_hidden_printer_settings;
    QMenu* m_menu_hidden_material_settings;
    QMenu* m_menu_hidden_profile_settings;
    QMenu* m_menu_hidden_experimental_settings;
    QMenu* m_menu_settings;
    QMenu* m_menu_project;
    QMenu* m_menu_tools;
    QMenu* m_menu_scripts;
    QMenu* m_menu_help;
    QMenu* m_menu_debug;

    //! \brief Layouts
    QGridLayout* m_main_layout;
    QHBoxLayout* m_main_container_layout;

    //! \brief Bars and Docks
    QMenuBar* m_menubar;
    QStatusBar* m_statusbar;
    MainToolbar* m_main_toolbar;
    QDockWidget* m_cmddock;
    QDockWidget* m_settingdock;
    QDockWidget* m_gcodedock;
    QDockWidget* m_layertimesdock;

    QTabWidget* m_tab_widget;

    //! \brief Custom Widgets
    LayerTimesWindow* m_layertimebar;
    PartWidget* m_part_widget;
    GCodeWidget* m_gcode_widget;
    LayerBar* m_layerbar;
    SettingBar* m_settingbar;
    GcodeBar* m_gcodebar;
    CmdWidget* m_cmdbar;
    QScopedPointer<SliceDialog> m_slice_dialog;

    //! \brief All other widgets
    QWidget* m_main_widget;
    QWidget* m_main_container;
    QProgressBar* m_progressbar;

    //! \brief Setting panel to menu map
    QHash<QString, QMenu*> m_setting_panel_to_menu_map;

  protected:
    void closeEvent(QCloseEvent* event);
};
} // namespace ORNL
