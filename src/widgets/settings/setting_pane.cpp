#include "widgets/settings/setting_pane.h"

#include <qboxlayout.h>
#include <qcontainerfwd.h>
#include <qicon.h>
#include <qinputdialog.h>
#include <qminmax.h>
#include <qnamespace.h>
#include <qscrollarea.h>
#include <qsharedpointer.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include "managers/settings/settings_manager.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_tab.h"

namespace ORNL {
SettingPane::SettingPane(int idx, QWidget* parent, QString pane, int warnings)
    : QWidget(parent), m_idx(idx), m_name(pane), m_pane_warning(warnings) {
    this->setupWidget();
}

SettingTab* SettingPane::getTab(QString category) {
    if (!m_tabs.contains(category))
        return nullptr;
    return m_tabs[category];
}

QVector<SettingTab*> SettingPane::getTabs() { return m_tabs.values().toVector(); }

SettingTab* SettingPane::newTab(QString category, QIcon icon, bool isHidden) {
    QSharedPointer<SettingsBase> sb = GSM->getGlobal();
    // int currentIndex = m_tabs.size() - 1;

    m_tabs[category] = new SettingTab(this, category, icon, m_tabs.size(), isHidden, GSM->getGlobal());
    // -1 to insert before stretch.
    if (!isHidden)
        m_scroll_layout->insertWidget(m_scroll_layout->count() - 1, m_tabs[category]);

    connect(m_tabs[category], &SettingTab::modified, this, &SettingPane::forwardModifiedSetting);
    connect(m_tabs[category], &SettingTab::removeTabFromList, this, &SettingPane::hideTab);
    connect(m_tabs[category], &SettingTab::warnPane, this, &SettingPane::paneWarning);

    return m_tabs[category];
}

int SettingPane::getIndex() { return m_idx; }

void SettingPane::setLock(bool status) {
    for (SettingTab* curr_tab : m_tabs) {
        for (QSharedPointer<SettingRowBase> cur_row : curr_tab->getRows()) {
            cur_row->setEnabled(status);
        }
    }
}

void SettingPane::reload() {
    for (SettingTab* curr_tab : m_tabs) {
        curr_tab->reload();
    }
}

void SettingPane::hideTab(QString category) {
    m_scroll_layout->removeWidget(m_tabs[category]);
    emit forwardHideTab(m_name, category);
}

void SettingPane::showTab(QString category) {
    int maxIndex = qMin(m_scroll_layout->count() - 1, m_tabs[category]->getIndex());
    m_scroll_layout->insertWidget(maxIndex, m_tabs[category]);
    m_tabs[category]->showTab();
}

void SettingPane::paneWarning(int count) {
    m_pane_warning = m_pane_warning + count; // keep track of total number of warnings from all children (tabs)
    if (m_pane_warning > 0) {
        emit warnSettingBar(1, m_name);
    }
    else {
        emit warnSettingBar(0, m_name);
    }
}

void SettingPane::forwardModifiedSetting(QString setting_key) { emit settingModified(setting_key); }

void SettingPane::setupWidget() {
    this->setupSubWidgets();
    this->setupLayouts();
    this->setupInsert();
}

void SettingPane::setupSubWidgets() {
    // ScrollArea
    m_scroll_area = new QScrollArea(this);
    m_scroll_area->setStyleSheet("QScrollArea { background: transparent; border:0px;}\
                            QScrollArea > QWidget > QWidget { background: transparent; }");
    m_scroll_area->setWidgetResizable(true);
    m_scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Scroll Container
    m_scroll_container = new QWidget(this);
}

void SettingPane::setupLayouts() {
    // Layout
    m_layout = new QVBoxLayout(this);
    m_layout->setStretch(0, 1);

    // Layout in scrollarea.
    m_scroll_layout = new QVBoxLayout(m_scroll_container);
    m_scroll_layout->addStretch();
    m_scroll_layout->setContentsMargins(0, 0, 0, 0);
}

void SettingPane::setupInsert() {
    m_layout->addWidget(m_scroll_area);

    m_scroll_area->setWidget(m_scroll_container);
}

} // Namespace ORNL
