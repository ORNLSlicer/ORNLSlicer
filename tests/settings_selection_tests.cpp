#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>

#include "managers/preferences_manager.h"
#include "managers/session_manager.h"
#include "managers/settings/settings_manager.h"
#include "test_utils.h"
#include "widgets/settings/setting_bar.h"
#include "windows/preferences_window.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(configs);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(styles);

    bool passed       = true;
    const auto expect = [&passed](bool condition, const char* message) {
        passed &= ORNL::Testing::expect(condition, message);
    };
    const auto preferences = ORNL::PreferencesManager::getInstance();
    ORNL::PreferencesWindow preferences_window;
    auto* preference_tabs = preferences_window.findChild<QTabWidget*>();
    for (int index = 0; index < preference_tabs->count(); ++index)
        if (preference_tabs->tabText(index) == "Settings") preference_tabs->setCurrentIndex(index);
    preferences_window.show();
    app.processEvents();
    if (argc > 1)
        expect(preferences_window.grab().save(QString(argv[1]) + "/preferences.png"),
               "Could not save preferences screenshot.");

    auto* checkbox = preferences_window.findChild<QCheckBox*>("prompt_settings_for_all_categories");
    expect(checkbox != nullptr, "Settings preferences must offer an all-category settings-file prompt toggle.");
    if (checkbox == nullptr) return EXIT_FAILURE;
    expect(!checkbox->isChecked(), "The all-category prompt must default to disabled.");

    QTemporaryDir temp_dir;
    if (!ORNL::Testing::expect(temp_dir.isValid(), "Could not create settings fixture directory.")) return EXIT_FAILURE;
    const auto manager = ORNL::SettingsManager::getInstance();
    QFile versions(":/configs/versions.conf");
    if (!ORNL::Testing::expect(versions.open(QIODevice::ReadOnly), "Could not read settings version."))
        return EXIT_FAILURE;
    const auto version = fifojson::parse(versions.readAll().toStdString()).at("master_version");
    const QStringList categories {"Printer", "Material", "Profile", "Experimental"};
    const QStringList keys {"tool_number", "printing_material", "perimeter_count", "tormach_mode"};
    const auto load_fixture = [&](const QString& name, const QList<int>& indices, int value) {
        fifojson settings = fifojson::object();
        for (int index : indices) settings[keys[index].toStdString()] = value;
        if (name == "old") settings["individual_layer_output"] = true;
        fifojson document  = {{"header", {{"version", version}}}, {"settings", fifojson::array({settings})}};
        const QString path = temp_dir.filePath(name + ".s2c");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(document.dump().c_str());
        file.close();
        return manager->loadGlobalJson(path);
    };
    if (!ORNL::Testing::expect(load_fixture("old", {0, 1, 2, 3}, 1) && load_fixture("new", {0, 1, 2, 3}, 2) &&
                                   load_fixture("partial", {0, 2}, 3) && load_fixture("single", {1}, 4),
                               "Could not load settings fixtures."))
        return EXIT_FAILURE;

    QHash<QString, QString> initial_history;
    for (const QString& category : categories) {
        initial_history[category] = "old";
        ORNL::CSM->setMostRecentSettingHistory(category, "old");
    }
    manager->constructActiveGlobal(initial_history);
    ORNL::SettingBar bar(initial_history);
    bar.resize(650, 800);
    auto* tabs  = bar.findChild<QTabWidget*>();
    auto* combo = bar.findChild<QComboBox*>(QString(), Qt::FindDirectChildrenOnly);
    bar.show();
    const auto select_category = [&](int category_index) {
        for (int index = 0; index < tabs->count(); ++index)
            if (tabs->tabText(index) == categories[category_index]) tabs->setCurrentIndex(index);
    };
    const auto select_file = [&](const QString& name) {
        expect(combo->findText(name) >= 0, "The selected file must be available in the current category.");
        combo->setCurrentText(name);
        QMetaObject::invokeMethod(combo, "textActivated", Qt::DirectConnection, Q_ARG(QString, name));
    };
    int prompts = 0;
    auto answer = QMessageBox::No;
    QTimer responder;
    QObject::connect(&responder, &QTimer::timeout, [&] {
        if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
            ++prompts;
            expect(prompt->text().contains("Printer") && prompt->text().contains("Profile"),
                   "The prompt must identify the categories being changed.");
            if (argc > 1)
                expect(prompt->grab().save(QString(argv[1]) + QString("/prompt-%1.png").arg(prompts)),
                       "Could not save prompt.");
            prompt->button(answer)->click();
        }
    });
    responder.start(10);
    int changes = 0;
    QObject::connect(&bar, &ORNL::SettingBar::settingsBaseChanged, [&] { ++changes; });

    select_category(0);
    select_file("new");
    expect(prompts == 0 && changes == 1, "Disabled prompts must preserve a single category change.");
    expect(manager->getGlobal()->setting<int>(keys[0]) == 2 && manager->getGlobal()->setting<int>(keys[1]) == 1,
           "Changing the printer file must preserve the material file when prompts are disabled.");
    manager->getGlobal()->setSetting(keys[0], 9);
    select_file("new");
    expect(prompts == 0 && changes == 1 && manager->getGlobal()->setting<int>(keys[0]) == 9,
           "Reselecting the active file with prompts disabled must preserve manual edits.");

    checkbox->click();
    if (argc > 1)
        expect(preferences_window.grab().save(QString(argv[1]) + "/preferences-enabled.png"),
               "Could not save enabled preference.");
    select_category(2);
    select_file("new");
    expect(prompts == 1 && changes == 2, "Enabling the preference must prompt once per user selection.");
    expect(manager->getGlobal()->setting<int>(keys[2]) == 2 && manager->getGlobal()->setting<int>(keys[3]) == 1,
           "Declining must apply only the current category.");
    expect(manager->getGlobal()->setting<bool>("individual_layer_output"),
           "Declining must preserve settings omitted from the file in other categories.");
    manager->getGlobal()->setSetting(keys[2], 9);
    select_file("new");
    expect(prompts == 2 && changes == 2 && manager->getGlobal()->setting<int>(keys[2]) == 9,
           "Declining when reselecting the active file must preserve manual edits.");

    answer = QMessageBox::Yes;
    // Reselect the current file to apply it to the remaining categories.
    select_file("new");
    expect(prompts == 3 && changes == 3, "Applying all categories must emit one settings change notification.");
    for (int index = 0; index < categories.size(); ++index) {
        expect(manager->getGlobal()->setting<int>(keys[index]) == 2, "Accepting must update all four category values.");
        expect(ORNL::CSM->getMostRecentSettingHistory().value(categories[index]) == "new",
               "Accepting must persist the file selection for every category.");
        select_category(index);
        expect(combo->currentText() == "new", "Each category dropdown must display the applied file.");
    }
    expect(!manager->getGlobal()->setting<bool>("individual_layer_output"),
           "Applying all categories must reset settings omitted from the replacement file in other categories.");
    bar.reloadDisplayedList();
    bar.displayNewSetting(categories, "new");
    app.processEvents();
    expect(prompts == 3 && changes == 3, "Tab changes and programmatic list refreshes must not prompt or apply files.");

    select_category(0);
    select_file("partial");
    expect(prompts == 4, "Files containing multiple categories must support the prompt.");
    for (int index = 0; index < categories.size(); ++index) {
        const bool included = index == 0 || index == 2;
        expect(manager->getGlobal()->setting<int>(keys[index]) == (included ? 3 : 2),
               "Applying a partial file must preserve values in categories absent from the file.");
        select_category(index);
        expect(combo->currentText() == (included ? "partial" : "new"),
               "Applying a partial file must preserve selections in categories absent from the file.");
        expect(ORNL::CSM->getMostRecentSettingHistory().value(categories[index]) == (included ? "partial" : "new"),
               "Applying a partial file must persist only the categories it contains.");
    }
    select_category(1);
    select_file("single");
    expect(prompts == 4 && manager->getGlobal()->setting<int>(keys[1]) == 4,
           "A file containing only one category must apply without a redundant prompt.");
    checkbox->click();
    select_category(3);
    select_file("old");
    expect(prompts == 4 && manager->getGlobal()->setting<int>(keys[3]) == 1 &&
               manager->getGlobal()->setting<int>(keys[0]) == 3,
           "Disabling the preference must immediately restore category-only selection.");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
