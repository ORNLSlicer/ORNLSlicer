#include <QApplication>
#include <QDir>
#include <QMenu>
#include <QMenuBar>
#include <QTabWidget>
#include <QToolButton>

#include "capture_common.h"
#include "managers/preferences_manager.h"
#include "utilities/enums.h"
#include "widgets/settings/setting_bar.h"
#include "widgets/settings/setting_pane.h"
#include "widgets/settings/setting_row_base.h"
#include "widgets/settings/setting_tab.h"
#include "windows/main_window.h"

namespace ORNL {

QStringList captureAllMenuBarMenus(MainWindow* window, const QString& outputDirectory) {
    QStringList saved_paths;
    const QDir baseDir(outputDirectory);
    auto* menuBar = requireNonNull(window->menuBar(), "MainWindow::menuBar");

    for (QAction* act : menuBar->actions()) {
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
    auto* tabWidget  = requireChild<QTabWidget>(prefWindow);
    const QDir baseDir(outputDirectory);

    for (QWidget* w : prefWindow->findChildren<QWidget*>()) { w->setEnabled(true); }
    for (QAction* a : prefWindow->findChildren<QAction*>()) { a->setEnabled(true); }

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);
        QApplication::sendPostedEvents();
        auto* page = requireNonNull(tabWidget->widget(i), QString("Preferences tab %1").arg(i));
        page->adjustSize();
        const QString destFile = baseDir.filePath(QString("preferences/%1_tab.png").arg(toSlug(tabWidget->tabText(i))));
        saveWidget(page, destFile);
        saved_paths << destFile;
    }

    return saved_paths;
}

QStringList captureAllSettingsCategories(MainWindow* window, const QString& outputDirectory,
                                         const QStringList& categoryFilters) {
    QStringList saved_paths;
    PreferencesManager::getInstance()->setDisabledSettingVisibilityPreference(DisabledSettingVisibility::kGrey);

    auto* settingBar = requireChild<SettingBar>(window);
    settingBar->settingsBasesSelected({"", {}}, {});

    auto* tabWidget = requireChild<QTabWidget>(settingBar);
    const QDir baseDir(outputDirectory);

    QStringList normalizedFilters;
    for (const QString& rawFilter : categoryFilters) {
        for (const QString& part : rawFilter.split(',', Qt::SkipEmptyParts)) {
            const QStringList tokens = part.split('/', Qt::SkipEmptyParts);
            if (tokens.size() == 2) { normalizedFilters << (toSlug(tokens[0]) + "/" + toSlug(tokens[1])); }
            else if (!tokens.isEmpty()) { normalizedFilters << toSlug(tokens[0]); }
        }
    }

    auto matchesCategory = [&](const QString& majorSlug, const QString& minorSlug) {
        if (normalizedFilters.isEmpty()) { return true; }
        return normalizedFilters.contains(minorSlug) || normalizedFilters.contains(majorSlug + "/" + minorSlug);
    };

    for (int i = 0; i < tabWidget->count(); ++i) {
        tabWidget->setCurrentIndex(i);
        const QString majorSlug = toSlug(tabWidget->tabText(i));

        auto* pane = static_cast<SettingPane*>(tabWidget->widget(i));
        for (SettingTab* tab : pane->getTabs()) {
            tab->show();
            tab->expandTab();
            for (const QSharedPointer<SettingRowBase>& row : tab->getRows()) { row->show(); }
            if (toSlug(tab->getName()) == "arc_specialties") {
                if (auto* toggle = tab->findChild<QToolButton*>("setting_group_toggle_Perimeter");
                    toggle && !toggle->isChecked()) {
                    toggle->click();
                }
                tab->refreshGroupVisibility();
            }
            for (QWidget* w : tab->findChildren<QWidget*>()) { w->setEnabled(true); }
        }

        QApplication::sendPostedEvents();

        for (SettingTab* tab : pane->getTabs()) {
            const QString minorSlug = toSlug(tab->getName());
            if (!matchesCategory(majorSlug, minorSlug)) { continue; }
            tab->adjustSize();
            const QString destFile = baseDir.filePath(QString("settings/%1/%2_options.png").arg(majorSlug, minorSlug));
            saveWidget(tab, destFile);
            saved_paths << destFile;
        }
    }

    return saved_paths;
}

}  // namespace ORNL
