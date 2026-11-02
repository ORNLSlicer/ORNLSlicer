#include <QApplication>
#include <QDir>
#include <QSpinBox>
#include <QStandardPaths>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "widgets/settings/setting_bar.h"
#include "widgets/settings/vector3_input_widget.h"

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
    auto global = ORNL::GSM->getGlobal();
    global->setSetting("syntax", 31);
    global->setSetting("slicing_mode", 0);
    global->setSetting("cylindrical_path_pattern", 1);
    global->setSetting("helical_perimeter_revolutions", 0);
    global->setSetting("helical_inset_revolutions", 0);
    for (const QString& region : {"perimeter", "inset", "infill"}) global->setSetting(region, true);
    auto preferences = ORNL::PreferencesManager::getInstance();
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kGrey);
    ORNL::SettingBar bar({});
    QMap<QString, ORNL::SettingTab*> tabs;
    for (auto* pane : bar.getPanes())
        for (auto* tab : pane->getTabs()) tabs.insert(tab->getName(), tab);
    auto& master               = ORNL::GSM->getMaster()->json();
    const QStringList sections = {"Perimeter", "Inset", "Infill", "Travel"};
    const auto row             = [&](const QString& key) {
        return tabs.value(QString::fromStdString(master.at(key.toStdString()).at("minor").get<std::string>()))
            ->getRow(key);
    };
    for (auto& entry : master.items()) {
        const QString key = QString::fromStdString(entry.key());
        for (const auto& section : sections) {
            if (!key.startsWith("helical_" + section.toLower() + "_")) continue;
            expect(entry.value().at("minor") == section.toStdString(), key + " must be in " + section);
            if (key.endsWith("tool_frame_x_rotation")) {
                auto grouped = row(key);
                expect(dynamic_cast<ORNL::Vector3InputWidget*>(grouped.get()) != nullptr,
                       key + " must remain a grouped vector control.");
                expect(grouped == row(QString(key).replace("_x_", "_y_")) &&
                           grouped == row(QString(key).replace("_x_", "_z_")),
                       key + " must share one row with its Y and Z components.");
            }
        }
    }
    const auto set = [&](const QString& key, int value) {
        auto* combo = dynamic_cast<QComboBox*>(row(key).get());
        expect(combo != nullptr, key + " must be a mode selector.");
        if (combo == nullptr) return;
        combo->setCurrentIndex(value);
        expect(QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, value)),
               key + " must notify its dependent rows.");
    };
    const auto check = [&](bool planar, bool helical) {
        for (auto& entry : master.items()) {
            const QString key     = QString::fromStdString(entry.key());
            const QString section = QString::fromStdString(entry.value().at("minor").get<std::string>());
            bool regional_helical = false;
            for (const auto& name : sections) regional_helical |= key.startsWith("helical_" + name.toLower() + "_");
            if (regional_helical) {
                if (!helical) expect(!row(key)->isShown(), key + " must hide outside Helical.");
            }
            else if (sections.contains(section) && !planar &&
                     (section != "Travel" ||
                      entry.value().at("depends").dump().find("slicing_mode") != std::string::npos)) {
                expect(!row(key)->isShown(), key + " must hide outside Planar.");
            }
        }
        for (const QString& section : {"Perimeter", "Inset", "Infill"}) {
            expect(tabs.value(section)->hasShownRows() == (planar || helical), section + " section visibility.");
            expect(tabs.value(section)->isHidden() == !(planar || helical), section + " header visibility.");
        }
        for (const QString& key :
             {"travel_speed", "min_travel_length", "minimum_travel_for_lift", "travel_lift_height"})
            expect(row(key)->isShown(), key + " must remain available in both modes.");
    };
    const auto snapshot = [&](const QString& state) {
        if (argc < 2) return;
        auto* pane_tabs = bar.findChild<QTabWidget*>();
        for (int i = 0; i < pane_tabs->count(); ++i)
            if (pane_tabs->tabText(i) == "Profile") pane_tabs->setCurrentIndex(i);
        bar.resize(850, 1000);
        bar.show();
        for (const auto& section : sections) {
            for (auto* tab : tabs) tab->hide();
            tabs.value(section)->show();
            tabs.value(section)->expandTab();
            app.processEvents();
            expect(bar.grab().save(QDir(QString::fromLocal8Bit(argv[1])).filePath(state + "-" + section + ".png")),
                   "Could not save " + state + " " + section + " screenshot.");
        }
        bar.restoreSettingValue("slicing_mode");
    };

    check(true, false);
    snapshot("planar");
    set("slicing_mode", 1);
    check(false, true);
    snapshot("helical-zero");
    expect(row("helical_perimeter_revolutions")->isShown(), "Zero perimeter count must remain editable.");
    expect(row("helical_inset_revolutions")->isShown(), "Zero inset count must remain editable.");
    expect(row("helical_perimeter_stepover")->isShown(), "Grey preference keeps disabled Helical controls visible.");
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kHide);
    check(false, true);
    expect(!row("helical_perimeter_stepover")->isShown(), "Hide preference hides zero-count Helical controls.");
    expect(!row("helical_inset_tool_frame_x_rotation")->isShown(), "Hide preference applies to grouped rotation.");
    dynamic_cast<QSpinBox*>(row("helical_perimeter_revolutions").get())->setValue(1);
    dynamic_cast<QSpinBox*>(row("helical_inset_revolutions").get())->setValue(1);
    expect(row("helical_perimeter_stepover")->isShown(), "Positive perimeter count reveals stepover.");
    expect(row("helical_inset_tool_frame_x_rotation")->isShown(), "Positive inset count reveals grouped rotation.");
    snapshot("helical-positive");
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kGrey);
    auto* filter = bar.findChild<QLineEdit*>();
    filter->setText("Perimeter");
    expect(!row("perimeter_speed")->isShown(), "Filtering must not reveal inactive Planar controls.");
    expect(row("helical_perimeter_revolutions")->isShown(), "Filtering must preserve active Helical controls.");
    filter->clear();
    check(false, true);
    set("cylindrical_path_pattern", 0);
    check(false, false);
    set("slicing_mode", 2);
    check(false, false);
    set("slicing_mode", 0);
    check(true, false);
    snapshot("planar-restored");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
