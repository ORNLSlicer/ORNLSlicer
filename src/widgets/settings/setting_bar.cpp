#include "widgets/settings/setting_bar.h"

#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"

namespace ORNL {

SettingBar::SettingBar(QHash<QString, QString> selectedSettingBases)
    : QWidget(nullptr), mostRecentSetting(selectedSettingBases), m_range_selected(false) {
    this->setupWidget();
}

SettingBar::~SettingBar() {
    for (SettingPane* cur_pane : m_panes) {
        for (SettingTab* cur_tab : cur_pane->getTabs()) {
            for (QSharedPointer<SettingRowBase> cur_row : cur_tab->getRows())
                cur_row->clearDependencyLogic();
        }
    }
}

QVector<SettingPane*> SettingBar::getPanes() { return m_panes.values().toVector(); }

SettingPane* SettingBar::getPane(QString major) {
    if (m_panes.contains(major))
        return m_panes[major];

    m_panes[major] = new SettingPane(m_tab_widget->count(), this, major);
    paneMapping[m_tab_widget->count()] = major;
    m_tab_widget->addTab(m_panes[major], major);

    connect(PreferencesManager::getInstance().get(), &PreferencesManager::anyUnitChanged, m_panes[major],
            &SettingPane::reload);
    connect(m_panes[major], &SettingPane::settingModified, this, &SettingBar::forwardModifiedSetting);
    connect(m_panes[major], &SettingPane::forwardHideTab, this, &SettingBar::forwardHideTab);
    connect(m_panes[major], &SettingPane::warnSettingBar, this, &SettingBar::barTabWarning);

    return m_panes[major];
}

void SettingBar::barTabWarning(int count, QString pane) {
    if (count > 0) {
        m_tab_widget->setTabIcon(m_tab_widget->indexOf(m_panes[pane]), QIcon(":/icons/warning.png"));
        m_tab_widget->setIconSize(QSize(16, 16));
    }
    else {
        m_tab_widget->setTabIcon(m_tab_widget->indexOf(m_panes[pane]), QIcon(""));
    }
}

SettingTab* SettingBar::getTab(QString major, QString minor) {
    SettingTab* tab = this->getPane(major)->getTab(minor);
    if (tab == nullptr)
        tab = this->getPane(major)->newTab(minor, QIcon(":/icons/ornlslicer_logo.png"),
                                           PreferencesManager::getInstance()->isSettingHidden(major, minor));

    return tab;
}

void SettingBar::filter(QString str) {
    // If the string is empty, ensure all tabs can be seen.
    if (str.isEmpty()) {
        for (SettingPane* cur_pane : m_panes) {
            for (SettingTab* cur_tab : cur_pane->getTabs()) {
                for (QSharedPointer<SettingRowBase> cur_row : cur_tab->getRows())
                    cur_row->show();
                cur_tab->show();
                cur_tab->shrinkTab();
            }
            m_tab_widget->setTabEnabled(cur_pane->getIndex(), true);
        }

        return;
    }

    for (SettingPane* cur_pane : m_panes) {
        bool major_hidden = true;
        for (SettingTab* cur_tab : cur_pane->getTabs()) {
            bool minor_hidden = true;
            for (QSharedPointer<SettingRowBase> cur_row : cur_tab->getRows()) {
                // Check if the current row's label matches.
                if (cur_row->getLabelText().contains(str, Qt::CaseInsensitive)) {
                    cur_row->show();
                    minor_hidden = false;
                    major_hidden = false;
                }
                else
                    cur_row->hide();
            }

            // If none of the rows are shown, hide the tab.
            if (minor_hidden)
                cur_tab->hide();
            else {
                cur_tab->show();
                cur_tab->expandTab();
            }
        }

        // If major tab is empty, disable the pane.
        if (major_hidden)
            m_tab_widget->setTabEnabled(cur_pane->getIndex(), false);
        else
            m_tab_widget->setTabEnabled(cur_pane->getIndex(), true);
    }
}

void SettingBar::settingsBasesSelected(QPair<QString, QList<QSharedPointer<SettingsBase>>> name_and_bases) {
    auto settings_bases = name_and_bases.second;

    // here we clear any warnings if a new settings base has been selected, or do nothing if the settings base remains
    // the same
    for (SettingPane* cur_pane : m_panes) {
        for (SettingTab* cur_tab : cur_pane->getTabs()) {
            if (cur_tab->m_settings_bases != settings_bases) {
                cur_pane->m_pane_warning = 0;
            }
        }
    }

    this->blockSignals(true);
    QString accentColor = PreferencesManager::getInstance()->getTheme().getDotPairedColor().name();
    if (name_and_bases.first == "")
        m_current_editing->setText("Currently editing <font color=\"" % accentColor % "\">global</font> settings");
    else
        m_current_editing->setText("Currently editing <font color=\"" % accentColor % "\">" % name_and_bases.first %
                                   "</font> settings");

    // If vector is empty, ensure all tabs that aren't user hidden can be seen.
    if (settings_bases.size() == 0) {
        m_range_selected = false;
        for (QString key : m_panes.keys()) {
            QList<QString> hiddenSettings = PreferencesManager::getInstance()->getHiddenSettings(key);
            SettingPane* cur_pane = m_panes.value(key);

            for (SettingTab* cur_tab : cur_pane->getTabs()) {
                cur_tab->settingsBasesSelected(settings_bases);
                if (!hiddenSettings.contains(cur_tab->getName())) {
                    cur_tab->show();
                    for (QSharedPointer<SettingRowBase> cur_row : cur_tab->getRows()) {
                        cur_row->show();
                    }
                }
            }
        }
    }
    else {
        m_range_selected = true;
        for (QString key : m_panes.keys()) {
            QList<QString> hiddenSettings = PreferencesManager::getInstance()->getHiddenSettings(key);
            SettingPane* cur_pane = m_panes.value(key);

            for (SettingTab* cur_tab : cur_pane->getTabs()) {
                if (!hiddenSettings.contains(cur_tab->getName())) {
                    int settingsHidden = 0;
                    QList<QSharedPointer<SettingRowBase>> rows = cur_tab->getRows();
                    for (QSharedPointer<SettingRowBase> cur_row : rows) {
                        if (!cur_row->isLocal()) {
                            cur_row->hide();
                            ++settingsHidden;
                        }
                    }

                    if (settingsHidden == rows.size())
                        cur_tab->hide();

                    cur_tab->settingsBasesSelected(settings_bases);
                }
            }
        }
    }

    // Run through and check dependencies on everthing
    for (QString key : m_panes.keys())
        for (SettingTab* cur_tab : m_panes.value(key)->getTabs())
            for (QSharedPointer<SettingRowBase> cur_row : cur_tab->getRows())
                cur_row->checkDependencies();

    this->blockSignals(false);
}

void SettingBar::closeAll() {
    for (SettingPane* cur_pane : m_panes) {
        for (SettingTab* cur_tab : cur_pane->getTabs()) {
            cur_tab->shrinkTab();
        }
    }
}

void SettingBar::openAll() {
    for (SettingPane* cur_pane : m_panes) {
        for (SettingTab* cur_tab : cur_pane->getTabs()) {
            cur_tab->expandTab();
        }
    }
}

void SettingBar::setLock(bool status, QString category) { m_panes[category]->setLock(status); }

void SettingBar::setLock(bool status) {
    for (SettingPane* cur_pane : m_panes) {
        cur_pane->setLock(status);
    }
}

void SettingBar::reloadDisplayedList() { updateDisplayedLists(m_tab_widget->currentIndex()); }

// change tab index
void SettingBar::updateDisplayedLists(int index) {
    // changing tabs so need to update combo box, disable events while doing so
    const QSignalBlocker blocker(m_combo_box);
    m_combo_box->clear();

    m_combo_box->addItems(GSM->getAllGlobals()[paneMapping[index]].keys());
    int listIndex = m_combo_box->findText(mostRecentSetting[paneMapping[index]]);
    if (listIndex != -1)
        m_combo_box->setCurrentIndex(listIndex);
    else
        m_combo_box->setCurrentIndex(0);
}

// change selected settings base
void SettingBar::updateSettings(QString text) {
    if (text != "") {
        GSM->removeCurrentSettings(paneMapping[m_tab_widget->currentIndex()],
                                   mostRecentSetting[paneMapping[m_tab_widget->currentIndex()]]);
        GSM->constructActiveGlobal(paneMapping[m_tab_widget->currentIndex()], text);
        for (SettingTab* tab : m_panes[paneMapping[m_tab_widget->currentIndex()]]->getTabs()) {
            tab->setSettingBase(GSM->getGlobal());
            tab->reload();
        }
        mostRecentSetting[paneMapping[m_tab_widget->currentIndex()]] = text;
        CSM->setMostRecentSettingHistory(paneMapping[m_tab_widget->currentIndex()], text);
        enableDependRows();
    }
}

/**
 * \brief Updates listed settings and sets UI to newly loaded base.
 *
 * Typical usage:
 * \code
 *   fooBar(categories, "filename");
 * \endcode
 *
 * \param settingCategories - categories to update (typically three: printer, material, profile.
 * \param settingFile - filename of newly loaded base.  Will set this base after UI update.
 */
void SettingBar::displayNewSetting(QStringList settingCategories, QString settingFile) {
    // Must preform a few related steps.  First, update the most recent for each category
    // so that the appropriate base is loaded when swapping tabs.  Then, reload the
    // currently displayed list choices.  Finally, set the actual values and reload.
    for (QString category : settingCategories)
        mostRecentSetting[category] = settingFile;

    reloadDisplayedList();

    for (SettingPane* cur_pane : m_panes) {
        for (SettingTab* cur_tab : cur_pane->getTabs()) {
            cur_tab->setSettingBase(GSM->getGlobal());
            cur_tab->reload();
        }
    }
    enableDependRows();
}

void SettingBar::forwardModifiedSetting(QString setting_key) { emit settingModified(setting_key); }

void SettingBar::forwardHideTab(QString pane, QString category) { emit tabHidden(pane, category); }

void SettingBar::showHiddenSetting(QString panel, QString category) {
    if (m_range_selected) {
        SettingTab* cur_tab = m_panes[panel]->getTab(category);
        int settingsHidden = 0;
        QList<QSharedPointer<SettingRowBase>> rows = cur_tab->getRows();
        for (QSharedPointer<SettingRowBase> cur_row : rows) {
            if (!cur_row->isLocal()) {
                cur_row->hide();
                ++settingsHidden;
            }
        }

        if (settingsHidden < rows.size())
            cur_tab->show();
    }
    else
        m_panes[panel]->showTab(category);
}

void SettingBar::hideSetting(QString panel, QString category) { m_panes[panel]->hideTab(category); }

void SettingBar::setCurrentFolder(QString path) {
    m_current_folder->setText("Currently searching in: " % path % " for additional setting files");
}

void SettingBar::setupStyle() {
    m_accentColor = PreferencesManager::getInstance()->getTheme().getDotPairedColor().name();
    m_current_editing->setText("Currently editing <font color=\"" % m_accentColor % "\">global</font> settings");
    for (SettingPane* cur_pane : m_panes) {
        for (SettingTab* cur_tab : cur_pane->getTabs()) {
            cur_tab->setupStyle();
        }
    }
    this->update();
}

void SettingBar::setupWidget() {
    this->setupSubWidgets();
    this->setupLayouts();
    this->setupInsert();
    this->setupGlobalSettings();
    this->setupEvents();
}

void SettingBar::setupSubWidgets() {
    m_filter_bar = new QLineEdit(this);
    m_filter_bar->setPlaceholderText("Search for setting...");

    m_tab_widget = new QTabWidget(this);
    m_tab_widget->setMovable(true);

    m_combo_box = new QComboBox(this);

    m_current_folder = new QLabel(this);
    m_current_folder->setText("Currently searching in : <Not Set> for additional setting files");

    m_accentColor = PreferencesManager::getInstance()->getTheme().getDotPairedColor().name();
    m_current_editing = new QLabel(this);
    m_current_editing->setText("Currently editing <font color=\"" % m_accentColor % "\">global</font> settings");

    QString style = "QLabel{font-weight:400; font-size: 10pt;}";
    m_current_editing->setStyleSheet(style);
}

void SettingBar::setupLayouts() { m_layout = new QVBoxLayout(this); }

void SettingBar::setupInsert() {
    this->setLayout(m_layout);

    m_layout->addWidget(m_current_editing);
    m_layout->addWidget(m_tab_widget, 1);
    m_layout->addWidget(m_filter_bar, 0);
    m_layout->addWidget(m_combo_box, 1);
    m_layout->addWidget(m_current_folder);
}

void SettingBar::setupGlobalSettings() {
    if (GSM->getMaster() == nullptr)
        return;
    fifojson master_json = GSM->getMaster()->json();

    for (auto& it : master_json.items()) {
        fifojson curr_json = it.value();
        SettingTab* curr_tab = this->getTab(curr_json[Constants::Settings::Master::kMajor],
                                            curr_json[Constants::Settings::Master::kMinor]);
        QString key = QString::fromStdString(it.key());

        curr_tab->addRow(key, curr_json);
    }
    enableDependRows();

    m_combo_box->addItems(GSM->getAllGlobals()[paneMapping[0]].keys());

    // should always be found and should already be loaded by virtue of the GSM creation, else default values should
    // have been loaded and nothing selected
    if (mostRecentSetting.contains(paneMapping[0]))
        m_combo_box->setCurrentIndex(m_combo_box->findText(mostRecentSetting[paneMapping[0]]));
}

void SettingBar::setupEvents() {
    connect(m_filter_bar, &QLineEdit::textChanged, this, &SettingBar::filter);
    connect(m_tab_widget, &QTabWidget::currentChanged, this, &SettingBar::updateDisplayedLists);
    connect(m_combo_box, QOverload<const QString&>::of(&QComboBox::currentTextChanged), this,
            &SettingBar::updateSettings);
}

// this function iterates through each tab of each pane, and make those rows enabled/disabled
// based on the variable those rows depend on
void SettingBar::enableDependRows() {
    fifojson master_json = GSM->getMaster()->json();

    for (SettingPane* curr_pane : m_panes) {
        for (SettingTab* curr_tab : curr_pane->getTabs()) {

            for (QSharedPointer<SettingRowBase> row : curr_tab->getRows()) {
                fifojson depends = row->getDependencies();
                if (depends != "") {
                    DependencyNode root = createNodes(master_json, row, depends);
                    row->setDependencyLogic(root);
                    row->checkDependencies();
                }
            }
        }
    }
}

DependencyNode SettingBar::createNodes(fifojson& master, QSharedPointer<SettingRowBase> row, fifojson& json) {
    DependencyNode root;
    for (auto& el : json.items()) {
        root.key = QString::fromStdString(el.key());
        if (el.key() == "AND" || el.key() == "OR" || el.key() == "NOT") {
            for (auto& el2 : el.value().items()) {
                root.children.push_back(createNodes(master, row, el2.value()));
            }
        }
        else {
            root.val = el.value();

            fifojson master_entry = master.at(el.key());

            QSharedPointer<SettingRowBase> parentRow =
                getPane(master_entry.at(Constants::Settings::Master::kMajor))
                    ->getTab(master_entry.at(Constants::Settings::Master::kMinor))
                    ->getRow(QString::fromStdString(el.key()));

            root.dependentRow = parentRow;
            parentRow->addRowToNotify(row);
        }
    }
    return root;
}
} // Namespace ORNL
