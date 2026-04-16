#pragma once

#include <QComboBox>
#include <QIcon>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qlist.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "configs/settings_base.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_pane.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {
class SettingTab;

/*!
 * \class SettingBar
 * \brief Widget that displays all available settings.
 *
 * The relationship between the the various settings widgets is as follows:
 * SettingsBar - Main container for all widgets. Has a tab widget which contains SettingPanes and seach bar.
 *      SettingsPane - Contains a scroll area used to display SettingsTabs.
 *          SettingsTab - Contains a SettingsHeader(the thing you click on to expand the tab) and a container
 *                        for all settings under that category.
 */
class SettingBar : public QWidget {
    Q_OBJECT
  public:
    //! \brief Constructor.
    explicit SettingBar(QHash<QString, QString> selectedSettingBases);

    //! \brief Destructor.  Needs to clean dependency information from individual
    //! setting rows before general destruction happens.  Otherwise, errors can be
    //! thrown due to multiple free's/Qt's parent-child system.
    ~SettingBar();

    QVector<SettingPane*> getPanes();

    //! \brief Shows a hidden setting after a user clicked on the appropriate menu option
    //! \param panel Panel that setting is contained in
    //! \param category Setting header to add
    void showHiddenSetting(QString panel, QString category);

    //! \brief Hides a setting based on previously selected preferences
    //! \param panel Panel that setting is contained in
    //! \param category Setting header to add
    void hideSetting(QString panel, QString category);

    //! \brief Sets current folder additional settings are loaded from for informational purposes
    //! \param path Path of current folder
    void setCurrentFolder(QString path);

  signals:
    /*!
     * \brief Signals that a setting has been modified.
     * \param setting_key   Setting that has been modified.
     */
    void settingModified(QString setting_key);

    //! \brief Signal for main window to notify that a setting tab has been hidden
    //! \param pane Pane that setting is contained in
    //! \param category Setting header to add
    void tabHidden(QString pane, QString category);

  public slots:
    /*!
     * \brief Hides all settings that do not match the filter.
     * \param str   String to filter.
     */
    void filter(QString str);

    //!\brief Currently selected settings bases as provided
    //! \param settings_bases: List of currently selected ranges
    void settingsBasesSelected(QPair<QString, QList<QSharedPointer<SettingsBase>>> name_and_bases);

    //! \brief Closes all tabs.
    void closeAll();

    //! \brief Opens all tabs.
    void openAll();

    /*!
     * \brief Locks the bar, disallowing input or settings changes.
     * \param status    If true, the input is locked. If false, the input is unlocked.
     * \param category  Category to lock.
     */
    void setLock(bool status, QString category);

    /*!
     * \brief Locks the bar, disallowing input or settings changes.
     * \param status    If true, the input is locked. If false, the input is unlocked.
     */
    void setLock(bool status);

    //! \brief Reloads the settings to match updated settings
    void reloadDisplayedList();
    void updateDisplayedLists(int index);

    void updateSettings(QString text);

    void displayNewSetting(QStringList settingCategories, QString settingFile);

    //! \brief Forwards tab hide signal to main window to adjust menu options
    //! \param pane Pane that setting is contained in
    //! \param category Setting header to add
    void forwardHideTab(QString pane, QString category);

    //! \brief Places a warning icon on a tab if there's a setting warning from any children
    //! \param count: Integer representing the number of warnings, should be a positive integer or zero.
    //! \param pane: The parent pane of the relevant tab
    void barTabWarning(int count, QString pane);

    //! \brief sets the style of the widget according to current theme
    void setupStyle();

  private slots:
    /*!
     * \brief Re-emitts a signal that a setting has been modified.
     * \param setting_key   Key that was modified.
     */
    void forwardModifiedSetting(QString setting_key);

  private:
    /*!
     * \brief Returns the pane based on the major category.
     * \param major Category.
     * \return Pane pointer.
     */
    SettingPane* getPane(QString major);

    /*!
     * \brief Retuns the tab based on the major/minor category.
     * \param major Category.
     * \param minor Group.
     * \return Tab pointer.
     */
    SettingTab* getTab(QString major, QString minor);

    //! \brief Setup the static widgets and their layouts.
    void setupWidget();

    //! \brief 1. Setup the widgets.
    void setupSubWidgets();
    //! \brief 2. Setup the positions of the widgets.
    void setupLayouts();
    //! \brief 3. Setup the insertion into the layouts.
    void setupInsert();
    //! \brief 4. Setup the SettingsTabs.
    void setupGlobalSettings();
    //! \brief 5. Setup the events for the various widgets.
    void setupEvents();

    //! \brief Enables appropriate rows after all settings have been loaded
    void enableDependRows();

    //! \brief Creates dependency node information for each setting row
    //! \param master Copy of master json to pull appropriate information from
    //! \param row Pointer to row to assign appropriate information to
    //! \param json Local dependency information to parse for row
    DependencyNode createNodes(fifojson& master, QSharedPointer<SettingRowBase> row, fifojson& json);

    //! \brief Template names.
    QStringList m_templates;

    //! \brief Layouts
    QVBoxLayout* m_layout;

    //! \brief Color of emphasized text
    QString m_accentColor;

    // Widgets
    QLineEdit* m_filter_bar;
    QTabWidget* m_tab_widget;
    QComboBox* m_combo_box;
    QLabel* m_current_folder;
    QLabel* m_current_editing;

    // Combobox History
    QHash<QString, QString> mostRecentSetting;
    QHash<int, QString> paneMapping;

    //! \brief Setting Panes
    QHash<QString, SettingPane*> m_panes;

    //! \brief Whether or not ranges are currently selected
    bool m_range_selected;
};
} // Namespace ORNL
