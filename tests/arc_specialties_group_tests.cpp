#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QGroupBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "widgets/settings/setting_bar.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);
    bool passed       = true;
    const auto expect = [&](bool condition, const QString& message) {
        passed &= ORNL::Testing::expect(condition, message.toStdString());
    };
    const QStringList regions  = {"Perimeter", "Inset", "Infill", "Skeleton"};
    const QStringList sections = {"Gas Timing", "Startup", "Main Weld", "Crater Fill"};
    auto global                = ORNL::GSM->getGlobal();
    global->setSetting("syntax", 31);
    global->setSetting("slicing_mode", 0);
    for (const auto& region : regions) global->setSetting(region.toLower(), true);
    auto preferences = ORNL::PreferencesManager::getInstance();
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kGrey);
    preferences->setVelocityUnit(ORNL::mm / ORNL::s);
    preferences->setTimeUnit(ORNL::s);
    preferences->setVoltageUnit(ORNL::V);
    ORNL::SettingBar bar({});
    ORNL::SettingTab* arc = nullptr;
    for (auto* pane : bar.getPanes())
        for (auto* tab : pane->getTabs())
            if (tab->getName() == "Arc Specialties") arc = tab;
    expect(arc != nullptr, "ARC settings must exist.");
    if (arc == nullptr) return EXIT_FAILURE;
    auto* pane_tabs = bar.findChild<QTabWidget*>();
    for (int i = 0; i < pane_tabs->count(); ++i)
        if (pane_tabs->tabText(i) == "Profile") pane_tabs->setCurrentIndex(i);
    auto* filter        = bar.findChild<QLineEdit*>();
    const auto snapshot = [&](const QString& name, int width) {
        bar.resize(width, 1100);
        bar.show();
        for (auto* pane : bar.getPanes())
            for (auto* tab : pane->getTabs()) tab->setVisible(tab == arc);
        arc->expandTab();
        QEventLoop layout_events;
        QTimer::singleShot(50, &layout_events, &QEventLoop::quit);
        layout_events.exec();
        if (argc < 2) return;
        expect(bar.grab().save(QDir(QString::fromLocal8Bit(argv[1])).filePath(name + ".png")),
               "Could not save " + name + " screenshot.");
    };
    filter->setText("Perimeter");
    snapshot("perimeter-wide", 950);
    snapshot("perimeter-sidebar", 650);
    filter->clear();
    snapshot("regions-collapsed", 650);

    const auto group  = [&](const QString& name) { return arc->findChild<QWidget*>("setting_group_" + name); };
    bool groups_exist = true;
    for (const auto& region : regions) {
        expect(group(region) != nullptr, region + " must have a collapsible group.");
        groups_exist &= group(region) != nullptr;
        for (const auto& section : sections) {
            const bool exists = arc->findChild<QGroupBox*>("setting_group_" + region + "/" + section) != nullptr;
            expect(exists, region + " must have a " + section + " form.");
            groups_exist &= exists;
        }
    }
    if (!groups_exist) return EXIT_FAILURE;
    auto* scroll = pane_tabs->currentWidget()->findChild<QScrollArea*>();
    expect(scroll->horizontalScrollBar()->maximum() == 0 && scroll->verticalScrollBar()->maximum() == 0,
           "Collapsed groups must fit the sidebar without stale scrollbars after layouts settle.");
    for (const auto& region : regions) {
        auto* header = arc->findChild<QToolButton*>("setting_group_toggle_" + region);
        expect(header != nullptr && group(region)->height() <= header->sizeHint().height() + 20,
               "Collapsed region headers must remain compact without stretching to fill the panel.");
    }
    expect(arc->getRows().size() == 77,
           "All 76 regional scalar settings and generation control must remain registered.");
    for (const auto& region : regions) {
        int count = 0;
        for (const auto& section : sections) {
            auto* form     = group(region + "/" + section);
            int form_count = 0;
            for (const auto& row : arc->getRows()) {
                if (form->isAncestorOf(dynamic_cast<QWidget*>(row.get()))) ++form_count;
            }
            const int expected = section == "Gas Timing"  ? 4
                                 : section == "Startup"   ? 6
                                 : section == "Main Weld" ? 4
                                                          : 5;
            expect(form_count == expected, region + " " + section + " must contain its existing settings.");
            count += form_count;
        }
        expect(count == 19, region + " must retain 19 independent settings.");
        auto* gas = qobject_cast<QGridLayout*>(group(region + "/Gas Timing")->layout());
        expect(gas != nullptr && gas->itemAtPosition(0, 0) && gas->itemAtPosition(0, 1) && gas->itemAtPosition(1, 0) &&
                   gas->itemAtPosition(1, 1),
               region + " gas timing must use a two-by-two grid.");
    }
    const QString speed_key = "arc_specialties_perimeter_start_wire_feed_speed";
    auto speed_row          = arc->getRow(speed_key);
    auto* speed             = dynamic_cast<QDoubleSpinBox*>(speed_row.get());
    auto* toggle            = arc->findChild<QToolButton*>("setting_group_toggle_Perimeter");
    auto* content           = arc->findChild<QWidget*>("setting_group_content_Perimeter");
    expect(speed != nullptr && toggle != nullptr && content != nullptr,
           "Grouped controls and collapse widgets must exist.");
    if (speed == nullptr || toggle == nullptr || content == nullptr) return EXIT_FAILURE;
    expect(speed_row->getLabelText() == "Wire Feed Speed", "Grouped labels must omit redundant region/phase prefixes.");
    expect(content->isHidden(), "Region forms start collapsed.");
    toggle->click();
    expect(!content->isHidden() && speed->isVisibleTo(arc), "Expanding a region must reveal its controls.");
    toggle->click();
    expect(content->isHidden() && speed_row->isShown(), "Collapsing must not change logical dependency visibility.");
    filter->setText("Perimeter Start Wire Feed Speed");
    expect(speed_row->isShown() && !content->isHidden(), "Full original names must find and reveal grouped controls.");
    expect(!toggle->isEnabled(), "Collapse toggles must not silently change saved state while searching.");
    toggle->click();
    expect(!group("Perimeter/Startup")->isHidden() && group("Perimeter/Gas Timing")->isHidden() &&
               group("Inset")->isHidden(),
           "Search must hide empty phases and regions.");
    filter->clear();
    expect(content->isHidden() && toggle->isEnabled(), "Clearing search must restore the user's collapsed state.");
    filter->setText("Startup");
    for (const auto& region : regions) {
        expect(!group(region)->isHidden() && !group(region + "/Startup")->isHidden() &&
                   group(region + "/Main Weld")->isHidden(),
               "Group titles must be searchable.");
    }
    filter->setText("no matching weld setting");
    for (const auto& region : regions) expect(group(region)->isHidden(), "Empty region groups must disappear.");
    filter->clear();
    toggle->click();
    filter->setText("Perimeter");
    filter->clear();
    expect(!content->isHidden(), "Search must also restore an expanded region.");

    QString changed_key;
    QString modified_key;
    QObject::connect(&bar, &ORNL::SettingBar::settingAboutToChange, &bar,
                     [&](const QString& key, const QList<QSharedPointer<ORNL::SettingsBase>>&) { changed_key = key; });
    QObject::connect(&bar, &ORNL::SettingBar::settingModified, &bar, [&](const QString& key) { modified_key = key; });
    speed->setValue(12.5);
    expect(ORNL::Testing::near(global->setting<ORNL::Velocity>(speed_key).to(ORNL::mm / ORNL::s), 12.5) &&
               changed_key == speed_key && modified_key == speed_key,
           "Grouped edits must retain their units and undo/change notifications.");
    preferences->setVelocityUnit(ORNL::inch / ORNL::minute);
    expect(ORNL::Testing::near(speed->value(), 29.53, 0.01), "Grouped speeds must reload in UI preference units.");
    speed->setValue(10);
    expect(ORNL::Testing::near(global->setting<ORNL::Velocity>(speed_key).to(ORNL::inch / ORNL::minute), 10),
           "Editing a converted grouped speed must store the physical quantity.");
    auto* preflow = dynamic_cast<QDoubleSpinBox*>(arc->getRow("arc_specialties_perimeter_preflow").get());
    preferences->setTimeUnit(ORNL::ms);
    preflow->setValue(250);
    expect(ORNL::Testing::near(global->setting<ORNL::Time>("arc_specialties_perimeter_preflow").to(ORNL::s), 0.25),
           "Grouped timing edits must use UI units.");
    auto* voltage = dynamic_cast<QDoubleSpinBox*>(arc->getRow("arc_specialties_perimeter_start_voltage").get());
    preferences->setVoltageUnit(ORNL::mV);
    voltage->setValue(17000);
    expect(
        ORNL::Testing::near(global->setting<ORNL::Voltage>("arc_specialties_perimeter_start_voltage").to(ORNL::V), 17),
        "Grouped voltage edits must use UI units.");
    const auto set = [&](const QString& key, int value) {
        global->setSetting(key, value);
        bar.restoreSettingValue(key);
    };
    set("slicing_mode", 1);
    set("cylindrical_path_pattern", 1);
    set("helical_perimeter_revolutions", 0);
    set("helical_inset_revolutions", 0);
    for (const auto& region : regions)
        expect(!group(region)->isHidden(), "Grey preference must keep disabled groups visible.");
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kHide);
    expect(group("Perimeter")->isHidden() && group("Inset")->isHidden() && !group("Infill")->isHidden() &&
               group("Skeleton")->isHidden(),
           "Helical zero-shell groups must follow hide preference.");
    set("helical_perimeter_revolutions", 1);
    expect(!group("Perimeter")->isHidden(), "Positive Helical revolutions must reveal the region group.");
    filter->setText("Perimeter");
    snapshot("helical-perimeter", 650);
    filter->clear();
    set("cylindrical_path_pattern", 0);
    expect(!group("Perimeter")->isHidden() && group("Inset")->isHidden() && group("Infill")->isHidden() &&
               group("Skeleton")->isHidden(),
           "Radial mode must expose only the enabled Perimeter group.");
    set("syntax", 0);
    for (const auto& region : regions) expect(group(region)->isHidden(), "Other syntaxes must hide all region groups.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
