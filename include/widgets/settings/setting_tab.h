#pragma once

#include <functional>

#include <QFrame>
#include <QGridLayout>
#include <QWidget>
#include <qboxlayout.h>
#include <qhash.h>
#include <qlist.h>
#include <qobject.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>

#include "configs/settings_base.h"
#include "setting_header.h"
#include "utilities/qt_json_conversion.h"
#include "widgets/settings/setting_row_base.h"

namespace ORNL {
/*!
 * \class SettingTab
 * \brief Widget that contains all settings.
 *
 * The relationship between the the various settings widgets is as follows:
 * SettingsBar - Main container for all widgets. Has a tab widget which contains SettingPanes and seach bar.
 *      SettingsPane - Contains a scroll area used to display SettingsTabs.
 *          SettingsTab - Contains a SettingsHeader(the thing you click on to expand the tab) and a container
 *                        for all settings under that category.
 */
class SettingTab : public QWidget {
    Q_OBJECT

  public:
    /*!
     * \brief Constructor.
     * \param parent    Parent of this object (for QObject).
     * \param name      Name to display on the header.
     * \param icon      Icon to display on the header.
     * \param index     Index of header within scroll widget
     * \param sb        SettingsBase this tab will use.
     * \param isHidden  Whether or not SettingTab is hidden by default or not.
     */
    SettingTab(QWidget* parent = nullptr, QString name = QString(), QIcon icon = QIcon(), int index = 0,
               bool isHidden = false, QSharedPointer<SettingsBase> sb = nullptr);

    //! \brief Set the internal settingsbase.
    void setSettingBase(QSharedPointer<SettingsBase> sb);

    //! \brief Copy of settings bases, if applicable
    QList<QSharedPointer<SettingsBase>> m_settings_bases;

    //! \brief Adds a row to this tab.
    void addRow(QString key, fifojson& json);

    //! \brief Get all rows in this tab.
    QList<QSharedPointer<SettingRowBase>> getRows();

    QSharedPointer<SettingRowBase> getRow(QString key);

    //! \brief Returns index of tab to determine where to insert when showing a hidden tab.
    int getIndex();

    //! \brief Number of warnings from setting rows in a tab
    int m_warning_count;

    //! \brief Returns name of tab
    //! \return Name of tab
    QString getName();

  public slots:
    //! \brief Expand the current tab.
    void expandTab();

    //! \brief Shrink the current tab.
    void shrinkTab();

    //! \brief Update this tab by iterating through all rows and reading from sb / updating unit.
    void reload();

    //! \brief Settings bases currently selected to adjust setting
    //! \param settings_bases: settings bases to display for
    void settingsBasesSelected(QList<QSharedPointer<SettingsBase>> settings_bases);

    //! \brief Tells the header what icon to display, based on if there is a warning from any settings in the tab
    //! \param count: Total number of warnings in this tab, should be a positive integer or zero.
    void headerWarning(int count);

    //! \brief sets the style of the widget according to current theme
    void setupStyle();

    //! \brief Shows the tab
    void showTab();

    //! \brief Hides the tab
    void hideTab();

    void keyModified(QString key);

  signals:
    /*!
     * \brief Notification that a setting was modified.
     * \param key   Key that was modified.
     */
    void modified(QString key);

    //! \brief Signal SettingPane to remove current header from list after hide command
    //! \param category Name of header to remove
    void removeTabFromList(QString category);

    //! \brief Signal to forward tab warnings to the tab's parent (a setting pane).
    //! \param count: Number of warnings
    void warnPane(int count);

  private:
    //! \brief Setup the static widgets and their layouts.
    void setupWidget(bool isHidden);

    //! \brief 1. Setup the widgets.
    void setupSubWidgets(bool isHidden);
    //! \brief 2. Setup the positions of the widgets.
    void setupLayouts();
    //! \brief 3. Setup the insertion into the layouts.
    void setupInsert();
    //! \brief 4. Setup the events for the various widgets.
    void setupEvents();

    // Widgets
    SettingHeader* m_header;
    QFrame* m_container;

    // Layouts
    QVBoxLayout* m_layout;
    QGridLayout* m_container_layout;

    // Tab display.
    QIcon m_icon;
    QString m_name;
    int m_index;

    // Open and close animations.
    // QPropertyAnimation* m_open_ani;
    // QPropertyAnimation* m_close_ani;

    //! \brief Current size.
    int m_size;

    //! \brief Map of key to input row.
    QHash<QString, QSharedPointer<SettingRowBase>> m_rows;

    //! \brief Settings bases this tab uses.
    QSharedPointer<SettingsBase> m_sb;

    QHash<QString, std::function<SettingRowBase*(SettingTab*, QSharedPointer<SettingsBase>, QString, fifojson,
                                                 QGridLayout*, int)>>
        m_creation_mapping;
};
} // Namespace ORNL
