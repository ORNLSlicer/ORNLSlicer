#pragma once

#include <QInputDialog>
#include <QScrollArea>
#include <QToolButton>
#include <QWidget>
#include <qboxlayout.h>
#include <qcontainerfwd.h>
#include <qhash.h>
#include <qtmetamacros.h>

#include "widgets/settings/setting_tab.h"

namespace ORNL {
/*!
 * \class SettingPane
 * \brief Pane that contains a scroll area to display SettingTabs.
 * \todo This class is a hold over from a previous implementation and can be removed.
 *
 * The relationship between the the various settings widgets is as follows:
 * SettingsBar - Main container for all widgets. Has a tab widget which contains SettingPanes and seach bar.
 *      SettingsPane - Contains a scroll area used to display SettingsTabs.
 *          SettingsTab - Contains a SettingsHeader(the thing you click on to expand the tab) and a container
 *                        for all settings under that category.
 */
class SettingPane : public QWidget {
    Q_OBJECT
  public:
    /*!
     * \brief Constructor.
     * \param idx       Index of this pane in SettingsBar tab widget.
     * \param parent    The parent of this object (for QObject).
     * \param pane      The name of the pane this object represents
     */
    explicit SettingPane(int idx, QWidget* parent = nullptr, QString pane = "", int warnings = 0);

    //! \brief Get a specific tab on this pane.
    SettingTab* getTab(QString category);
    //! \brief Get all tabs on this pane.
    QVector<SettingTab*> getTabs();
    //! \brief Create a new tab.
    SettingTab* newTab(QString category, QIcon icon, bool isHidden);
    //! \brief Get index of this tab in the bar's tab widget.
    int getIndex();

    //! \brief Integer to count the number of warnings coming from setting rows in a pane
    int m_pane_warning;

    //! \brief Show tab located in pane
    //! \param category Header category to show
    void showTab(QString category);

  public slots:
    //! \brief Lock all tabs on this pane.
    void setLock(bool status);
    //! \brief Reloads all settings on this pane.
    void reload();

    //! \brief Hide tab located in pane
    //! \param category Header category to hide
    void hideTab(QString category);

    //! \brief Slot to forward setting warning to next level
    //! \param count: The number of warnings forwarded from the pane's children
    void paneWarning(int count);

  private slots:
    /*!
     * \brief Forwards a modified setting to the bar for retransmission.
     * \param setting_key   Key that was modified.
     */
    void forwardModifiedSetting(QString setting_key);

  signals:
    /*!
     * \brief Signal that setting was modified.
     * \param setting_key   Key that was modified.
     */
    void settingModified(QString setting_key);

    //! \brief Forward hide signal to settingbar to eventual use by mainwindow for menus
    //! \param name Name of current pane
    //! \param category Header category
    void forwardHideTab(QString name, QString category);

    //! \brief Signal to forward setting warning to next level
    //! \param warn: Bool (0 or 1) to indicate a warning is present
    //! \param pane: String representing the name of the pane that is emitting the signal
    void warnSettingBar(bool warn, QString pane);

  private:
    //! \brief Update the configurations listed in the selection box.
    void updateConfigBox();

    //! \brief Setup the static widgets and their layouts.
    void setupWidget();

    //! \brief 1. Setup the widgets.
    void setupSubWidgets();
    //! \brief 2. Setup the positions of the widgets.
    void setupLayouts();
    //! \brief 3. Setup the insertion into the layouts.
    void setupInsert();
    //! \brief 4. Setup the events for the various widgets.
    void setupEvents();

    // Layout
    QVBoxLayout* m_layout;
    QVBoxLayout* m_scroll_layout;

    // Widgets
    QWidget* m_scroll_container;
    QScrollArea* m_scroll_area;

    //! \brief SettingsTabs (category to tab)
    QHash<QString, SettingTab*> m_tabs;

    //! \brief Index of this pane.
    int m_idx;

    //! \brief Name of this pane
    QString m_name;
};
} // Namespace ORNL
