#include <QApplication>
#include <QDir>
#include <QSpinBox>
#include <QStandardPaths>
#include <array>
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
    const auto expect = [&passed](bool condition, const QString& message) {
        passed &= ORNL::Testing::expect(condition, message.toStdString());
    };
    const std::array<QString, 4> regions = {"perimeter", "inset", "infill", "skeleton"};
    auto global                          = ORNL::GSM->getGlobal();
    global->setSetting("syntax", 31);
    global->setSetting("slicing_mode", 0);
    global->setSetting("cylindrical_path_pattern", 1);
    global->setSetting("helical_perimeter_revolutions", 0);
    global->setSetting("helical_inset_revolutions", 0);
    for (const auto& region : regions) global->setSetting(region, true);
    auto preferences = ORNL::PreferencesManager::getInstance();
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kGrey);
    ORNL::SettingBar bar({});
    ORNL::SettingTab* arc_tab = nullptr;
    for (auto* pane : bar.getPanes()) {
        for (auto* tab : pane->getTabs()) {
            if (tab->getName() == "Arc Specialties") arc_tab = tab;
        }
    }
    expect(arc_tab != nullptr, "Arc Specialties settings tab must exist.");
    if (arc_tab == nullptr) return EXIT_FAILURE;

    std::array<QList<QSharedPointer<ORNL::SettingRowBase>>, 4> rows;
    for (auto& entry : ORNL::GSM->getMaster()->json().items()) {
        const QString key = QString::fromStdString(entry.key());
        for (size_t i = 0; i < regions.size(); ++i) {
            if (key.startsWith("arc_specialties_" + regions[i] + "_")) rows[i].append(arc_tab->getRow(key));
        }
    }
    for (size_t i = 0; i < regions.size(); ++i) expect(!rows[i].isEmpty(), regions[i] + " schedule rows must exist.");
    const auto set = [&](const QString& key, auto value) {
        global->setSetting(key, value);
        bar.restoreSettingValue(key);
    };
    const auto check = [&](const std::array<bool, 4>& enabled, const QString& context) {
        for (size_t i = 0; i < regions.size(); ++i) {
            for (const auto& row : rows[i]) {
                auto* widget = dynamic_cast<QWidget*>(row.get());
                expect(widget != nullptr && widget->isEnabled() == enabled[i],
                       context + ": unexpected enablement for " + row->getLabelText());
                const bool visible = enabled[i] || preferences->getDisabledSettingVisibilityPreference() ==
                                                       ORNL::DisabledSettingVisibility::kGrey;
                expect(row->isShown() == visible, context + ": unexpected visibility for " + row->getLabelText());
            }
        }
    };
    const auto snapshot = [&](const QString& name) {
        if (argc < 2) return;
        bar.resize(650, 550);
        auto* tabs = bar.findChild<QTabWidget*>();
        for (int i = 0; i < tabs->count(); ++i)
            if (tabs->tabText(i) == "Profile") tabs->setCurrentIndex(i);
        auto* filter = bar.findChild<QLineEdit*>();
        filter->setText("Preflow");
        bar.show();
        app.processEvents();
        expect(bar.grab().save(QDir(QString::fromLocal8Bit(argv[1])).filePath(name + ".png")),
               "Could not save " + name + " screenshot.");
        filter->clear();
    };

    check({true, true, true, true}, "Planar regions enabled");
    snapshot("planar");
    for (size_t i = 0; i < regions.size(); ++i) {
        set(regions[i], false);
        std::array<bool, 4> enabled = {true, true, true, true};
        enabled[i]                  = false;
        check(enabled, "Planar region disabled");
        set(regions[i], true);
    }

    set("slicing_mode", 1);
    check({false, false, true, false}, "Helical zero shell revolutions with stale Planar toggles");
    snapshot("helical-zero");
    for (const auto& region : regions) set(region, false);
    check({false, false, true, false}, "Helical infill ignores Planar toggle");
    // Exercise live spin-box notifications rather than refreshing all dependencies.
    const auto spinBox = [&](const QString& key) -> QSpinBox* {
        for (auto* pane : bar.getPanes())
            for (auto* tab : pane->getTabs())
                if (auto* spin = dynamic_cast<QSpinBox*>(tab->getRow(key).get())) return spin;
        return nullptr;
    };
    auto* perimeter = spinBox("helical_perimeter_revolutions");
    auto* inset     = spinBox("helical_inset_revolutions");
    expect(perimeter != nullptr && inset != nullptr, "Helical revolution controls must exist.");
    if (perimeter == nullptr || inset == nullptr) return EXIT_FAILURE;
    perimeter->setValue(1);
    check({true, false, true, false}, "Helical perimeter revolutions");
    inset->setValue(2);
    check({true, true, true, false}, "Helical inset revolutions");
    snapshot("helical-positive");
    perimeter->setValue(0);
    check({false, true, true, false}, "Helical perimeter reset to zero");
    inset->setValue(0);

    set("cylindrical_path_pattern", 0);
    check({true, false, false, false}, "Radial perimeter ignores Planar toggle");
    snapshot("radial");
    for (const auto& region : regions) set(region, true);
    check({true, false, false, false}, "Radial ignores stale Planar toggles");
    set("slicing_mode", 2);
    check({false, false, false, false}, "Image slicing");
    set("slicing_mode", 0);
    check({true, true, true, true}, "Return to Planar");
    set("syntax", 0);
    check({false, false, false, false}, "Other syntax in Planar");
    set("slicing_mode", 1);
    set("cylindrical_path_pattern", 1);
    set("helical_perimeter_revolutions", 1);
    set("helical_inset_revolutions", 1);
    check({false, false, false, false}, "Other syntax in Helical");
    set("syntax", 31);
    preferences->setDisabledSettingVisibilityPreference(ORNL::DisabledSettingVisibility::kHide);
    check({true, true, true, false}, "Hide disabled Helical settings");
    set("helical_perimeter_revolutions", 0);
    set("helical_inset_revolutions", 0);
    check({false, false, true, false}, "Hide zero-revolution shell settings");
    set("cylindrical_path_pattern", 0);
    check({true, false, false, false}, "Hide disabled radial settings");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
